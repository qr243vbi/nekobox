#!/usr/bin/env python3
"""Compile verbatim ECH import/export and Data::Node conversion excerpts.

Default uses explicit Qt/container adapters. --qt uses Qt6 Core development
headers for strings, JSON, and containers. Neither is a full application test.
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


def cpp_value(value):
    if value is None:
        return "QJsonValue()"
    if isinstance(value, bool):
        return "QJsonValue(" + str(value).lower() + ")"
    if isinstance(value, (str, int, float)):
        return "QJsonValue(" + json.dumps(value, ensure_ascii=True) + ")"
    if isinstance(value, list):
        return "QJsonArray{" + ",".join(map(cpp_value, value)) + "}"
    return "QJsonObject{" + ",".join("{" + json.dumps(k) + "," + cpp_value(v) + "}" for k, v in value.items()) + "}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=HERE.parents[1])
    parser.add_argument("--ref", help="Read immutable production files through git show")
    parser.add_argument("--qt", action="store_true")
    parser.add_argument("--fixture-parser", choices=("python", "qt5"), default="python",
                        help="In adapter mode, optionally cross-check fixture JSON with installed PyQt5")
    parser.add_argument("--keep-generated", type=Path)
    args = parser.parse_args()
    paths = {}
    def read(path):
        text = subprocess.check_output(["git", "-C", str(args.repo), "show", f"{args.ref}:{path}"], text=True) if args.ref else (args.repo / path).read_text()
        paths[path] = hashlib.sha256(text.encode()).hexdigest()
        return text
    importer = read("src/gharqad/configs/proxy/Json2Bean.cpp")
    exporter = read("src/gharqad/configs/proxy/Bean2CoreObj_box.cpp")
    node = read("src/gharqad/dataStore/ConfigData.cpp")
    stream = read("src/nekobox/configs/proxy/V2RayStreamSettings.hpp")
    declarations = section(stream, '        QString ech_config = "";', '\n\n        std::shared_ptr<V2RAYTransportsEnum>')
    bodies = [function(importer, "bool From_Json::add_tls(")]
    node_bodies = [function(node, s) for s in (
        "bool Node::isMap() const", "bool Node::isObject() const", "bool Node::isArray() const",
        "bool Node::isString() const", "bool Node::isNumber() const", "bool Node::isBoolean() const",
        "bool Node::isBool() const", "bool Node::isNull() const", "bool Node::isUndefined() const",
        "bool Node::isNothing() const", "bool Node::toBoolean() const", "bool Node::toBool() const",
        "bool Node::getBoolean(bool def) const", "QString Node::getString(const EnumFieldName & def ) const",
        "QString Node::toString() const", "QStringList Node::toStringList() const", "size_t Node::count() const",
        "QList<Node> Node::values() const", "bool Node::contains(const EnumFieldName & index) const",
    )]
    export_body = section(exporter, '            QJsonObject tls{{"enabled", true}};', '            add_non_empty(tls, "server_name", sni);')
    code = (HERE / "support.hpp").read_text().replace("ECH_DECLARATIONS", declarations)
    code += "\nnamespace Configs { namespace Data {\n" + "\n".join(node_bodies) + "\n} " + "\n".join(bodies) + "\n"
    code += "QJsonObject V2rayStreamSettings::exportEch() const {\n" + export_body + '\nreturn QJsonObject{{"tls", tls}};\n}\n}\n'
    for bean in ("AnyTLS", "Http", "Juicity", "Naive", "ShadowTLS", "TrojanVLESS", "TrustTunnel", "VMess"):
        body = function(read(f"src/gharqad/configs/proxy/{bean}Bean.cpp"), f"bool {bean}Bean::TryParseJson(")
        code += "\nnamespace Configs {\n" + body + "\n}\n"
    code += "\nnamespace Configs {\n" + function(read("src/gharqad/configs/proxy/TrustTunnelBean.cpp"), "bool TrustTunnelBean::TryParseYaml(") + "\n}\n"
    updater = read("src/gharqad/configs/sub/GroupUpdater.cpp")
    code += "\nnamespace Subscription {\n" + function(updater, "void RawUpdater::updateSingBox(") + "\n}\n"
    fixture_text = (HERE / "fixtures.json").read_text()
    fixtures = json.loads(fixture_text)
    if args.fixture_parser == "qt5":
        from PyQt5 import QtCore
        error = QtCore.QJsonParseError()
        parsed = QtCore.QJsonDocument.fromJson(fixture_text.encode(), error).toVariant()
        if error.error != QtCore.QJsonParseError.NoError or parsed != fixtures:
            raise ValueError("Qt5/Python fixture parse disagreement")
        fixtures = parsed
        print("FIXTURE PARSER: Qt", QtCore.qVersion(), "cross-checked with Python", flush=True)
    code += "\nstruct Fixture { const char *name; QJsonObject outbound; bool accepted, enabled; QString config, query; };\n"
    code += "const std::vector<Fixture> fixtures = {\n"
    for case in fixtures:
        literal = lambda value: json.dumps(value, ensure_ascii=True)
        outbound = cpp_value(case["outbound"])
        if args.qt:
            outbound = "QJsonDocument::fromJson(" + literal(json.dumps(case["outbound"])) + ").object()"
        code += "{" + literal(case["name"]) + "," + outbound + "," + literal(case["accepted"]) + "," + literal(case["enabled"]) + "," + literal(case["config"]) + "," + literal(case["query"]) + "},\n"
    code += "};\n" + (HERE / "checks.cpp").read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    flags = []
    if args.qt:
        probe = subprocess.run(["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True, capture_output=True)
        if probe.returncode:
            raise SystemExit("BLOCKED: Qt6 Core development headers/pkg-config metadata unavailable; native Qt6 test not run.")
        flags = ["-DUSE_REAL_QT", "-fPIC"] + shlex.split(probe.stdout)
    print("SOURCE:", args.ref or "working files", "in", args.repo, flush=True)
    print("SOURCE SHA256:", json.dumps(paths, sort_keys=True), flush=True)
    print("MODE:", "Qt6 Core with Node construction adapter" if args.qt else "explicit Qt/container/Node construction adapters", flush=True)
    with tempfile.TemporaryDirectory(prefix="nekobox-ech-json-") as td:
        cpp, binary = Path(td) / "checks.cpp", Path(td) / "checks"
        cpp.write_text(code)
        command = shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-O0"]
        command += shlex.split(os.environ.get("EXTRA_CXXFLAGS", "")) + [str(cpp), "-o", str(binary)] + flags
        subprocess.run(command, check=True, timeout=60)
        return subprocess.run([str(binary)], timeout=20).returncode


if __name__ == "__main__":
    raise SystemExit(main())
