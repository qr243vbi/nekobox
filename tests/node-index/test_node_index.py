#!/usr/bin/env python3
"""Offline real-Qt regression for production Node constructors and index overloads."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def function(text, signature):
    if text.count(signature) != 1:
        raise ValueError(f"Expected one function anchor: {signature}")
    start = text.index(signature)
    begin = text.index("{", start)
    depth = 0
    for pos in range(begin, len(text)):
        depth += (text[pos] == "{") - (text[pos] == "}")
        if depth == 0:
            return text[start:pos + 1]
    raise ValueError(f"Unterminated function: {signature}")


def section(text, start, end):
    if text.count(start) != 1:
        raise ValueError(f"Expected one section anchor: {start}")
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def generate(read):
    header = read("src/nekobox/dataStore/Configs.hpp")
    node = read("src/gharqad/dataStore/ConfigData.cpp")
    enum = read("src/gharqad/dataStore/ConfigItem.cpp")
    code = """
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <variant>
#include <tuple>
#include <string>
#include <iostream>
#include <type_traits>
#include <utility>
// Referenced by the production declaration; no range/serialization is exercised.
template<class T> class KeyValueRange;
"""
    code += section(header, "struct EnumFieldName {", "// Custom hasher")
    code += "\nnamespace Configs { namespace Data {\n"
    code += section(header, "        enum class Tag {", "\n    };\n}")
    code += "\n}}\n"
    code += section(enum, "EnumFieldName::EnumFieldName() = default;", "// --- Hasher and equality functors ---")
    signatures = (
        "Node Node::undefined()", "Node Node::null()", "Node::Node(const Node& node)", "Node::Node(Tag tag)",
        "Node Node::string(", "Node Node::boolean(", "Node Node::number(", "Node Node::map()", "Node Node::array()",
        "bool Node::add( Node val)", "bool Node::addLast( Node val)", "Tag Node::type() const",
        "bool Node::isNumber() const", "bool Node::isBoolean() const", "bool Node::isMap() const",
        "bool Node::isArray() const", "bool Node::isString() const", "bool Node::isNull() const",
        "bool Node::isUndefined() const", "bool Node::getBoolean(bool def) const",
        "long double Node::getNumber(long double def) const", "QString Node::getString(const EnumFieldName & def ) const",
        "size_t Node::count() const", "bool Node::contains(const EnumFieldName & index) const",
        "Node::Node(const EnumFieldName & str)", "Node::Node(bool val)", "Node::Node(double val)",
        "Node::Node(const QString & value)", "Node::Node(const QJsonObject& obj)", "Node::Node(const QJsonArray &obj)",
        "Node::Node(QJsonValueConstRef value)", "Node::Node(const QJsonValue & value)", "Node Node::parseJsonValue(",
        "Node Node::fromVariantMap(",
        "const Node & Node::at(size_t index) const", "Node & Node::at(size_t index) {",
        "const Node & Node::at(const QList<EnumFieldName>& index) const", "Node & Node::at(const QList<EnumFieldName>& index) {",
        "const Node & Node::at(const EnumFieldName& t) const", "Node & Node::at(const EnumFieldName& t) {",
        "const Node & Node::operator [](size_t index) const", "Node & Node::operator [](size_t index){",
        "const Node & Node::operator [](const EnumFieldName& index) const", "Node & Node::operator [](const EnumFieldName& index){",
        "const Node & Node::operator [](const QList<EnumFieldName>& index) const", "Node & Node::operator [](const QList<EnumFieldName>& index){",
    )
    # Every executed Node/EnumFieldName body is production source, unchanged.
    code += "\nnamespace Configs { namespace Data {\n" + "\n".join(function(node, s) for s in signatures) + "\n}}\n"
    return code + "\n" + (HERE / "checks.cpp").read_text()


def qt_flags(root):
    if root:
        includes = root / "usr/include/x86_64-linux-gnu/qt6"
        libraries = root / "usr/lib/x86_64-linux-gnu"
        return ["-I" + str(includes), "-I" + str(includes / "QtCore"), "-L" + str(libraries), "-Wl,-rpath," + str(libraries), "-l:libQt6Core.so.6"]
    probe = subprocess.run(["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True, capture_output=True)
    if probe.returncode:
        raise SystemExit("BLOCKED: supply existing Qt6 Core development files with --qt-root; installs nothing.")
    return shlex.split(probe.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=HERE.parents[1])
    parser.add_argument("--ref", help="Extract production source from an immutable git revision")
    parser.add_argument("--qt-root", type=Path, default=os.environ.get("QT6_ROOT"))
    parser.add_argument("--keep-generated", type=Path)
    args = parser.parse_args()
    hashes = {}
    def read(path):
        text = subprocess.check_output(["git", "-C", str(args.repo), "show", f"{args.ref}:{path}"], text=True) if args.ref else (args.repo / path).read_text()
        hashes[path] = hashlib.sha256(text.encode()).hexdigest()
        return text
    code = generate(read)
    if args.keep_generated:
        args.keep_generated.write_text(code)
    print("SOURCE:", args.ref or "working files", "in", args.repo, flush=True)
    print("SOURCE SHA256:", json.dumps(hashes, sort_keys=True), flush=True)
    print("MODE: real Qt6; production scalar/map/array constructors and all index overloads; no Node adapters", flush=True)
    with tempfile.TemporaryDirectory(prefix="nekobox-node-index-") as td:
        cpp, binary = Path(td) / "checks.cpp", Path(td) / "checks"
        cpp.write_text(code)
        # Deliberately stop the defective baseline before execution. The compiler
        # diagnoses both direct recursive overloads, so no stack exhaustion is needed.
        # Existing signed array-size comparison and deprecated implicit assignment
        # are unrelated warnings in the production excerpts.
        command = shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Werror=infinite-recursion", "-Wno-deprecated-copy", "-Wno-sign-compare", "-fPIC", "-O0"]
        command += shlex.split(os.environ.get("EXTRA_CXXFLAGS", "")) + [str(cpp), "-o", str(binary)] + qt_flags(args.qt_root)
        compiled = subprocess.run(command, text=True, capture_output=True, timeout=60)
        if compiled.returncode:
            print(compiled.stdout + compiled.stderr, end="")
            if "[-Werror=infinite-recursion]" in compiled.stderr:
                print("FAIL: compiler diagnosed infinite recursion; unsafe executable was not run.")
                return 1
            print("ERROR: unrelated compilation failure; this does not establish the regression.")
            return 2
        return subprocess.run([str(binary)], timeout=20).returncode


if __name__ == "__main__":
    raise SystemExit(main())
