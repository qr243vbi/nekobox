#!/usr/bin/env python3
"""Verify the JSON importer regression rejects targeted source mutations.

Only temporary copies are changed. No application or network is used.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PATH = Path("src/gharqad/configs/sub/GroupUpdater.cpp")
CUSTOM = Path("src/gharqad/configs/proxy/CustomBean.cpp")
source = (ROOT / PATH).read_text()
addition = '    AddProxy(ent);\n    goto ret_loop;\n  }\n\n  if (str.startsWith("//")'
mutations = {
    "drop_json_addition": (addition, '    goto ret_loop;\n  }\n\n  if (str.startsWith("//")'),
    "accept_invalid_types": ('json["type"].isString() && !json["type"].toString().isEmpty()', 'json.contains("type")'),
    "select_wrong_profile_mode": ('bean->core = "internal";\n      bean->config_simple = str;', 'bean->core = "internal-full";\n      bean->config_simple = str;'),
    "return_before_remaining_stack": (addition, '    AddProxy(ent);\n    return;\n  }\n\n  if (str.startsWith("//")'),
    "bypass_ignore_boundary": (addition, '    proxies << ent;\n    goto ret_loop;\n  }\n\n  if (str.startsWith("//")'),
}
log_dir = Path(os.environ["MUTATION_LOG_DIR"]) if os.environ.get("MUTATION_LOG_DIR") else None
if log_dir:
    log_dir.mkdir(parents=True, exist_ok=True)
failures = 0
for name, (before, after) in mutations.items():
    if source.count(before) != 1:
        raise SystemExit(f"Expected exactly one mutation anchor for {name}")
    with tempfile.TemporaryDirectory(prefix="nekobox-json-mutation-") as td:
        repo = Path(td)
        (repo / PATH).parent.mkdir(parents=True)
        (repo / PATH).write_text(source.replace(before, after))
        (repo / CUSTOM).parent.mkdir(parents=True)
        (repo / CUSTOM).write_bytes((ROOT / CUSTOM).read_bytes())
        result = subprocess.run([sys.executable, str(HERE / "test_json_import.py"), "--repo", str(repo)], text=True, capture_output=True, timeout=90)
        report = result.stdout + result.stderr
        if log_dir:
            (log_dir / (name + ".log")).write_text(report)
        # A compile error/crash is not evidence that an assertion killed it.
        killed = result.returncode == 1 and "RESULT:" in report and "FAIL " in report
        print(("PASS " if killed else "FAIL ") + name)
        if not killed:
            failures += 1
            print(report)
print(f"RESULT: {len(mutations)-failures}/{len(mutations)} mutations rejected by runtime assertions")
raise SystemExit(bool(failures))
