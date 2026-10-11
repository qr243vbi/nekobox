# Multiline JSON subscription splitting regression

This fixes a current, independently reproduced parser defect on
`a536846c9c74dd8fb104dce3319cbd9672920bb3`. It was investigated alongside
[issue #148](https://github.com/qr243vbi/nekobox/issues/148), but that historical
report contains no payload or logs. These fixtures do **not** establish that
its original Windows symptom is reproduced or fully resolved.

## Defect and bounded fix

`RawUpdater::update` falls back to `Disect` for multiple JSON objects and mixed
JSON/share-link input. `JsonEndIdx` previously counted braces inside quoted
strings as structural delimiters. A valid object such as
`{"type":"socks","tag":"{"}` followed by another object was dropped; a quoted
closing brace could cut an object short. Some pretty-printed valid inputs then
repeatedly requeued a truncated, invalid object.

The fix tracks quoted-string state and escapes before counting braces. Escaped
quotes, odd/even runs of backslashes, quoted keys and values, nested objects,
and arrays inside objects retain the correct boundary. `Disect` itself and the
normal JSON/format dispatch stay unchanged.

Independent review exposed a second requirement directly caused by making
boundaries correct: a malformed multiline object containing a quoted opening
brace, for example a trailing-comma object, previously lacked a detected end
and was dropped. With the boundary fixed, it can reach the existing unchanged
input retry loop. A three-line guard now drops a sole split result whose
trimmed text equals the **exact current input**. This terminates both that
newly reachable case and existing identical-input retries, while allowing
whitespace normalization to make progress once. Comparing both sides after
trimming would incorrectly drop share links ending in a newline; regression
fixtures cover direct input, JSON array strings and `proxy` wrappers.

This is not a new JSON validator or a general malformed-input recovery policy:
Qt still owns syntax validation. Balanced invalid objects are rejected;
complete valid neighbors continue. Incomplete objects/unterminated strings
remain incomplete and the splitter retains only the completed prefix. No
attempt is made to guess a new object boundary inside an unterminated string.

## Run

Requirements: Python 3, a GCC/Clang-compatible C++20 compiler (`CXX`, or `c++`),
and Qt6 Core development headers/libraries discoverable by `pkg-config`.

```sh
python3 tests/json-splitter/test_json_splitter.py
python3 tests/json-splitter/mutation_checks.py
```

To compare the same test against immutable production source:

```sh
python3 tests/json-splitter/test_json_splitter.py \
  --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
```

`--repo PATH` chooses another source tree; test fixtures remain those beside the
runner. `--keep-generated PATH` preserves the generated C++ translation unit.
`--splitter-only` skips the isolated import-case group, retaining the main
boundary/compatibility checks (which also include a few bounded import checks).
`EXTRA_CXXFLAGS` supplies optional compiler flags. No software is downloaded.

## Coverage and limits

The runner compiles these complete production bodies verbatim:

- `JsonEndIdx` and `Disect`
- `RawUpdater::update` and `RawUpdater::AddProxy`

Real Qt6 supplies UTF-16 strings, JSON parsing/serialization, and containers.
The test substitutes entity/bean creation and ownership, profile filter keys,
base64/Happ decoding, unrelated format/protocol parsers, full-configuration
sanitization, envelope internals, fixup and logging. It tests dispatch and
payload retention, not those substituted components. No application, actual
profile storage, proxy core, network connection or VPN is started.

Fixtures include compact and pretty JSON; quoted braces in keys and values;
escaped quotes and backslash parity; nested arrays/objects; Unicode and
nonzero UTF-16 offsets; 1,554 exhaustively generated short strings; adjacent
objects; mixed share links; comments; CRLF/blank lines; incomplete tails;
invalid types/syntax; outbound/endpoint/full-configuration dispatch; and
whitespace normalization. Forty importer cases run in separate subprocesses
with two-second limits so broken baseline retries cannot stall the suite.
The ten malformed cases include both newly reachable and previously existing
no-progress inputs, alone and surrounded by valid objects.

Validation on Qt 6.8.2:

- Unchanged baseline: 2,988/7,645 main checks and 14/40 isolated import
  cases pass; 12 cases time out. Main totals differ because parsing assertions
  run for each actual split part and dependent checks require a created profile.
- Candidate: 7,970/7,970 main checks and 40/40 isolated import cases pass.
- Independent review: 10,002/10,002 Python-serialized randomized boundary
  fixtures pass, versus 744/10,002 on baseline; the separate malformed
  trailing-comma reproducer terminates with no imported profile.
- Quote-state fix without no-progress guard: 30/40 isolated cases pass; all
  ten malformed cases time out. This demonstrated the guard requirement
  before its implementation.
- Five mutations fail runtime assertions: ignoring string state, ignoring
  escapes, never clearing escape state, never closing strings, and an
  overaggressive trimmed-to-trimmed no-progress comparison.
- Existing standalone JSON import: 72 checks and five mutations pass in its
  explicit adapter mode. Existing ECH JSON import: 1,248 adapter checks pass.
- Native Qt6 neighboring checks pass: DNS 774, link booleans 251, TLS 91.

These focused checks are not a complete application test suite. A full CMake
configure/build was blocked by missing CMake in the validation environment.
No Windows/GUI integration, real fallback-parser integration, or historical
issue resolution is claimed.
