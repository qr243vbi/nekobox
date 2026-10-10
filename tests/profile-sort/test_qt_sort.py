#!/usr/bin/env python3
"""Real Qt selection/proxy regression using a Python equivalent, NOT C++ execution.

--mode=baseline mirrors the unnotified same-count reorder. The other modes test
paired layout signals, persistent-index remapping, and snapshot timing on real
Qt model/view objects. Run the extracted-production Qt6 target for C++ coverage.
"""
import argparse
import importlib
import sys
import unittest
from functools import cmp_to_key

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mode', choices=['baseline', 'signals-only', 'early-snapshot', 'fixed'], default='fixed')
args, remaining = parser.parse_known_args()
for binding in ('PySide6', 'PyQt6', 'PySide2', 'PyQt5'):
    try:
        C = importlib.import_module(binding + '.QtCore')
        W = importlib.import_module(binding + '.QtWidgets')
        break
    except ImportError:
        continue
else:
    raise SystemExit('No supported Qt Python binding; nothing tested')
Qt = C.Qt
Roles = getattr(Qt, 'ItemDataRole', Qt)
DISPLAY = Roles.DisplayRole
ID = int(Roles.UserRole) + 3
Flags = getattr(C.QItemSelectionModel, 'SelectionFlag', C.QItemSelectionModel)
INDEX = C.QModelIndex
ROWS = {101: ('tcp', 'east', 'keep charlie', '30', '100'),
        102: ('udp', 'west', 'skip alpha', '10', '300'),
        103: ('tcp', 'west', 'keep bravo', '20', '200'),
        104: ('tcp', 'east', 'keep bravo', '20', '200')}


class EquivalentSource(C.QAbstractTableModel):
    def __init__(self, ids=(101, 102, 103)):
        super().__init__()
        self.ids = list(ids)

    def rowCount(self, parent=INDEX()):
        return 0 if parent.isValid() else len(self.ids)

    def columnCount(self, parent=INDEX()):
        return 0 if parent.isValid() else 5

    def data(self, idx, role=DISPLAY):
        if not idx.isValid():
            return None
        identity = self.ids[idx.row()]
        return identity if role == ID else ROWS[identity][idx.column()] if role == DISPLAY else None

    def sortProfiles(self, column=2, descending=False):
        def less(a, b):
            a, b = ROWS[a][column], ROWS[b][column]
            return a > b if descending else a < b
        order = sorted(range(len(self.ids)), key=cmp_to_key(
            lambda a, b: -1 if less(self.ids[a], self.ids[b]) else 1 if less(self.ids[b], self.ids[a]) else 0))
        new_ids = [self.ids[row] for row in order]
        if new_ids == self.ids:
            return
        old_to_new = [None] * len(order)
        for new, old in enumerate(order):
            old_to_new[old] = new
        if args.mode == 'early-snapshot':
            before = self.persistentIndexList()
        if args.mode != 'baseline':
            self.layoutAboutToBeChanged.emit()
        if args.mode != 'early-snapshot':
            before = self.persistentIndexList()
        self.ids = new_ids
        if args.mode in ('fixed', 'early-snapshot'):
            self.changePersistentIndexList(before, [self.index(old_to_new[idx.row()], idx.column()) for idx in before])
        if args.mode != 'baseline':
            self.layoutChanged.emit()


class EquivalentProxy(C.QSortFilterProxyModel):
    def __init__(self):
        super().__init__()
        self.global_filter = ''
        self.columns = {}

    def filterAcceptsRow(self, row, parent):
        src = self.sourceModel()
        values = [str(src.index(row, col, parent).data()).casefold() for col in range(src.columnCount())]
        return (not self.global_filter or any(self.global_filter.casefold() in value for value in values)) and all(
            pattern.casefold() in values[col] for col, pattern in self.columns.items())


class EquivalentSelectionKeeper(C.QObject):
    """Mirror the production keeper's signal/ID behavior, including Select (not ClearAndSelect)."""
    def __init__(self, view):
        super().__init__(view)
        self.view, self.model = view, view.model()
        self.ids, self.current = set(), -1
        sm = view.selectionModel()
        sm.selectionChanged.connect(self.selectionChanged)
        sm.currentChanged.connect(self.currentChanged)
        self.model.modelReset.connect(self.restore)
        self.model.layoutChanged.connect(self.restore)

    def selectionChanged(self, selected, deselected):
        for selection, add in ((deselected, False), (selected, True)):
            for region in selection:
                for row in range(region.top(), region.bottom() + 1):
                    identity = self.model.index(row, 0).data(ID)
                    if add:
                        self.ids.add(identity)
                    else:
                        self.ids.discard(identity)

    def currentChanged(self, current, previous):
        if current.isValid():
            self.current = current.data(ID)

    def indexFromId(self, identity):
        return next((self.model.index(row, 0) for row in range(self.model.rowCount())
                     if self.model.index(row, 0).data(ID) == identity), INDEX())

    def restore(self):
        sm = self.view.selectionModel()
        vertical, horizontal = self.view.verticalScrollBar(), self.view.horizontalScrollBar()
        v, h = vertical.value(), horizontal.value()
        selection = C.QItemSelection()
        for identity in self.ids:
            idx = self.indexFromId(identity)
            if idx.isValid():
                selection.select(idx, idx)
        current = self.indexFromId(self.current)
        sm.select(selection, Flags.Select | Flags.Rows)
        if current.isValid():
            sm.setCurrentIndex(current, Flags.NoUpdate)
        vertical.setValue(v)
        horizontal.setValue(h)


class SortRegression(unittest.TestCase):
    def fixture(self, ids=(101, 102, 103), filtered=False, keeper=True):
        self.source = EquivalentSource(ids)
        self.proxy = EquivalentProxy()
        self.proxy.setSourceModel(self.source)
        self.proxy.setDynamicSortFilter(False)
        if filtered:
            self.proxy.global_filter = 'keep'
        self.view = W.QTableView()
        self.view.setModel(self.proxy if filtered else self.source)
        self.model = self.view.model()
        self.sm = self.view.selectionModel()
        self.keeper = EquivalentSelectionKeeper(self.view) if keeper else None
        self.events = []
        for name in ('layoutAboutToBeChanged', 'layoutChanged', 'modelReset', 'rowsInserted', 'rowsRemoved'):
            getattr(self.source, name).connect(lambda *unused, event=name: self.events.append((event, list(self.source.ids))))
        return self.model

    def tearDown(self):
        self.view.close()
        self.view.deleteLater()
        app.processEvents()

    def visible(self):
        result = []
        for row in range(self.model.rowCount()):
            idx = self.model.index(row, 0)
            result.append(idx.data(ID))
            if self.model is self.proxy:
                src = self.proxy.mapToSource(idx)
                self.assertEqual(self.proxy.mapFromSource(src), idx)
        return result

    def select(self, identities, current=None):
        for row in range(self.model.rowCount()):
            idx = self.model.index(row, 0)
            if idx.data(ID) in identities:
                self.sm.select(idx, Flags.Select | Flags.Rows)
            if idx.data(ID) == current:
                self.sm.setCurrentIndex(idx, Flags.NoUpdate)

    def check_selection(self, identities, current):
        self.assertEqual(sorted(i.data(ID) for i in self.sm.selectedRows()), sorted(identities))
        self.assertEqual(self.sm.currentIndex().data(ID), current)

    def test_single_selection_current_and_columns_survive_both_directions(self):
        self.fixture()
        self.select([101], current=101)
        refs = [C.QPersistentModelIndex(self.source.index(0, col)) for col in range(5)]
        for descending in (False, True, False):
            self.source.sortProfiles(descending=descending)
            self.view.doItemsLayout()  # Production refresh() does this; it is not notification.
            self.check_selection([101], 101)
            self.assertEqual([(i.data(ID), i.column()) for i in refs], [(101, col) for col in range(5)])

    def test_multiselection_is_preserved_without_extra_old_rows(self):
        self.fixture((101, 102, 103, 104))
        self.select([101, 102], current=102)
        for descending in (False, True):
            self.source.sortProfiles(descending=descending)
            self.check_selection([101, 102], 102)

    def test_established_global_filter_keeps_matching_identities(self):
        self.fixture(filtered=True)
        self.assertEqual(self.visible(), [101, 103])  # Establish the real proxy cache first.
        self.select([101], current=101)
        ref = C.QPersistentModelIndex(self.model.index(0, 2))
        for descending, expected in ((False, [103, 101]), (True, [101, 103])):
            self.source.sortProfiles(descending=descending)
            self.view.doItemsLayout()
            self.assertEqual(self.visible(), expected)
            self.check_selection([101], 101)
            self.assertEqual((ref.data(ID), ref.column()), (101, 2))
            self.assertFalse(self.proxy.mapFromSource(self.source.index(self.source.ids.index(102), 0)).isValid())

    def test_column_and_global_filters_are_preserved(self):
        self.fixture(filtered=True)
        self.proxy.columns = {1: 'east'}
        self.proxy.invalidateFilter()
        self.assertEqual(self.visible(), [101])
        self.select([101], current=101)
        for descending in (False, True):
            self.source.sortProfiles(descending=descending)
            self.assertEqual(self.visible(), [101])
            self.check_selection([101], 101)

    def test_empty_selection_stays_empty(self):
        self.fixture()
        self.source.sortProfiles()
        self.assertEqual(self.sm.selectedRows(), [])
        self.assertFalse(self.sm.currentIndex().isValid())

    def test_empty_single_and_already_sorted_emit_no_change(self):
        self.fixture(ids=())
        self.source.sortProfiles()
        self.assertEqual(self.events, [])
        self.source.ids = [101]
        self.source.sortProfiles()
        self.assertEqual(self.events, [])
        self.source.ids = [103, 101, 102]
        self.source.sortProfiles()
        self.assertEqual(self.events, [])

    def test_duplicate_ids_and_equal_keys_keep_distinct_persistent_rows(self):
        self.fixture((101, 102, 101, 103, 104), keeper=False)
        refs = [C.QPersistentModelIndex(self.source.index(row, row % 5)) for row in range(5)]
        before = [(i.data(ID), i.column()) for i in refs]
        for descending in (False, True):
            self.source.sortProfiles(descending=descending)
            self.assertEqual([(i.data(ID), i.column()) for i in refs], before)
            self.assertEqual(len({i.row() for i in refs}), 5)

    def test_snapshot_is_after_about_to_signal_and_before_mutation(self):
        self.fixture(keeper=False)
        created = []
        self.source.layoutAboutToBeChanged.connect(lambda: created.append(C.QPersistentModelIndex(self.source.index(0, 4))))
        self.source.sortProfiles()
        self.assertEqual(self.events, [('layoutAboutToBeChanged', [101, 102, 103]), ('layoutChanged', [103, 101, 102])])
        self.assertEqual([(i.data(ID), i.column()) for i in created], [(101, 4)])


if __name__ == '__main__':
    app = W.QApplication([])
    print(f'Python equivalent on real {binding} / Qt {C.qVersion()}, mode={args.mode}', flush=True)
    unittest.main(argv=[sys.argv[0]] + remaining, verbosity=2)
