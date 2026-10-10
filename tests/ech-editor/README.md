# ECH editor binding regression

Related to [issue #205](https://github.com/qr243vbi/nekobox/issues/205).
Baseline: `22b5abad885dfacc4bf6784f96e4aaf01270d9cc`.

## Scope

The production patch changes three lines in `dialog_edit_profile.cpp`:

- Seed the ECH input dialog from `CACHE.ech_config`, rather than the unrelated
  certificate cache. Accepting an unchanged dialog must preserve the ECH text.
- Load the existing editable Query Server Name field from
  `stream->query_server_name` and save its current text back to the stream.

Loading QSN even for Naive preserves an existing value when its widget is
hidden. Visibility rules, model defaults, ECH enablement, TLS verification,
certificate editing, and cache acceptance/cancellation rules are unchanged.
There is no persistence, JSON/Clash/link import, group-apply feature, DNS, or TLS
handshake change. These source defects are established independently of the
historical reporter's unspecified subscription and failure.

## Run

Use Python 3 with one supported Qt binding: PySide6, PyQt6, PySide2, or PyQt5.
The test defaults to Qt's offscreen platform and uses synthetic text only.
It makes no network request and starts no NekoBox/core process or tunnel.

```sh
python3 tests/ech-editor/test_qt_bindings.py -v
python3 tests/ech-editor/mutation_checks.py
```

For the unchanged baseline, pass a Git repository that contains the revision:

```sh
python3 tests/ech-editor/test_qt_bindings.py \
  --repo /path/to/git-checkout \
  --ref 22b5abad885dfacc4bf6784f96e4aaf01270d9cc -v
```

A baseline failure is expected. `--repo` without `--ref` reads working files.
Mutation checks use temporary copies and leave the candidate source untouched.
The optional `--logs /path/to/output` saves each mutation's full output.
Missing Python Qt bindings are an error, not a successful skipped test run.

## What is actually tested

This is a **source-linked Python bridge with real Qt widgets**, not execution
of the production C++ editor. The bridge reads the current source's binding
fields, setters/getters, cache assignments, and both complete input-dialog
handlers. The small handlers must match an explicitly supported grammar, or
the bridge fails. Model defaults and QSN widget properties are also read from
the production files. It uses actual `QLineEdit`, `QCheckBox`, and modal
`QInputDialog.getMultiLineText`; a Qt timer inspects and accepts/rejects each
modal dialog through the real event loop.

The ten cases cover:

- Different certificate/ECH sentinels and multiline ECH text
- Reopen and unchanged acceptance, edit, clear, and cancellation
- Certificate editing remains independent of ECH
- QSN load, unchanged save, edit, clear, and reopen
- Preservation of QSN when its Naive widget is hidden
- Disabled and query-only ECH; existing enable/insecure flag combinations
- Empty model defaults without copying certificate text into ECH

Five mutations alter actual temporary production-source copies: restore the
wrong ECH initializer, omit QSN loading, omit QSN saving, invert the ECH
acceptance guard, or write ECH text to the certificate cache. Each must fail
with test assertions, rather than a broken harness or import error.

## Observed verification and limits

On the validation machine, `/usr/bin/python3` supplied PySide2 / Qt 5.15.15:

- Immutable baseline: 6 failing tests out of 10, exit 1
- Candidate: 10 passing tests, exit 0
- Source mutations: 5/5 caught by failing assertions

Qt's offscreen plugin prints its usual `propagateSizeHints()` warnings. They
are retained in the logs. Runtime/cache directory warnings were avoided by
using dedicated temporary XDG directories for validation.

The bridge only selects direct relevant bindings from `typeSelected` and
`onEnd`; it does not execute arbitrary C++ control flow in those methods. Its
Naive case reproduces the current visibility block for a TLS-capable profile.
It does not instantiate the complete application `.ui`, validate Qt6 C++
compilation, test the outer profile dialog's rejection/validation, test
persistence, or exercise profile type switching. Review the bridge when the
production control flow or field types change.

Native application configuration was attempted and blocked by missing CMake
(exit 127); `pkg-config` also could not find Qt6 Widgets development files.
No native C++ editor/full-application build or GUI integration success is
claimed. Before release, build with the repository's normal Qt6 toolchain and
repeat the same edit/reopen/cancel/hidden-field flows in the actual profile
editor. The repository's other standalone diagnostics are complementary and
do not remove this limitation.
