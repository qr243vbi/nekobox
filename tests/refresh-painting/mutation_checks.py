#!/usr/bin/env python3
"""Reject painting and sorting-contract regressions using production-source edits."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
source = (ROOT / "src/gharqad/ui/mainwindow.cpp").read_text()
mutations = (
    ("restore_outer_disable",
     "GroupSortAction groupSortAction) {\n\n",
     "GroupSortAction groupSortAction) {\n\n  ui->proxyListTable->setUpdatesEnabled(false);\n",
     "test_missing_group_full"),
    ("reject_12000_boundary", "currentGroup->profiles.count() > 12000",
     "currentGroup->profiles.count() >= 12000", "test_boundary_12000"),
    ("continue_after_sort_warning",
     'MessageBoxWarning(software_name, tr("Group is too big to sort"));\n            return;',
     'MessageBoxWarning(software_name, tr("Group is too big to sort"));', "test_oversized_name"),
    ("omit_final_restore",
     "  ui->proxyListTable->setUpdatesEnabled(true);\n\n  this->tableModel->refresh();",
     "  this->tableModel->refresh();", "test_normal_full"),
    ("omit_missing_id_restore",
     "      ui->proxyListTable->setUpdatesEnabled(true);\n      return;",
     "      return;", "test_missing_id_small"),
)
with tempfile.TemporaryDirectory(prefix="refresh-painting-mutations-") as directory:
    for name, before, after, expected in mutations:
        if source.count(before) != 1:
            raise SystemExit(f"{name}: production mutation seam changed")
        path = Path(directory) / (name + ".cpp")
        path.write_text(source.replace(before, after))
        env = dict(os.environ, REFRESH_PAINTING_SOURCE=str(path), REFRESH_PAINTING_QT="0")
        result = subprocess.run([sys.executable, str(HERE / "test_refresh_painting.py")],
                                env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode != 1 or "FAILED (failures=" not in result.stdout or f"FAIL: {expected}" not in result.stdout:
            print(result.stdout)
            raise SystemExit(f"{name}: expected assertion failure was not observed")
        print(f"REJECTED {name}: {expected}")
