#!/usr/bin/env python3
"""Offline source-linked checks for null old profiles in subscription updating.

Python 3 and a GCC/Clang C++20 compiler are required. Compiles the unchanged
production Update, AddProxy, key comparator, AddProfileBatch, BatchDeleteProfiles,
Group::RemoveProfile and Group::HasProfile with synthetic boundary adapters.
No real application, user profile, database, network, or VPN is opened.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT
REF = None
HERE = Path(__file__).resolve().parent / "subscription-null"
CASES = (
    "one_missing", "two_missing", "repeated_missing_id", "mixed", "duplicates",
    "unchanged", "repeated_groups", "recovered_lookup", "incremental_boundary",
    "automatic_clear", "explicit_clear", "started_profile", "network_error",
)


def read(path):
    if REF:
        return subprocess.check_output(["git", "-C", str(REPO), "show", f"{REF}:{path}"], text=True)
    return (REPO / path).read_text(encoding="utf-8")


def section(source, start, end):
    if source.count(start) != 1 or source.count(end) != 1:
        raise AssertionError(f"Production seam changed: {start!r}, {end!r}")
    return source[source.index(start):source.index(end, source.index(start))]


class SubscriptionNullProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="nekobox-subscription-null-")
        cls.addClassCleanup(cls.temp.cleanup)
        temporary = Path(cls.temp.name)
        header_dir = temporary / "nekobox/dataStore"
        header_dir.mkdir(parents=True)
        header = read("src/nekobox/dataStore/ProfileFilter.hpp")
        if header.count('#include "ProxyEntity.hpp"') != 1:
            raise AssertionError("Production header dependency seam changed")
        (header_dir / "ProfileFilter.hpp").write_text(header.replace(
            '#include "ProxyEntity.hpp"', '#include "support.hpp"'), encoding="utf-8")
        updater = read("src/gharqad/configs/sub/GroupUpdater.cpp")
        database = read("src/gharqad/dataStore/Database.cpp")
        group = read("src/gharqad/dataStore/Group.cpp")
        extracted = "namespace Subscription {\n" + section(updater,
            "bool RawUpdater::AddProxy(", "void GroupUpdater::AsyncUpdateGroup(") + section(updater,
            "void GroupUpdater::Update(", "\n} // namespace Subscription") + "\n}\nnamespace Configs {\n"
        extracted += section(database, "bool ProfileManager::AddProfileBatch(",
                             "bool ProfileManager::ReplaceProfile(")
        extracted += section(database, "void ProfileManager::BatchDeleteProfiles(",
                             "void ProfileManager::unlock()")
        extracted += section(group, "    bool Group::RemoveProfile(", "    bool Group::SwapProfiles(")
        extracted += section(group, "    bool Group::HasProfile(", "    std::shared_ptr<const GroupExtra> Group::getExtra()")
        extracted += "\n}\n"
        (temporary / "production.inc").write_text(extracted, encoding="utf-8")
        comparator = read("src/gharqad/dataStore/ProfileFilter.cpp")
        (temporary / "ProfileFilter.cpp").write_text(comparator, encoding="utf-8")
        cls.binary = temporary / "regression"
        command = shlex.split(os.environ.get("CXX", "g++")) + [
            "-std=c++20", "-O0", "-g", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
            "-fsanitize=undefined", "-fno-sanitize-recover=all",
            "-I", str(temporary), "-I", str(HERE), str(HERE / "regression.cpp"),
            str(temporary / "ProfileFilter.cpp"), "-o", str(cls.binary),
        ]
        compiled = subprocess.run(command, text=True, capture_output=True, timeout=60)
        if compiled.returncode:
            raise AssertionError(compiled.stdout + compiled.stderr)
        print(f"SOURCE {REF or 'working tree'}; compiled excerpt SHA256 "
              f"{hashlib.sha256(extracted.encode()).hexdigest()}", flush=True)

    def run_case(self, name):
        result = subprocess.run([str(self.binary), name], text=True, capture_output=True, timeout=10)
        print(result.stdout, end="", flush=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


for case in CASES:
    setattr(SubscriptionNullProfiles, "test_" + case, lambda self, name=case: self.run_case(name))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--ref", help="Read production sources from this immutable Git revision")
    args = parser.parse_args()
    REPO = args.repo.resolve()
    REF = args.ref
    unittest.main(argv=[__file__], verbosity=2)
