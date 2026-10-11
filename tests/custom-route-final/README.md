# Preserve explicit full-config default outbounds

A narrow data-preservation defect found while auditing
[issue #241](https://github.com/qr243vbi/nekobox/issues/241).
Baseline: `a536846c9c74dd8fb104dce3319cbd9672920bb3`.

## Contract and scope

Full JSON imports intentionally produce one `internal-full` profile. The normal
builder loads its own config rather than applying the app's ordinary routing
profile default. `ProxyEntity.hpp` documents full configs as bringing their own
outbounds and `route.final`. TUN-specific app rules can be layered onto these
configs, but this does not replace their final outbound.

Before this patch, `sanitizeSingBoxConfig` unconditionally replaced `route.final`
with the first non-direct/block/DNS outbound tag. Thus an explicitly selected
`direct`, a later proxy, selector, or endpoint could be silently changed at
import. Only synthesize the default when the existing value converts to an
empty string, preserving every supplied nonempty string exactly.

This deliberately preserves NekoBox's existing fallback policy: missing, empty
and non-string `final` values still select the first non-direct/block/DNS
outbound; an untagged eligible outbound receives `proxy`; no eligible outbound
means no replacement. This is **not** the same as sing-box's native empty-final
contract, which uses the first outbound. See the
[official route reference](https://sing-box.sagernet.org/configuration/route/#final).
Changing that fallback or rejecting malformed values is outside this patch.
Unknown nonempty tags are preserved for normal core validation rather than
silently repaired. The tests do not validate core acceptance.

This does not resolve all of #241: expanding full subscriptions into individual
nodes or making the app's Default outbound override full configs remains a
product decision. It does not duplicate PR #301's standalone JSON dispatch fix.
The separate speed-test builder intentionally overrides full-config defaults
and is unchanged. Existing stored profiles are not migrated or repaired.

## Run

Use Python 3, a C++20 compiler and native Qt6Core development files:

```sh
python3 tests/custom-route-final/test_route_final.py
python3 tests/custom-route-final/mutation_checks.py
```

An already-extracted Qt6 distribution can be supplied with `--qt-root PATH`
(the Debian-style prefix containing `usr/include/x86_64-linux-gnu/qt6` and
`usr/lib/x86_64-linux-gnu`). `QT6_ROOT` is also accepted by the main runner.
No dependencies are downloaded or installed by these tests.

Read immutable source for the red check:

```sh
python3 tests/custom-route-final/test_route_final.py \
  --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' \
  python3 tests/custom-route-final/test_route_final.py
```

`--source FILE` supports temporary whole-source mutations. `--keep-generated
FILE` keeps the compiled translation unit for inspection.

## What executes

The runner extracts the complete, unmodified production bodies of:

- `jsonFirstString`, `jsonFirstInt`
- `migrateDnsServerToSingBox112`, `convertV2RayNToSingBox`
- `normalizeHysteriaOutbound`, `sanitizeSingBoxConfig`
- the `addFullJsonProxy` lambda inside `RawUpdater::update`

QString, QUrl, JSON parsing, object/array manipulation and compact serialization
are native Qt6. Only the app settings object, entity/bean factory, unlock and
AddProxy destination are synthetic in-memory substitutes. No application, core,
profile store, GUI, socket, proxy, DNS lookup or system routing action executes.
The full RawUpdater dispatcher and ConfigBuilder do not execute in this new
fixture. Existing standalone-import regression separately checks dispatch with
its documented dependency adapters.

The 552 assertions cover outbound-order rotations; direct/block/DNS defaults;
selectors, later proxies and endpoint references; Unicode, unknown and
whitespace-containing tags; unchanged outbound graphs, routing rules and rule
sets; repeated sanitation; implicit defaults, malformed values, untagged and
no-eligible-outbound cases; actual V2RayN conversion; legacy `routing` precedence;
and successive one-profile imports with real serialized JSON and independent
ownership. Synthetic test data only.

## Results

- Native Qt 6.8.2: candidate 552/552; baseline 487/552, with 65 expected failures.
- UBSan: 552/552, no sanitizer report.
- Independent review found no blocking issue. An additional 3,570 native Qt6
  differential cases verify that complete sanitizer output differs from baseline
  only where preserving an explicit nonempty final requires it.
- Seven mutations rejected by runtime assertions: unconditional overwrite,
  removed fallback, missing-only fallback, last-eligible fallback, direct as
  fallback, overwrite after sanitation, and skipped sanitation during import.
- Neighboring native Qt6 checks: DNS 774/774; link booleans 251/251; TLS 91/91.
- Existing JSON import adapters: 72/72 plus five mutations.
- Existing subscription guard: 27/27.
- Python compilation and `git diff --check` pass.

No full application or Windows build was run; CMake is unavailable. No core
config validation, networking, GUI integration or disk persistence was tested.
The unrestricted aggregate suite was not rerun because it includes lifecycle
investigations excluded from this audit's authorization. Focused success is not
a green aggregate-suite or full-application claim.
