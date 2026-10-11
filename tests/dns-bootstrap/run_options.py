#!/usr/bin/env python3
"""Validate saved Qt test output with pinned Go options, strictly offline.

Requires the module cache for core/server/go.mod. Never constructs core or
transport objects. GO may point to a compatible installed Go executable.
"""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("observations", type=Path)
    parser.add_argument("--repo", type=Path, default=HERE.parents[1])
    args = parser.parse_args()
    environment = dict(os.environ, GOPROXY="off", GOSUMDB="off", GOTOOLCHAIN="local",
                       GOWORK="off", CGO_ENABLED="0")
    with tempfile.TemporaryDirectory(prefix="nekobox-dns-options-") as directory:
        temporary = Path(directory)
        for name in ("go.mod", "go.sum"):
            shutil.copyfile(args.repo / "core/server" / name, temporary / name)
        shutil.copyfile(HERE / "options_check.go", temporary / "options_check.go")
        with args.observations.open() as observations:
            return subprocess.run(shlex.split(os.environ.get("GO", "go")) +
                                  ["run", "-mod=readonly", "options_check.go"],
                                  cwd=temporary, env=environment, stdin=observations,
                                  timeout=120).returncode


if __name__ == "__main__":
    raise SystemExit(main())
