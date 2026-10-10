#!/usr/bin/env python3
"""Offline source-extracted JSON import control-flow regression.

Default: explicit Qt/string adapters and Python-preparsed fixture objects.
--qt: native Qt6 Core string/JSON parsing (requires development headers).
Neither mode launches NekoBox, uses profiles, or makes network connections.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def section(text, start, end):
    if text.count(start) != 1:
        raise ValueError(f"Expected one start anchor: {start}")
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def literal(value):
    return json.dumps(value, ensure_ascii=True)


def cpp_value(value):
    if value is None:
        return "QJsonValue()"
    if isinstance(value, bool):
        return "QJsonValue(" + str(value).lower() + ")"
    if isinstance(value, (str, int)):
        return "QJsonValue(" + literal(value) + ")"
    if isinstance(value, list):
        return "QJsonArray{" + ",".join(map(cpp_value, value)) + "}"
    return "QJsonObject{" + ",".join("{" + literal(k) + "," + cpp_value(v) + "}" for k, v in value.items()) + "}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=HERE.parents[1])
    parser.add_argument("--ref", help="Read production source from this immutable Git revision")
    parser.add_argument("--qt", action="store_true")
    parser.add_argument("--fixture-parser", choices=["python", "qt5"], default="python",
                        help="Adapter fixture parser; qt5 requires PyQt5 in the selected Python")
    parser.add_argument("--keep-generated", type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    def read(path):
        if args.ref:
            return subprocess.check_output(["git", "-C", str(repo), "show", f"{args.ref}:{path}"], text=True)
        return (repo / path).read_text()
    source = read("src/gharqad/configs/sub/GroupUpdater.cpp")
    custom = read("src/gharqad/configs/proxy/CustomBean.cpp")
    # Anchors extract complete production functions without modifying their bodies.
    bodies = section(source, "int JsonEndIdx(", "static QString jsonFirstString(")
    bodies += section(source, "void RawUpdater::update(const QString &str3)", "void RawUpdater::updateSIP008(")
    bodies += section(source, "bool RawUpdater::AddProxy(", "void GroupUpdater::AsyncUpdateGroup(")
    build = section(custom, "    CoreObjOutboundBuildResult CustomBean::BuildCoreObjSingBox() const", "\n}")
    cases = json.loads((HERE / "fixtures.json").read_text())
    registrations = []
    constants = []
    seen = set()
    qt5 = None
    if args.fixture_parser == "qt5":
        from PyQt5 import QtCore
        qt5 = QtCore
        print("FIXTURE PARSER: installed Qt", qt5.qVersion(), flush=True)
    def register(text):
        if text in seen:
            return
        seen.add(text)
        try:
            value = json.loads(text)
        except json.JSONDecodeError:
            value = None
        if qt5 is not None:
            error = qt5.QJsonParseError()
            document = qt5.QJsonDocument.fromJson(text.encode(), error)
            qt_value = document.toVariant()
            expected_root = value if isinstance(value, (dict, list)) else None
            if qt_value != expected_root:
                raise AssertionError(f"Qt/Python fixture parse disagreement: {text!r}")
            value = qt_value
        registrations.append("jsonFixtures.emplace(" + literal(text) + ", " + cpp_value(value) + ");")
        if isinstance(value, list):
            for v in value:
                if isinstance(v, str):
                    register(v)
                elif isinstance(v, dict) and isinstance(v.get("proxy"), str):
                    register(v["proxy"])
        # Register the known single-line entries used by multiline fixtures.
        if not isinstance(value, (dict, list)):
            for line in text.splitlines():
                register(line.strip())
    for name, text in cases.items():
        constants.append("const QString " + name + " = " + literal(text) + ";")
        register(text)
    prefix = (HERE / "support.hpp").read_text()
    setup = "\n".join(constants) + "\nvoid registerFixtures() {\n#ifndef USE_REAL_QT\n" + "\n".join(registrations) + "\n#endif\n}\n"
    code = prefix + "\nnamespace Configs {\n" + build + "\n}\nnamespace Subscription {\n" + bodies + "\n}\n" + setup + (HERE / "checks.cpp").read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    flags = []
    if args.qt:
        probe = subprocess.run(["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True, capture_output=True)
        if probe.returncode:
            raise SystemExit("BLOCKED: Qt6 Core development headers/pkg-config metadata unavailable. No native Qt test executed.")
        flags = shlex.split(probe.stdout)
    with tempfile.TemporaryDirectory(prefix="nekobox-json-import-") as td:
        cpp, binary = Path(td) / "regression.cpp", Path(td) / "regression"
        cpp.write_text(code)
        command = shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", "-O0"]
        if args.qt:
            command += ["-DUSE_REAL_QT", "-fPIC"]
        command += shlex.split(os.environ.get("EXTRA_CXXFLAGS", ""))
        command += [str(cpp), "-o", str(binary)] + flags
        print("SOURCE:", args.ref or "working tree", "SHA256:", hashlib.sha256(source.encode()).hexdigest(), flush=True)
        print("MODE:", "native Qt6 Core" if args.qt else "Qt/string adapters; " + args.fixture_parser + "-preparsed JSON fixtures", flush=True)
        subprocess.run(command, check=True, timeout=60)
        return subprocess.run([str(binary)], timeout=20).returncode


if __name__ == "__main__":
    raise SystemExit(main())
