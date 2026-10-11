# Profile-sort model notification regression

Pinned baseline: `22b5abad885dfacc4bf6784f96e4aaf01270d9cc`.

## Defect and fix

Header clicks call `MainWindow::refresh_proxy_list_impl(-1, action)`. Previously
this sorted the current group's profile IDs in place. `MyTableModel::refresh()`
only resets the model when the row count changes, so an ordinary sort emitted no
model notification. A table layout refresh cannot repair source/proxy index
identity or cached filter membership.

`MyTableModel::sortProfiles()` computes a row permutation without modifying live
data. When that permutation changes the ID order, it emits
`layoutAboutToBeChanged`, captures `persistentIndexList()` **after** the signal,
installs the order, remaps every persistent index's row while retaining its
column, and emits `layoutChanged`. The row permutation also handles duplicate
IDs without collapsing their persistent indexes. Empty, single-row, and
unchanged-ID orders do not emit a structural change.

The caller's comparator, ascending/descending behavior, 12,000-row guard, Raw
and ById no-op behavior, save behavior, and painting calls remain unchanged.
The existing ID-based `SelectionKeeper` and generic `refresh()` are unchanged.

[Qt's model contract](https://doc.qt.io/qt-6/qabstractitemmodel.html#layoutChanged)
requires the layout pair and persistent-index repair. Capturing indexes before
the first signal misses indexes created by connected views/proxies during that
signal. Adding layout signals without remapping is also wrong: with the current
keeper it can leave both the old row and the restored identity selected.

## Run the real-Qt Python equivalent

This test uses actual Qt models, views, selection models, persistent indexes,
signals and filter caches. Its source model, matching logic and selection keeper
are Python equivalents. It **does not execute the production C++** or the full
application. The available local run used PySide2 5.15.16 / Qt 5.15.15; NekoBox
builds against Qt6. Native Qt6 confirmation is still required.

```sh
QT_QPA_PLATFORM=offscreen /usr/bin/python3 tests/profile-sort/test_qt_sort.py --mode fixed
QT_QPA_PLATFORM=offscreen /usr/bin/python3 tests/profile-sort/test_qt_sort.py --mode baseline
QT_QPA_PLATFORM=offscreen /usr/bin/python3 tests/profile-sort/test_qt_sort.py --mode signals-only
QT_QPA_PLATFORM=offscreen /usr/bin/python3 tests/profile-sort/test_qt_sort.py --mode early-snapshot
```

Observed results (8 tests per mode): fixed passes 8/8; baseline fails 6;
signals-only fails 5; early-snapshot fails 2. The failing modes deliberately
omit one or more parts of the notification contract. Tests check single/multiple
and empty selection, current identity, all columns, both sort directions,
active global/column filters with an already-established cache, duplicate IDs,
equal keys, no-op orders, and indexes created by about-to-change slots.
The duplicate-ID case disables SelectionKeeper and verifies distinct persistent
indexes only. Its pre-existing ID-based restoration does not distinguish duplicate
occurrences; normal Group::AddProfile rejects duplicate IDs.
Python's stable sorting of ties is not evidence of the production C++ sort's
relative ordering of equal keys; these tests only require identity preservation.

## Run unchanged-production extraction against Qt6

Prerequisites: Python 3, CMake >=3.16 with CTest, a C++20 compiler, and Qt6 Core,
Widgets and Test development packages including moc. Run the candidate's test
runner against separate unchanged and patched source trees:

```sh
python3 tests/profile-sort/run_cpp_test.py \
  --source-root /path/to/unchanged-base --build-dir /tmp/profile-sort-before
python3 tests/profile-sort/run_cpp_test.py \
  --source-root /path/to/candidate --build-dir /tmp/profile-sort-after
```

The desired native result is a failing baseline and passing candidate. **This
has not been established locally:** CMake and Qt development tools are absent.
`--extract-only` has passed on both trees, and generates source hashes.

The extraction copies the production MainWindow sort caller (including the
actual comparator and guard), refresh-data path, model ID/count/refresh methods,
new model helper when present, and complete SelectionKeeper and ColumnFilterProxy
class declarations and definitions verbatim. A small synthetic domain supplies
in-memory groups/profiles, display data, and unrelated header geometry. Qt is
real, not mocked. No settings, database, persistence, core, VPN or network service
is touched. This focused native test does not replace a full application build.

The C++ suite has 10 cases, including all five actual header comparators and the
oversized-sort guard, with QSignalSpy checks for reset/layout behavior.

## Limits

This fixes the same-count **header-sort** notification path. It does not fix or
verify same-count group switching, profile insertion/deletion, drag/drop,
concurrent updates, changing filter text/selection policy, or statistics-based
filter reevaluation. It is not a reproduction or explanation of the historical
crashes in issues #115/#126. No claim is made about the historical post-deletion
selection report. The separate painting-suppression fix is not included.

An offscreen synthetic check is not a full native Qt6 application run. Before
release, run the native target and build the application; then test a small
synthetic group in the GUI with selected rows, header sorts, an active search,
and multiple selected rows. No private profiles are necessary.
