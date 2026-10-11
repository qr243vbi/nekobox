#!/usr/bin/env python3
"""Run the real cache/download functions without loading the proxy engine."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path)
parser.add_argument("--go", default="go")
parser.add_argument("go_args", nargs="*")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = (args.source or root / "core/server/internal/utils.go").read_text()
names = ["DownloadFile", "copyAndCloseDownload", "urlToPath", "fileExists", "CacheHttpBool", "CacheHttp"]
functions = []
for name in names:
    match = re.search(r"^func " + name + r"\(.*?(?=^func |\Z)", source, re.M | re.S)
    if match:
        functions.append(match.group(0))
    elif name != "copyAndCloseDownload":
        raise SystemExit("Missing production function: " + name)

# The selected functions are copied verbatim. The only stand-in is the proxy
# client factory, which panics if invoked: these tests must never use a proxy.
prefix = '''package internal
import (
    "fmt"
    "io"
    "log"
    "net/http"
    "net/url"
    "os"
    "path/filepath"
    "strings"
)
var ruleset_cachedir string
var BoxInstance *struct{}
func BoxCreateHttpClient(_ *struct{}) *http.Client { panic("proxy client in isolated test") }
'''
with tempfile.TemporaryDirectory(prefix="nekobox-download-tests-") as tmp:
    tmp = Path(tmp)
    (tmp / "go.mod").write_text("module cache_download_regression\n\ngo 1.20\n")
    (tmp / "download.go").write_text(prefix + "\n".join(functions))
    shutil.copyfile(root / "core/server/internal/utils_download_test.go", tmp / "download_test.go")
    if "func copyAndCloseDownload(" in source:
        shutil.copyfile(root / "core/server/internal/utils_download_writer_test.go", tmp / "writer_test.go")
    env = dict(os.environ, GOTOOLCHAIN="local", GOPROXY="off", GOSUMDB="off", GOWORK="off")
    result = subprocess.run([args.go, "test", *args.go_args], cwd=tmp, env=env)
    raise SystemExit(result.returncode)
