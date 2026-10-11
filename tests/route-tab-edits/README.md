# Route editor tab-state regression

Baseline: `a536846c9c74dd8fb104dce3319cbd9672920bb3`.

## Production change

Basic's nine text editors now mark their draft dirty when their text changes.
A tab change does not reload a dirty draft from the older routing model.
Committing an untouched Basic view is a no-op, so Save and tab switches cannot
rebuild stale Basic values over newer Advanced changes. Loading the model and
successfully committing a draft clear the dirty flag; validation errors retain
it so the text remains available for correction.

This is a dialog-state change in `RouteItem.cpp` and `RouteItem.h`. It does not
change routing execution, networking, parsers, storage, or update behavior.
A deliberate Basic edit retains the existing `UpdateSimpleRules` rebuilding
semantics. Custom rules are not rewritten by that method.

## Native test

Requirements: Python 3, C++20 compiler, Qt6 Widgets development files, `uic`,
`moc`, and `pkg-config`. No CMake, running NekoBox process, core, or network
connection is needed for these tests.

```sh
python3 tests/route-tab-edits/run_native_test.py --build-dir /tmp/route-tab-test
python3 tests/route-tab-edits/mutation_checks.py --logs /tmp/route-tab-mutations
```

An optional `--qt-prefix /path/to/extracted-debian-packages` selects development
files extracted from Debian amd64 packages without installing them. Their Qt
runtime dependencies must already be available. Both commands accept this
option. `CXX` selects a compiler executable (default `g++`).

To compare the immutable baseline, pass a separate checkout or archive root:

```sh
python3 tests/route-tab-edits/run_native_test.py \
  --source-root /path/to/baseline \
  --build-dir /tmp/route-tab-baseline
```

`--extract-only` emits the generated source and SHA256 manifest without
compiling. Every build directory contains `source-manifest.json`, generated
C++, the `uic` header, and the test executable. Mutation checks use temporary
source copies and require an actual failed test assertion, not a compiler or
harness error, for each mutation to count as caught.

## What runs

The harness copies these production fragments verbatim into a focused native
fixture:

- Basic widget construction/replacement, tab-change connection, Advanced text
  input connections, and Save/Cancel button connections
- `splitSimpleRules`, `loadSimpleBoxes`, `commitSimpleBoxes`, `accept`, and
  `updateRouteItemsView`
- Route rule fields, enums, copy constructors, `set_field_value`, JSON rule
  construction and `isEmpty`, plus `GetSimpleRules`, `UpdateSimpleRules`, and
  their simple-rule parsing helpers

It compiles the production `AutoCompleteTextEdit` source unchanged and uses
`uic` on the full production `RouteItem.ui`. Qt6 tab widgets, text editors,
lists, signal delivery, JSON/containers, dialog acceptance/rejection, and the
`settingsChanged` signal are real native C++/Qt6. No Python Qt or hand-written
Qt container/widget substitute is used.

The constructor is a focused assembly, not the complete application
constructor. `updateRuleSection` and `updateRulePreview` are no-op display
boundaries. Network updates, profile lookup, rule-set name resolution, warning
presentation, and note-file storage are synthetic or excluded. Advanced test
edits use the production text-change connection and `set_field_value`, but do
not validate the complete Advanced display/selection pipeline. Tests invoke
text setters and real buttons/tab changes; they do not validate keyboard
completion behavior or rendering.

“Save/reopen” means receiving the real `settingsChanged` signal and constructing
a new fixture dialog from the emitted in-memory chain. Disk persistence and
the outer route-manager integration are not tested. Cancel cases verify that
the original chain/rules remain unchanged and no Save signal is emitted.

The 27 cases cover each editor signal independently; all nine boxes across
General, Notes, and Advanced; repeated transitions; Notes text retention;
Save/reopen from all four tabs; Advanced edits and Advanced-only fields when
Basic is untouched; later intentional Basic edits; ownership after successful
commit; two Cancel paths; clearing Basic rules; and invalid-text retention and
correction on a chain that also contains a valid custom rule.

## Observed results

Native Qt 6.8.2, g++ 14.2.0, Linux/offscreen:

- Immutable baseline: 27 cases, 22 assertion failures, exit 1
- Candidate: 27 cases, 0 failures, exit 0
- Production-source mutations: 15/15 caught
- Independent review found no introduced production defect

The mutations remove either guard, omit load/success resets, discard dirty
state despite validation errors, make the chain copy shallow, and omit each of
the nine individual text-change connections. The unmodified autocomplete
source emits its existing `-Wparentheses` compiler warning; it is retained in
the test logs.

## Remaining boundaries and existing limitations

This is a native extracted-code regression, not a full application build or
end-to-end GUI test. Full configuration on the validation machine remained
blocked by missing CMake. Broader standalone diagnostics were also run;
existing unrelated limitations were recorded rather than reported as green:

- `unittest discover -s tests -v` collides with the module-level argument parser
  in `test_subscription_update_guard.py`; its standalone run passes
- `tests/json-import/test_json_import.py --qt` redeclares Qt's `QObject` in its
  existing support fixture; its default adapter mode separately passes 72/72 checks
- `tests/refresh-painting/test_refresh_painting.py` uses an existing synthetic
  `Model` without the newly required `sortProfiles` member

Review reproduced the following pre-existing behavior on both baseline and
candidate, outside this bounded fix:

- Any intentional Basic edit still rebuilds all simple actions, so an
  Advanced-only field can be dropped even when its own action was not edited
- Invalid Basic parsing partially replaces the model and still permits entering
  Advanced, where simultaneous edits can later be replaced by a Basic commit
- If invalid parsing removed a chain's only simple rule, the existing empty-chain
  check runs before committing corrected Basic text and blocks immediate Save

Do not interpret this patch as transactional validation, comprehensive invalid
input recovery, or preservation of Advanced-only fields after an intentional
Basic edit.
