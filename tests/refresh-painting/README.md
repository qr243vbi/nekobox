# Profile table painting regression

## Bounded fix

`MainWindow::refresh_proxy_list_impl` used to disable table updates before
checking the current group and before rejecting a comparison sort above 12,000
profiles. Those returns never reached the helper's matching enable. A
single-profile refresh with no current group also returned from the helper
without restoring the outer disable. A previously enabled table could therefore
stop repainting until another refresh reached the normal helper path.

Remove only that redundant outer disable. The helper already brackets its work
with `setUpdatesEnabled(false/true)` and explicitly restores updates for its
small-group missing-ID return. The sort comparator mutates no widget and does
not run the event loop; no outer suppression is needed for it.

All sorting logic, warning text, `> 12000` limit, Raw/ById handling, latency-order
calculation, signal blocking and model refresh behavior remain unchanged. This
does not introduce a general save/restore policy for caller-owned update state.
It does not claim to fix the historical sorting crashes in #115 or #126.

Qt documents the relevant widget contract at
<https://doc.qt.io/qt-6/qwidget.html#updatesEnabled-prop>.

## Run

The source-linked regression requires Python 3 and a GCC/Clang-compatible
C++20 compiler on a platform supporting `-fPIC -shared` and `ctypes` `.so`
loading. No Qt development files, network, core process, profile files or
credentials are needed.

```sh
python3 tests/refresh-painting/test_refresh_painting.py
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' \
  python3 tests/refresh-painting/test_refresh_painting.py
python3 tests/refresh-painting/mutation_checks.py
```

An optional Python Qt binding drives a real offscreen `QTableView` from the
production method's calls. Use a Python interpreter with PySide6, PyQt6, PyQt5
or PySide2 installed:

```sh
REFRESH_PAINTING_QT=1 python3 tests/refresh-painting/test_refresh_painting.py
```

For a baseline, export the unmodified `mainwindow.cpp` from the desired Git ref
and set `REFRESH_PAINTING_SOURCE=/path/to/baseline-mainwindow.cpp` when running
either mode. On base `22b5abad885dfacc4bf6784f96e4aaf01270d9cc`, eight of the
19 cases fail because painting remains disabled. All 19 pass after the fix.

## What this establishes

- Both refresh methods and their intervening latency comparator are copied
  verbatim into the fixture; a SHA-256 manifest is printed each run. The actual
  `GroupSort.hpp` is included directly.
- Coverage: missing-group full/single/direct-helper calls; all five restricted
  sort modes; descending rejection; Raw/ById for large groups; exactly 12,000
  profiles; the small missing-ID skip and its 3,000-profile boundary; empty,
  normal full/single, ascending and descending refreshes.
- Synthetic adapters supply strings, profile storage, table/model plumbing and
  warning/log recording. They validate the real C++ control flow but are not Qt
  implementations. Warning requests are recorded; the actual queued warning
  dialog and its event-loop timing are not executed. String coverage is ASCII.
- Optional Qt mode forwards painting and signal changes synchronously through
  a C callback to a live table, checks table/viewport update state, and verifies
  that an offscreen paint event is delivered by a later viewport repaint. This
  is a real widget-boundary check, not a native Qt C++ or full-app integration
  build, screenshot comparison, sorting-performance benchmark or historical
  crash reproduction.
- Five mutations must fail assertions: reintroducing the outer disable,
  rejecting exactly 12,000, continuing after a sort warning, and omitting each
  helper restore. A compilation error does not count as detecting a mutation.

This environment ran the optional boundary check using PyQt5 / Qt 5.15.15.
Native Qt6 headers/CMake and a full application build remain unverified.
