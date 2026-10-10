#!/usr/bin/env python3
"""Compile real ProfileFilter.cpp with synthetic, offline dependency stand-ins."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SOURCE = Path(os.environ.get("PROFILE_FILTER_SOURCE", str(ROOT / "src/gharqad/dataStore/ProfileFilter.cpp")))
CASES = ["ordering_cross_fields", "ordering_laws", "credentials_retained", "transport_retained",
         "metadata_ignored", "by_address_ignores_bean", "custom_compares_bean", "endpoint_fields_retained",
         "keep_first_and_last", "exclusions_preserved", "common_matches_by_bean", "permutation_invariance"]

class ProfileFilterRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="profilefilter-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        header_dir = directory / "nekobox/dataStore"
        header_dir.mkdir(parents=True)
        # Keep production declarations unchanged. Only replace the unavailable
        # Qt/storage dependency, never ProfileFilter logic or its declarations.
        header = (ROOT / "src/nekobox/dataStore/ProfileFilter.hpp").read_text()
        dependency = '#include "ProxyEntity.hpp"'
        if header.count(dependency) != 1:
            raise AssertionError("ProfileFilter dependency seam changed; review this harness")
        header = header.replace(dependency, '#include "synthetic_dependencies.hpp"')
        (header_dir / "ProfileFilter.hpp").write_text(header)
        cls.binary = directory / "profilefilter_regression"
        command = shlex.split(os.environ.get("CXX", "c++")) + [
            "-std=c++20", "-O0", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
            "-I", str(directory), "-I", str(HERE), str(SOURCE),
            str(HERE / "profilefilter_regression.cpp"), "-o", str(cls.binary)]
        subprocess.run(command, check=True, text=True, capture_output=True)

    def run_case(self, name):
        result = subprocess.run([str(self.binary), name], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

for name in CASES:
    setattr(ProfileFilterRegression, "test_" + name, lambda self, case=name: self.run_case(case))

if __name__ == "__main__":
    unittest.main(verbosity=2)
