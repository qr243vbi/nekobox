#!/usr/bin/env python3
"""Source-extracted ECH settings regression, without network or user profiles.

Compiles the production stream declarations/map, map macros, typed field access,
and JsonStore::ToJson/FromJson. Default mode supplies small standard-library
adapters for Qt containers; --qt uses actual Qt6 Core instead. This is a focused
serialization diagnostic, not a full NekoBox/editor/binary-database test.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def section(text, start, end):
    if text.count(start) != 1:
        raise ValueError(f"Expected exactly one source anchor: {start}")
    return start + text.split(start, 1)[1].split(end, 1)[0]


def function(text, signature):
    if text.count(signature) != 1:
        raise ValueError(f"Expected exactly one function: {signature}")
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for pos in range(brace, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
    raise ValueError(f"Unterminated function: {signature}")


def macro(text, name):
    pattern = rf"^#define {name}(?=[(\s])(?:\([^\n]*?\))?(?:[^\n]*\\\n)*[^\n]*"
    matches = re.findall(pattern, text, re.MULTILINE)
    if len(matches) != 1:
        raise ValueError(f"Expected exactly one macro: {name}")
    return matches[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--ref", help="Read an immutable Git revision instead of working files")
    parser.add_argument("--qt", action="store_true", help="Use Qt6 Core, located through pkg-config")
    parser.add_argument("--keep-generated", type=Path, help="Save the generated translation unit")
    args = parser.parse_args()
    repo = args.repo.resolve()

    def read(path):
        if args.ref:
            return subprocess.check_output(["git", "-C", str(repo), "show", f"{args.ref}:{path}"], text=True)
        return (repo / path).read_text()

    stream = read("src/nekobox/configs/proxy/V2RayStreamSettings.hpp")
    utils = read("src/nekobox/dataStore/Utils.hpp")
    item = read("src/gharqad/dataStore/ConfigItem.cpp")
    configs = read("src/gharqad/dataStore/Configs.cpp")
    declarations = section(stream, "    class KCPExtra:", "        void BuildStreamSettingsSingBox(") + "\n};\n"
    source = [Path(__file__).with_name("support.hpp").read_text()]
    source += [macro(utils, name) for name in ("MAP_BODY", "NEW_MAP", "STOP_MAP", "ADD_MAP")]
    source += [macro(item, name) for name in ("GET_PTR_OR_RETURN", "SET_NODE", "GET_PTR_TYPE", "GET_NODE", "PUT_STORE")]
    source += ["namespace Configs {\n" + declarations + "\n}\n"]
    source += [function(item, "static void _put_store(")]
    source += [function(configs, "void * configItem::getPtr(")]
    source += [function(item, "QJsonValue configItem::getNode(const JsonStore * store)")]
    source += [function(item, "void configItem::setNode(const JsonStore * store,")]
    for item_type in ("int", "str", "bool", "enum"):
        source += [function(item, f"SET_NODE({item_type}) {{")]
        source += [function(item, f"GET_NODE({item_type}) {{")]
    for signature in (
        "void JsonStore::_put(ConfJsMap _map, const QString &str, int *value)",
        "void JsonStore::_put(ConfJsMap _map, const QString &str, QString *value)",
        "void JsonStore::_put(ConfJsMap _map, const QString &str, bool *value)",
        "void JsonStore::_put(ConfJsMap _map, const QString &str,\n                     std::shared_ptr<JsonEnum>*value)",
    ):
        source += [function(item, signature)]
    source += [function(configs, "QJsonObject JsonStore::ToJson(")]
    source += [function(configs, "void JsonStore::FromJson(")]
    source += [Path(__file__).with_name("checks.cpp").read_text()]
    code = "\n\n".join(source)
    if args.keep_generated:
        args.keep_generated.write_text(code)
    flags = []
    if args.qt:
        flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True))
    with tempfile.TemporaryDirectory(prefix="nekobox-ech-") as temp:
        cpp, executable = Path(temp) / "regression.cpp", Path(temp) / "regression"
        cpp.write_text(code)
        command = shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-O0"]
        if args.qt:
            command += ["-DUSE_REAL_QT", "-fPIC"]
        command += [str(cpp), "-o", str(executable)] + flags
        print("SOURCE:", args.ref or "working tree", "in", repo, flush=True)
        print("MODE:", "Qt6 Core JSON bytes" if args.qt else "standard-library Qt adapters, JSON object roundtrip", flush=True)
        print("COMPILE:", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        return subprocess.run([str(executable)]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
