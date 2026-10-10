#!/usr/bin/env python3
"""Prove four completion regressions are rejected using temporary source copies."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
WINDOW = "src/gharqad/ui/mainwindow.cpp"
SOURCE = (ROOT / WINDOW).read_text(encoding="utf-8")
MUTATIONS = {
    "omit_join": ("      parallelCoreCallPool.waitForDone();", ""),
    "destroy_access_before_pool": (
        "      QMutex access;\n      QThreadPool parallelCoreCallPool;",
        "      QThreadPool parallelCoreCallPool;\n      QMutex access;"),
    "launch_only_one_worker": (
        "            });\n      }\n      parallelCoreCallPool.waitForDone();",
        "            });\n        break;\n      }\n      parallelCoreCallPool.waitForDone();"),
    "return_on_empty": (
        "      QList<std::shared_ptr<Configs::ProxyEntity>> out_del;\n      QMutex access;",
        "      if (out_all.empty()) return;\n"
        "      QList<std::shared_ptr<Configs::ProxyEntity>> out_del;\n      QMutex access;"),
}

for name, (before, after) in MUTATIONS.items():
    if SOURCE.count(before) != 1:
        raise AssertionError(f"Mutation source seam changed: {name}")
    with tempfile.TemporaryDirectory(prefix="nekobox-empty-mutation-") as directory:
        target = Path(directory)
        for path in (WINDOW, "src/gharqad/configs/sub/GroupUpdater.cpp",
                     "src/gharqad/dataStore/Database.cpp"):
            destination = target / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(SOURCE.replace(before, after) if path == WINDOW
                                   else (ROOT / path).read_text(encoding="utf-8"), encoding="utf-8")
        result = subprocess.run(
            ["python3", str(ROOT / "tests/test_subscription_empty_callback.py"), "--repo", str(target)],
            capture_output=True, text=True, timeout=60)
        output = result.stdout + result.stderr
        if result.returncode != 1 or "FAILED (failures=" not in output:
            raise AssertionError(f"Mutation was not rejected by a test assertion: {name}\n{output}")
        print(f"KILLED {name}: {output[output.rfind('FAILED ('):].strip()}", flush=True)
