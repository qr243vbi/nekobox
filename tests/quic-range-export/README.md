# QUIC authority-range export regressions

This is an exporter-only fix for `QUICBean::ToShareLink`, based on upstream
main `1c35d5fed1f167dcdb4c549bc2079dc5dd0a1da4`.

## Two failures of the old replacement

The exporter puts a temporary `:0` in the URL authority because QUrl supports
only scalar ports. It previously replaced every `:0?` in the serialized URL.

1. **Queryless Hysteria2 ranges were lost.** A synthetic bean containing host
   `example.invalid`, auth `synthetic-auth`, and range `443:445` exported as
   `hy2://synthetic-auth@example.invalid:0`. Adding a display name produced
   `:0#synthetic`. Both must retain the `443-445` authority range.
2. **Matching query/fragment text was overwritten.** For example, a display
   name `marker:0?tail` could become `marker:443-445?tail`. The same global
   replacement affected a query value containing `:0?` in HY1 or HY2.

The fix calculates the end of the fully encoded authority and replaces only
its final `:0`. It does not depend on whether a query or fragment follows it,
search other URI components, change their encoding, or change range grammar.
The `3` in the position calculation is the existing `://` scheme separator.
Both range-export branches explicitly install port zero; neither sets a path.
Scalar ports and TUIC take the unchanged serialization path.

## Run

Requires Python 3, a C++20 compiler, pkg-config and Qt6 Core development files.
No installation, network or real profile data is used. Standard pkg-config
sysroot settings support an already-extracted Qt SDK.

```sh
python3 tests/quic-range-export/run_test.py
python3 tests/quic-range-export/run_test.py --repo /path/to/repo --ref BASE_COMMIT
python3 tests/quic-range-export/run_test.py --keep-generated /tmp/quic-export.cpp
python3 tests/quic-range-export/mutation_checks.py
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' python3 tests/quic-range-export/run_test.py
```

Every run prints the production source hashes. Mutation compilation failures
are errors, not successful mutation detection.

## Meaningful scenarios

- HY2 queryless, fragment-only, query-only and query-plus-fragment exports
- HY1's required bandwidth query plus normal optional-query variants
- Single range, scalar list, mixed list/ranges, equal endpoints and edge ports
- DNS, IPv4, IPv6, IDN, Unicode and scoped IPv6 authority encoding
- Username-only auth, username/password, reserved characters, literal percent
  text and Unicode credentials
- Both null and non-null empty QString credentials, which Qt serializes
  differently and this patch preserves
- Adversarial `:0`/`:0?` markers in credentials, IPv6, query data and fragments
- Exact scalar-port output controls and a TUIC control
- Unmodified synthetic bean fields after export
- Existing empty/malformed-host behavior and unvalidated malformed range text

Verified on Qt 6.8.2: 22,115 assertions; baseline 2,533 failures, candidate zero;
UBSan zero failures; all five deliberate mutations compile and fail assertions.

Qt's existing setter sequence clears rejected hostnames before serialization,
so malformed stored hosts may produce a URL with an empty host. This change
neither adds nor repairs host validation. Malformed stored ranges retain their
existing colon-to-hyphen conversion and direct insertion; no new validation is
implied by these compatibility cases.

## Fidelity and limits

The harness extracts and compiles the complete production QUIC exporter,
constructor, field declarations/defaults, query/default-field/network helpers
and their string utilities against native Qt6. Bean/entity ownership and enum
storage are explicitly test-only shells. Exact authority expectations and key
regressions use literal goldens; query/fragment expectations use independent Qt
serialization. Synthetic beans are constructed directly. No URI importer is
compiled or called, so this evidence does not depend on an unmerged parser fix.

No combined importer/exporter roundtrip is claimed. The separate authority-range
import work remains separate. No app, GUI, core, persistence, network, real
subscription or user setting is opened. Passing this focused suite is not a
full application build, Windows verification or live-connectivity result.
