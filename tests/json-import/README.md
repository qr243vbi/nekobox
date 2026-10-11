# Standalone JSON outbound import regression

This narrow fix addresses a current, reproducible control-flow defect discovered
while reviewing [issue #148](https://github.com/qr243vbi/nekobox/issues/148).
The historical report supplies no payload, so this is **not evidence that its
original blank-profile symptom is reproduced or fully resolved**.

Base: `22b5abad885dfacc4bf6784f96e4aaf01270d9cc`.

## Contract and fix

`RawUpdater::update` recognizes a standalone JSON object and prepares a `custom`
profile with `CustomBean::core = "internal"` and the exact input text in
`config_simple`. Previously it then reached the share-link scheme guard.
Ordinary JSON has no link scheme, so that guard discarded the prepared profile
before `AddProxy`.

The fix adds the prepared JSON profile through the existing `AddProxy` boundary
and continues the pending-input loop. It requires a nonempty string `type` in
this newly enabled path: missing, null, numeric, boolean, array, object and empty
string types remain rejected. The importer is not a full sing-box schema
validator; arbitrary nonempty type strings and other fields remain core-owned.
It does not convert standalone JSON into a native SOCKS/other protocol bean or
change its display/name defaults.

Existing modes stay distinct:

- Standalone JSON, JSON strings in arrays, and `proxy` wrappers containing JSON
  use the custom/internal profile and preserve the text exactly.
- Bare outbound objects inside arrays remain ignored under the existing array
  contract. This patch does not add a new array format.
- `outbounds`/`endpoints` envelopes retain their existing `updateSingBox` route.
- Full configurations with inbounds, routing/route or DNS retain
  `internal-full` and the existing sanitizer route.
- SIP008 and share links retain their existing dispatch.
- Multiline splitting is unchanged. Its separate quoted-brace limitation is
  outside this patch.

## Run

Requirements: Python 3 and a GCC/Clang-compatible C++20 compiler (`CXX`, or `c++`).
No application/core is launched, no profile storage is accessed, and no network,
proxy connection, credentials or real user data are used.

```sh
python3 tests/json-import/test_json_import.py
python3 tests/json-import/mutation_checks.py
```

Run the same test against immutable source in a Git checkout:

```sh
python3 tests/json-import/test_json_import.py \
  --repo /path/to/nekobox --ref 22b5abad885dfacc4bf6784f96e4aaf01270d9cc
```

Native Qt6 Core mode, when development headers and pkg-config are available:

```sh
python3 tests/json-import/test_json_import.py --qt
```

Optional Qt5 fixture-parser validation, with an installed PyQt5 binding:

```sh
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' \
  /usr/bin/python3 tests/json-import/test_json_import.py --fixture-parser qt5
```

That last mode runs Qt's JSON byte parser **before** generating C++ boundary
objects; it does not turn the extracted C++ importer into a native Qt importer.
It checks that the registered fixtures agree with Qt's object/array parsing.

## What executes, and what does not

The runner copies these production bodies verbatim, without changing their
control flow:

- `RawUpdater::update`
- `RawUpdater::AddProxy`
- `JsonEndIdx` and `Disect`
- `CustomBean::BuildCoreObjSingBox`

Default mode supplies explicit ASCII string/container adapters. Python parses
only the fixture JSON and registers its values; an unregistered parser input is
an error. Qt6 mode instead uses real Qt6 containers and JSON parsing. Both modes
substitute the entity/bean factory and ownership, filter-key comparison,
base64 and Happ decoding, unrelated format parsers, the sanitizer, envelope
internals, share-link bean parsing, and logging. The actual `AddProxy` branch is
executed, but real profile comparison/serialization is not. Full-config
serialization is a placeholder in adapter mode and is not asserted as correct.

Fixtures cover standalone/whitespace/unknown-type objects, exact custom JSON
retention and custom builder extraction, malformed types and JSON, supported
array wrappers, unsupported bare array objects, full-config versus envelope
routing, SIP008, share-link acceptance/rejection dispatch, comments, separate
later JSON payloads and object ownership, and filter rejection followed by
remaining stack entries. Shared-link tests verify importer dispatch, not
protocol URL parsing. No persistence, application UI, Windows, network
operation, or historical issue resolution is established.

## Evidence from this environment

- Unchanged base: 42/51 executed checks pass, 9 expected failures. Dependent
  payload assertions are skipped when the prerequisite profile is absent.
- Candidate: 72/72 checks pass.
- Qt 5.15.15 fixture-parser mode with UBSan: 72/72; no UB report.
- Five temporary source mutations are rejected by runtime assertions: omitted
  add, malformed-type acceptance, wrong profile mode, early whole-function
  return, and bypassed `AddProxy` filtering.
- Native Qt6 execution is blocked by missing development headers/pkg-config
  metadata. The full application build attempt is blocked by missing CMake.
- Broader existing checks pass: subscription guard 27, link booleans 251, DNS
  774, stream settings 26, profile-filter 12 cases, and the existing
  Python-equivalent Qt cache diagnostic 6 cases.
- Aggregate `python3 -m unittest discover -s tests -v` runs 22 tests and reports
  one pre-existing import error: `test_subscription_update_guard` parses the
  unittest command-line arguments at import. Its standalone 27-check run passes.
  The aggregate suite is therefore **not green**.

Before broader release claims, run the native Qt6 test and normal application
build/integration checks in an equipped environment. Keep validation synthetic;
there is no need to launch a real connection or modify existing user profiles.
