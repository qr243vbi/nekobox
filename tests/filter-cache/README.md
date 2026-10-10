# Custom row-filter cache regression candidate

## Status and scope

Local source candidate only. No commits, pushes, or publication.

Pinned base: `5561d8a4041d9caa3e3ec2a3f8a6b8940d59966d` in
`qr243vbi/nekobox`. The original [issue #71](https://github.com/qr243vbi/nekobox/issues/71)
requested profile filtering; the maintainer [reported it available in 5.11.0](https://github.com/qr243vbi/nekobox/issues/71#issuecomment-4346203976).
This candidate addresses a current stale-cache defect discovered during review.
It is not evidence that the original feature remains unimplemented.

The production patch adds `invalidateFilter()` after changing custom filter
state in `setEnabled`, `setColumnFilter`, and `setGlobalFilter`. It does not
change the predicates or the default `enabled = false`. Disabling header
filters still clears only column filters; global search remains active.
Global search matches any column, and enabled column filters are intersected.

`invalidateFilter()` is available on the existing Qt6 baseline and on older Qt.
It deliberately avoids raising the required Qt version with the newer
`beginFilterChange` / `endFilterChange` API pair. See [Qt 6.8 documentation](https://doc.qt.io/qt-6.8/qsortfilterproxymodel.html#invalidateFilter).

## Why the current source can keep old results

- `src/gharqad/ui/mainwindow_table.cpp:168–186` (base) changes custom filter state
  without notifying `QSortFilterProxyModel`.
- `MyTableModel::capture` disables dynamic sorting/filtering and connects header
  changes to `setColumnFilter` followed by `refresh`.
- `MyTableModel::refresh` resets the source model only if its row count changes.
- `src/gharqad/ui/mainwindow.cpp:1910–1926` (base) changes the global needle,
  reinstalls the same proxy for nonempty searches, and calls `refresh`.

Changing a filter does not change the source count. Once Qt has built an index
mapping, these paths need explicit filter invalidation. Reinstalling the same
model on the view and requesting a view layout do not update custom proxy state.

## Evidence actually obtained

The installed runtime was **PySide2 with Qt 5.15.15**. The Python proxy below is
an explicitly labeled equivalent of the production proxy. Its index mapping,
cache, source model, and signals are real Qt, not a simulated cache.
The ASCII fixture avoids differences between Python case folding and QString.

Six cases keep a three-row source fixed, establish a mapping, then exercise
repeated global changes/clears, repeated column changes/clears, enable/disable
with global search active, cleared column state on re-enable, and combined
filter semantics. Every checkpoint checks visible stable IDs, mapping round
trips, excluded indexes, count=3, dynamic filtering disabled, and no source
reset/layout/data/row-change signals.

| Python-equivalent mode | Result |
| --- | --- |
| No invalidation (existing semantics) | 5 failures / 6 cases, exit 1 |
| All three setters invalidate | 6 passes / 6 cases, exit 0 |
| Omit enabled setter invalidation | 2 failures / 6 cases, exit 1 |
| Omit column setter invalidation | 3 failures / 6 cases, exit 1 |
| Omit global setter invalidation | 2 failures / 6 cases, exit 1 |

These results demonstrate the real Qt cache mechanism and exercise the intended
fix, but **do not compile or execute the production C++**. The mutation modes
alter the Python equivalent; they are not production-source mutations.

The native Qt6 test copies the production class declaration (including Q_OBJECT)
and all four production method definitions verbatim into a dependency-isolated
test target. Extraction ran for both base and candidate and saved hashes.
Compilation and execution were **blocked**: CMake is absent, no Qt6 Core
development headers/config or moc were found, and pkg-config cannot resolve
Qt6Core. Installed Qt6 runtime libraries alone are insufficient.

Application configuration was also attempted and stopped at `cmake: command
not found` (exit 127). No full application build, native C++ test, application
test suite, or GUI smoke test passed or ran. The root CMake project has no
registered CTest suite for this proxy. Production acceptance still requires
native compilation and UI integration validation.

## Run the available runtime test

From the checkout root, using a Python interpreter with a supported Qt binding:

```sh
python3 tests/filter-cache/test_qt_cache.py --invalidate none
python3 tests/filter-cache/test_qt_cache.py --invalidate all
python3 tests/filter-cache/test_qt_cache.py --invalidate global,column
python3 tests/filter-cache/test_qt_cache.py --invalidate global,enabled
python3 tests/filter-cache/test_qt_cache.py --invalidate column,enabled
```

Locally `/usr/bin/python3` provides the binding; the default `python` does not.
Expected exits are 1, 0, 1, 1, 1, respectively. A baseline failure is intentional.
No network service, NekoBox core, display server, or VPN is used by these tests.

## Run the production-source Qt6 regression

Prerequisites: Python 3, CMake >=3.16 with CTest, C++20 toolchain, and Qt6 Core
development files including moc. The standalone target requires only Qt6 Core;
it does not change the application's build configuration.

Keep an unchanged checkout of the pinned base and a separate patched checkout.
Use distinct build directories so their extracted sources and results cannot
be mixed. The test files may stay in the patched checkout:

```sh
python3 tests/filter-cache/run_cpp_test.py \
  --source-root /path/to/unchanged-base --build-dir /tmp/filter-cache-before
python3 tests/filter-cache/run_cpp_test.py \
  --source-root /path/to/patched-checkout --build-dir /tmp/filter-cache-after
```

The desired native result is red on base and green on candidate; this has **not
been established in the current environment**. The runner generates a source
manifest in each build directory. `--extract-only` verifies extraction without
claiming compilation. If the source layout changes, the narrow extraction must
be reviewed. This focused test does not replace compiling the full application.

Suggested GUI checks after a successful application build: use an unchanged
three-profile group; search Poland, Beta, empty, and no-match; change and clear
name/other-column header filters repeatedly; enable and disable header filters
while global search remains nonempty; verify visible profile identities after
every change, without adding or removing profiles.

## Source integrity

All 833 tracked blobs/symlinks were compared with the immutable Git tree.
Only `src/gharqad/ui/mainwindow_table.cpp` differs: three lines added, none removed.
No tracked modes changed. The original executable mode `100755` is preserved.
The source patch applies cleanly to the pinned original, produces the candidate
bytes exactly, and reverse-checks cleanly. The header and caller are unchanged.
See `validation/source-verification.json`, per-stage logs, and extracted-source
manifests in the accompanying validation bundle.
