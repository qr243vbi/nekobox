#!/usr/bin/env python3
"""Offline real-Qt6 source-extracted Node conversion and import regressions."""
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
        raise ValueError(f"Expected one source anchor: {start}")
    begin = text.index(start)
    return text[begin:text.index(end, begin)]

def generate(read):
    node = read("src/gharqad/dataStore/ConfigData.cpp")
    header = read("src/nekobox/dataStore/Configs.hpp")
    enum = read("src/gharqad/dataStore/ConfigItem.cpp")
    code = (HERE / "support.hpp").read_text()
    code = code.replace("ENUM_DECLARATIONS", section(header, "struct EnumFieldName {", "// Custom hasher"))
    code = code.replace("NODE_DECLARATIONS", section(header, "        enum class Tag {", "\n    };\n}"))
    code += "\n" + section(enum, "EnumFieldName::EnumFieldName() = default;", "// --- Hasher and equality functors ---")
    signatures = (
        "Node Node::undefined()", "Node Node::null()", "Node::Node(const Node& node)", "Node::Node(Tag tag)",
        "Node Node::string(", "Node Node::boolean(", "Node Node::number(", "Node Node::map()", "Node Node::array()",
        "bool Node::add( Node val)", "bool Node::addLast( Node val)", "Tag Node::type() const",
        "bool Node::isNumber() const", "bool Node::isBoolean() const", "bool Node::isMap() const",
        "bool Node::isArray() const", "bool Node::isString() const", "bool Node::isNull() const",
        "bool Node::isUndefined() const", "bool Node::isDouble() const", "bool Node::isBool() const",
        "bool Node::isObject() const", "bool Node::isNothing() const", "long double Node::toNumber() const",
        "bool Node::toBoolean() const", "bool Node::toBool() const", "int Node::toInt() const",
        "bool Node::getBoolean(bool def) const", "long double Node::getNumber(long double def) const",
        "QString Node::getString(const EnumFieldName & def ) const", "QString Node::toString() const",
        "QStringList Node::toStringList() const", "size_t Node::count() const", "QList<Node> Node::values() const",
        "bool Node::contains(const EnumFieldName & index) const", "Node::Node(const EnumFieldName & str)",
        "Node::Node(bool val)", "Node::Node(double val)", "Node::Node(const QString & value)",
        "Node::Node(const QJsonArray &obj)", "Node::Node(QJsonValueConstRef value)", "Node::Node(const QJsonValue & value)",
        "Node Node::parseJsonValue(", "QVariant Node::toVariant() const", "QVariantList Node::toVariantList() const",
        "const Node & Node::at(const QList<EnumFieldName>& index) const",
    )
    code += "\nnamespace Configs { namespace Data {\n" + "\n".join(function(node, s) for s in signatures) + "\n}}\n"
    for path, signatures in (
        ("src/gharqad/configs/proxy/Json2Bean.cpp", ("void From_Json::add_mux_state(", "int From_Json::parseUOT(", "bool From_Json::add_tls(")),
        ("src/gharqad/configs/proxy/Yaml2Bean.cpp", ("void From_Yaml::add_mux_state(", "int From_Yaml::parseUOT(", "bool From_Yaml::add_tls(")),
    ):
        source = read(path)
        code += "\nnamespace Configs {\n" + "\n".join(function(source, s) for s in signatures) + "\n}\n"
    return code + "\n" + (HERE / "checks.cpp").read_text()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=HERE.parents[1])
    parser.add_argument("--ref", help="Read production files from an immutable git revision")
    parser.add_argument("--qt-root", type=Path, default=os.environ.get("QT6_ROOT"), help="Optional already-extracted Qt6 sysroot; installs nothing")
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
    if args.qt_root:
        includes = args.qt_root / "usr/include/x86_64-linux-gnu/qt6"
        libraries = args.qt_root / "usr/lib/x86_64-linux-gnu"
        flags = ["-I" + str(includes), "-I" + str(includes / "QtCore"), "-L" + str(libraries), "-Wl,-rpath," + str(libraries), "-l:libQt6Core.so.6"]
    else:
        probe = subprocess.run(["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True, capture_output=True)
        if probe.returncode:
            raise SystemExit("BLOCKED: Qt6Core development files unavailable; supply an existing --qt-root.")
        flags = shlex.split(probe.stdout)
    print("SOURCE:", args.ref or "working files", "in", args.repo, flush=True)
    print("SOURCE SHA256:", json.dumps(hashes, sort_keys=True), flush=True)
    print("MODE: real Qt6; production scalar/array constructors, conversions, EnumFieldName normalization and selected import helpers; map access boundary", flush=True)
    with tempfile.TemporaryDirectory(prefix="nekobox-node-boolean-") as td:
        cpp, binary = Path(td) / "checks.cpp", Path(td) / "checks"
        cpp.write_text(code)
        # Production's user-defined copy ctor plus implicit assignment emits an unrelated warning.
        command = shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-deprecated-copy", "-fPIC", "-O0"]
        command += shlex.split(os.environ.get("EXTRA_CXXFLAGS", "")) + [str(cpp), "-o", str(binary)] + flags
        subprocess.run(command, check=True, timeout=60)
        return subprocess.run([str(binary)], timeout=20).returncode

if __name__ == "__main__":
    raise SystemExit(main())
