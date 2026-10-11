# Route editor tab-state regression

Original tab-state baseline: `a536846c9c74dd8fb104dce3319cbd9672920bb3`.
Validation-state follow-up baseline: `1c35d5fed1f167dcdb4c549bc2079dc5dd0a1da4`
(tree `36053b0d4e227eb767aaf22b09ed2324518246e3`).

## Production change

Basic's nine text editors now mark their draft dirty when their text changes.
A tab change does not reload a dirty draft from the older routing model.
Committing an untouched Basic view is a no-op, so Save and tab switches cannot
rebuild stale Basic values over newer Advanced changes. Loading the model and
successfully committing a draft clear the dirty flag; validation errors retain
it so the text remains available for correction.

The follow-up validates all three actions on a copied routing chain before
replacing any live dialog rules. Invalid drafts remain dirty and retain their
text and the prior model. An attempted Advanced transition returns to Basic
until the draft is valid, preventing two simultaneous editable sources of
truth. Save commits the draft before checking for an empty profile, so it
validates the prospective rules and corrected drafts can recover. Empty-profile
rejection restores the old rules and dirty flag so the Advanced selection stays
valid and the rejected Basic text remains available for correction.

This is a dialog-state change. The original change touched `RouteItem.cpp` and
`RouteItem.h`; the validation follow-up changes only `RouteItem.cpp` in
production. It does not change routing execution, networking, parsers,
storage, or update behavior.
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
constructor. `updateRuleSection` executes the production selection lookup behind a
test-only bounds-checking recorder; the rest of its display work and
`updateRulePreview` are no-op boundaries. Network updates, profile lookup, rule-set name resolution, warning
presentation, and note-file storage are synthetic or excluded. Advanced test
edits use the production text-change connection and `set_field_value`, but do
not validate the complete Advanced display/selection pipeline. Tests invoke
text setters and real buttons/tab changes; they do not validate keyboard
completion behavior or rendering.

“Save/reopen” means receiving the real `settingsChanged` signal and constructing
a new fixture dialog from the emitted in-memory chain. Disk persistence and
the outer route-manager integration are not tested. Cancel cases verify that
the original chain/rules remain unchanged and no Save signal is emitted.

The original 27 cases cover each editor signal independently; all nine boxes across
General, Notes, and Advanced; repeated transitions; Notes text retention;
Save/reopen from all four tabs; Advanced edits and Advanced-only fields when
Basic is untouched; later intentional Basic edits; ownership after successful
commit; two Cancel paths; clearing Basic rules; and invalid-text retention and
correction on a chain that also contains a valid custom rule.

The 21 additional validation-state cases cover an invalid line in each editor,
atomicity across different actions, valid commit → invalid Save → corrected
Save with a sole simple rule through every tab, rejected Advanced transitions
from General/Basic/Notes, valid Advanced edits after correction, repeated
invalid Save followed by Cancel, clear-only-rule rejection and recovery, and
a new valid Basic rule after the previous model is empty. The last case uses
a synthetic boundary for Advanced deletion; it does not execute the deletion
button handler. Another case retains an actual Advanced rule selection across
empty-profile rejection, General/Notes/Basic navigation, and corrected Save;
the fixture records any stale selection before an unsafe access can occur.

## Observed results

Validation-state follow-up, native Qt 6.8.2, g++ 14.2.0, Linux/offscreen:

- Current-main baseline with original suite: 27 cases, 0 failures, exit 0
- Same baseline with expanded suite: 48 cases, 20 assertion failures, exit 1
- Candidate with expanded suite: 48 cases, 0 failures, exit 0
- Production-source mutations: 21/21 caught

The 20 failures are overlapping behavioral cases, not 20 distinct defects.
The six added mutations reintroduce a partial commit, permit Advanced while
a draft is invalid, omit application of a valid candidate, or move the
empty-profile check before the draft commit, or omit restoring the model or
dirty flag after empty-profile rejection. Every mutation must compile and
produce a failed native test assertion to count as caught.

Original tab-state verification, native Qt 6.8.2, g++ 14.2.0, Linux/offscreen:

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

The original tab-state review reproduced partial mutation during invalid
Basic parsing and blocked recovery after that mutation emptied the model.
This follow-up covers those invalid-draft paths. It also rejects an empty
profile created by clearing its only Basic rule; the earlier ordering could
accept that empty result because the check ran before the commit.

Intentional successful Basic edits still rebuild all simple actions. Whether
to preserve Advanced-only fields after an intentional Basic edit is unchanged
product policy and outside this patch. The `UpdateSimpleRules` parser itself
still has its previous behavior; atomicity is implemented at the dialog's
whole-draft commit boundary.

Save metadata and note-file timing, rule syntax policy, full Advanced display
helpers, outer route-manager persistence, and the full application are not
validated by this fixture. No real application, core, network request,
settings store, lifecycle test, or memory diagnostic is run. No tools are
installed. The full suite is deliberately not run for this follow-up's bounded
inert route-editor scope; the broader historical results above are not a new
full-suite pass. CMake remains unavailable in this verification environment.
