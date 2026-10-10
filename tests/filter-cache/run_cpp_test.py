#!/usr/bin/env python3
"""Compile unchanged production proxy class/methods against real Qt6 Core.

Extraction isolates the proxy from unrelated main-window/database dependencies.
No matching logic, setter, Q_OBJECT macro, or default value is substituted.
Qt6 Core development files, moc, CMake, and a C++20 compiler are prerequisites.
This is a focused extracted-production test, not a full application build.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source-root", type=Path, default=HERE.parent.parent)
parser.add_argument("--build-dir", required=True, type=Path)
parser.add_argument("--extract-only", action="store_true")
args = parser.parse_args()
root = args.source_root.resolve()
build = args.build_dir.resolve()
generated = build / "production-proxy"
generated.mkdir(parents=True, exist_ok=True)
header_path = root / "src/nekobox/ui/mainwindow_table.h"
cpp_path = root / "src/gharqad/ui/mainwindow_table.cpp"
header = header_path.read_bytes()
cpp = cpp_path.read_bytes()
declaration = header[header.index(b"class ColumnFilterProxy :"):header.index(b"class SelectionKeeper :")]
definitions = cpp[cpp.index(b"void ColumnFilterProxy::setEnabled("):cpp.index(b"void FilterHeader::setFilterCount(")]
for method in (b"setEnabled", b"setColumnFilter", b"setGlobalFilter", b"filterAcceptsRow"):
    if definitions.count(b"ColumnFilterProxy::" + method + b"(") != 1:
        raise SystemExit("Production source layout changed; review extraction before testing")
if b"Q_OBJECT" not in declaration or b"bool enabled = false;" not in declaration:
    raise SystemExit("Production declaration changed; review extraction before testing")
(generated / "column_filter_proxy.h").write_bytes(
    b"#pragma once\n#include <QSortFilterProxyModel>\n#include <QHash>\n#include <QString>\n\n" + declaration)
(generated / "column_filter_proxy.cpp").write_bytes(b'#include "column_filter_proxy.h"\n\n' + definitions)
manifest = {
    "source_root": str(root),
    "header_sha256": hashlib.sha256(header).hexdigest(),
    "cpp_sha256": hashlib.sha256(cpp).hexdigest(),
    "declaration_sha256": hashlib.sha256(declaration).hexdigest(),
    "definitions_sha256": hashlib.sha256(definitions).hexdigest(),
    "transformation": "Only dependency includes added; class and four definitions copied verbatim.",
}
(generated / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2), flush=True)
if args.extract_only:
    raise SystemExit(0)
if shutil.which("cmake") is None:
    raise SystemExit("BLOCKED: cmake is not installed; production C++ was not compiled or run")
if shutil.which("ctest") is None:
    raise SystemExit("BLOCKED: ctest is not installed; production C++ was not compiled or run")
for command in (
    ["cmake", "-S", str(HERE), "-B", str(build), f"-DPROXY_SOURCE_DIR={generated}", "-DCMAKE_BUILD_TYPE=Debug"],
    ["cmake", "--build", str(build), "--config", "Debug"],
):
    print("+ " + " ".join(command), flush=True)
    result = subprocess.run(command)
    if result.returncode:
        raise SystemExit(result.returncode)

# --test-dir requires CMake 3.20; cwd works with the declared 3.16 minimum.
# JSON listing is supported since 3.14 and prevents an empty suite passing.
listing = subprocess.run(["ctest", "-C", "Debug", "--show-only=json-v1"],
                         cwd=build, text=True, stdout=subprocess.PIPE, check=True)
names = [test["name"] for test in json.loads(listing.stdout)["tests"]]
if names != ["column_filter_cache"]:
    raise SystemExit(f"Unexpected CTest registration; refusing an empty or changed suite: {names}")
command = ["ctest", "-C", "Debug", "--output-on-failure"]
print("+ " + " ".join(command) + f" (cwd={build})", flush=True)
raise SystemExit(subprocess.run(command, cwd=build).returncode)
