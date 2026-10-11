#!/usr/bin/env python3
"""Verify that individual production-source regressions fail the Qt diagnostic."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
EDITOR = "src/gharqad/ui/profile/dialog_edit_profile.cpp"
INPUTS = (EDITOR, "src/nekobox/ui/profile/dialog_edit_profile.ui",
          "src/nekobox/configs/proxy/V2RayStreamSettings.hpp")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--logs", type=Path)
    args = parser.parse_args()
    source = (args.repo / EDITOR).read_text()
    mutations = {
        "wrong_ech_initial_value": ("CACHE.ech_config, &ok", "CACHE.certificate, &ok"),
        "missing_qsn_load": ("    ui->query_server_name->setText(stream->query_server_name);\n", ""),
        "missing_qsn_save": ("    stream->query_server_name = ui->query_server_name->text();\n", ""),
        "cancel_updates_ech": ("if (ok) {\n    CACHE.ech_config = txt;",
                               "if (!ok) {\n    CACHE.ech_config = txt;"),
        "ech_overwrites_certificate": ("CACHE.ech_config = txt;", "CACHE.certificate = txt;"),
    }
    if args.logs:
        args.logs.mkdir(parents=True, exist_ok=True)
    failed = []
    with tempfile.TemporaryDirectory(prefix="nekobox-ech-editor-mutations-") as temp:
        root = Path(temp)
        for relative in INPUTS:
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((args.repo / relative).read_bytes())
        for name, (old, new) in mutations.items():
            if source.count(old) != 1:
                raise ValueError(f"Expected one mutation anchor for {name}")
            (root / EDITOR).write_text(source.replace(old, new, 1))
            result = subprocess.run([sys.executable, str(HERE / "test_qt_bindings.py"),
                                     "--repo", str(root), "-q"], text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=30)
            if args.logs:
                (args.logs / f"{name}.log").write_text(result.stdout)
            caught = result.returncode == 1 and "FAILED (failures=" in result.stdout and "ERROR:" not in result.stdout
            print(f"{name}: {'CAUGHT by assertions' if caught else 'UNEXPECTED RESULT'}")
            if not caught:
                print(result.stdout)
                failed.append(name)
    print(f"{len(mutations) - len(failed)}/{len(mutations)} source mutations caught")
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())
