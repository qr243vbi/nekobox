#!/usr/bin/env python3
"""Real Qt proxy-cache regression; Python equivalent, NOT production C++.

The cache, indexes, and signals are real Qt. Only the small production proxy's
state and matching logic are mirrored in Python. --invalidate controls which
setters call Qt's actual invalidateFilter(), allowing honest red/green and
single-setter omission checks without pretending to execute the C++ source.
"""

import argparse
import importlib
import sys
import unittest

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--invalidate", default="all",
                    help="all, none, or comma-separated global,column,enabled")
args, remaining = parser.parse_known_args()
INVALIDATE = {"global", "column", "enabled"} if args.invalidate == "all" else set(args.invalidate.split(","))
if not INVALIDATE <= {"global", "column", "enabled", "none"}:
    parser.error("unknown setter in --invalidate")

for binding in ("PySide6", "PyQt6", "PySide2", "PyQt5"):
    try:
        QtCore = importlib.import_module(binding + ".QtCore")
        break
    except ImportError:
        continue
else:
    raise SystemExit("No supported Qt Python binding installed; nothing tested")

Qt = QtCore.Qt
Roles = getattr(Qt, "ItemDataRole", Qt)
DISPLAY_ROLE = Roles.DisplayRole
ID_ROLE = int(Roles.UserRole) + 3
QModelIndex = QtCore.QModelIndex
ROWS = (
    (101, ("Poland Alpha", "east", "tcp")),
    (102, ("Germany Beta", "west", "udp")),
    (103, ("Poland Beta", "west", "tcp")),
)


class FixedSource(QtCore.QAbstractTableModel):
    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(ROWS)

    def columnCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else 3

    def data(self, index, role=DISPLAY_ROLE):
        if not index.isValid():
            return None
        if role == DISPLAY_ROLE:
            return ROWS[index.row()][1][index.column()]
        if role == ID_ROLE:
            return ROWS[index.row()][0]
        return None


class EquivalentColumnFilterProxy(QtCore.QSortFilterProxyModel):
    def __init__(self):
        super().__init__()
        self.m_filters = {}
        self.enabled = False
        self.m_globalFilter = ""

    def setEnabled(self, enable):
        self.enabled = enable
        if not enable:
            self.m_filters.clear()
        if "enabled" in INVALIDATE:
            self.invalidateFilter()

    def setColumnFilter(self, column, text):
        if not text:
            self.m_filters.pop(column, None)
        else:
            self.m_filters[column] = text
        if "column" in INVALIDATE:
            self.invalidateFilter()

    def setGlobalFilter(self, text):
        self.m_globalFilter = text
        if "global" in INVALIDATE:
            self.invalidateFilter()

    def filterAcceptsRow(self, row, parent):
        src = self.sourceModel()
        if src is None:
            return True
        if self.m_globalFilter:
            if not any(self.m_globalFilter.casefold() in
                       str(src.data(src.index(row, col, parent))).casefold()
                       for col in range(src.columnCount(parent))):
                return False
        if self.enabled:
            for col, pattern in self.m_filters.items():
                if pattern.casefold() not in str(src.data(src.index(row, col, parent))).casefold():
                    return False
        return True


class CacheRegression(unittest.TestCase):
    def setUp(self):
        self.source = FixedSource()
        self.proxy = EquivalentColumnFilterProxy()
        self.proxy.setSourceModel(self.source)
        self.proxy.setDynamicSortFilter(False)
        self.source_changes = []
        for name in ("modelReset", "layoutChanged", "dataChanged", "rowsInserted", "rowsRemoved"):
            getattr(self.source, name).connect(lambda *unused, n=name: self.source_changes.append(n))

    def check_ids(self, expected):
        self.assertEqual(self.source.rowCount(), 3, "source count must stay fixed")
        self.assertFalse(self.source_changes, "no source reset/change may rescue stale mapping")
        self.assertFalse(self.proxy.dynamicSortFilter())
        actual = []
        for row in range(self.proxy.rowCount()):
            proxy_index = self.proxy.index(row, 0)
            source_index = self.proxy.mapToSource(proxy_index)
            self.assertTrue(source_index.isValid())
            self.assertEqual(self.proxy.mapFromSource(source_index), proxy_index)
            actual.append(self.source.data(source_index, ID_ROLE))
        self.assertEqual(actual, expected)
        for row, (identity, unused) in enumerate(ROWS):
            self.assertEqual(self.proxy.mapFromSource(self.source.index(row, 0)).isValid(), identity in expected)

    def test_global_changes_reuse_established_mapping(self):
        self.proxy.setGlobalFilter("pOLaNd")
        self.check_ids([101, 103])
        for text, expected in [("beta", [102, 103]), ("", [101, 102, 103]),
                               ("EaSt", [101]), ("absent", []),
                               ("Poland", [101, 103]), ("", [101, 102, 103])]:
            self.proxy.setGlobalFilter(text)
            self.check_ids(expected)

    def test_column_changes_and_clears_reuse_established_mapping(self):
        self.proxy.setEnabled(True)
        self.proxy.setColumnFilter(0, "Poland")
        self.check_ids([101, 103])
        for text, expected in [("bETA", [102, 103]), ("", [101, 102, 103]),
                               ("absent", []), ("Alpha", [101]), ("", [101, 102, 103])]:
            self.proxy.setColumnFilter(0, text)
            self.check_ids(expected)

    def test_enabling_columns_keeps_active_global_filter(self):
        self.proxy.setGlobalFilter("beta")
        self.proxy.setColumnFilter(0, "Poland")
        self.check_ids([102, 103])  # default enabled is false
        self.proxy.setEnabled(True)
        self.check_ids([103])

    def test_disabling_clears_columns_but_keeps_global_filter(self):
        self.proxy.setEnabled(True)
        self.proxy.setGlobalFilter("beta")
        self.proxy.setColumnFilter(0, "Poland")
        self.check_ids([103])
        self.proxy.setEnabled(False)
        self.check_ids([102, 103])
        self.proxy.setEnabled(True)
        self.check_ids([102, 103])  # disabling cleared the old column filter
        self.proxy.setColumnFilter(0, "Germany")
        self.check_ids([102])
        self.proxy.setEnabled(False)
        self.check_ids([102, 103])

    def test_global_any_column_and_column_filters_are_intersected(self):
        self.proxy.setEnabled(True)
        self.proxy.setGlobalFilter("beta")
        self.proxy.setColumnFilter(2, "tcp")
        self.check_ids([103])
        self.proxy.setColumnFilter(0, "Germany")
        self.check_ids([])
        self.proxy.setColumnFilter(2, "")
        self.check_ids([102])
        self.proxy.setColumnFilter(0, "")
        self.check_ids([102, 103])
        self.proxy.setGlobalFilter("EAST")  # match outside the name column
        self.check_ids([101])
        self.proxy.setGlobalFilter("")
        self.check_ids([101, 102, 103])

    def test_columns_default_disabled_and_repeated_disable_clears_them(self):
        self.check_ids([101, 102, 103])
        self.proxy.setColumnFilter(0, "Poland")
        self.check_ids([101, 102, 103])
        self.proxy.setEnabled(False)
        self.check_ids([101, 102, 103])
        self.proxy.setEnabled(True)
        self.check_ids([101, 102, 103])


if __name__ == "__main__":
    app = QtCore.QCoreApplication([])
    print(f"Python equivalent on real {binding} / Qt {QtCore.qVersion()}, invalidation={args.invalidate}", flush=True)
    unittest.main(argv=[sys.argv[0]] + remaining, verbosity=2)
