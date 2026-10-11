# Manual Trojan/VLESS TLS boolean regressions

This narrowly scoped fix makes the existing `allowInsecure` input accept only
exact `1` or `true`, matching shared TLS query parsing. Missing, empty, false,
zero and unsupported values assign false, including on reused beans. It does
not change query-key aliases, exporter format or shared TLS semantics.

## Run

Requires Python 3, a C++20 compiler, pkg-config and Qt6Core development files.
These runners install nothing and use only synthetic in-memory fixtures.

```sh
python3 tests/manual-tls-booleans/test_manual_tls.py
python3 tests/manual-tls-booleans/test_manual_tls.py --repo /path/to/checkout --ref COMMIT
python3 tests/manual-tls-booleans/mutation_checks.py
python3 tests/manual-tls-booleans/test_manual_tls.py --alias-probe
python3 tests/query-decoding/test_query_decoding.py
python3 tests/test_link_tls.py
python3 tests/test_link_booleans.py
python3 tests/ech-share-links/test_ech_share_links.py
```

`--keep-generated PATH` saves the extracted translation unit. Every run records
source hashes and Qt version. `EXTRA_CXXFLAGS` can enable sanitizers. Mutation
controls require a completed runtime assertion failure; compiler errors do not
count as killed mutations.

## Runtime coverage

- Actual complete `TryParseLink`, `ToShareLink` and `BuildCoreObjSingBox` bodies
  for Trojan/VLESS and AnyTLS, plus complete stream JSON generation, are extracted
  verbatim from the selected source. Bean field declarations and constructors,
  stream field defaults, URL/query helpers, shared TLS helpers and relevant
  JSON construction helpers also come from production source.
- Qt URL tokenization, query decoding and JSON serialization/parsing are real
  Qt6. The destination `example.invalid` is data only. No DNS, TLS, application,
  VPN, core binary, profile/settings load or network operation runs.
- Each raw input is exercised on fresh and initially-insecure beans, with AnyTLS
  as the independent full-parser control. Assertions trace field state into
  serialized/reparsed outbound JSON with global `skip_cert` false. A separate
  explicit override control confirms the existing global override still works.
- Inputs include absent/empty/bare keys, false/0, true/1, unsupported casing,
  whitespace, malformed percent sequences, percent-encoded booleans, one-pass
  decoding, duplicate-first handling and delimiter-injection isolation. Repeated
  imports check true-to-false and true-to-absent resets.
- The production change detected by these tests is reverting to nonempty-string
  acceptance. Other mutants prevent positive tests from passing accidentally,
  preserve stale true, accept only 1, normalize case/whitespace, split a decoded
  whole query, or drop the JSON flag.

## Explicit test boundaries

Bean ownership/storage and enum representation are test-only stand-ins. The
full application inheritance, enum validation, storage maps and persistence are
not compiled here. Fixtures select TCP; unrelated XHTTP/KCP serialization has
throwing boundaries, so accidental entry fails rather than simulates behavior.
The TLS constructor/runtime is never invoked: JSON construction alone does not
prove certificate checking, a successful connection or runtime TLS behavior.

## Separate known alias mismatch

Shared export writes `insecure=1`, but the manual Trojan/VLESS importer reads
only `allowInsecure`. The observational `--alias-probe` traces a fresh bean
through import, export and fresh reimport and prints JSON evidence: true does
not survive this roundtrip. AnyTLS preserves true. The probe also records both
keys and conflicting/empty combinations; it deliberately does not assert a new
precedence policy. The shared importer itself reads only `insecure`, so there
is no existing cross-alias policy to reuse. Alias support and conflict precedence
remain a separate decision, not a green regression claim from this patch.

VMess URL parsing has the same nonempty `allowInsecure` pattern and is outside
this Trojan/VLESS-only patch. These synthetic results do not reproduce or
establish the cause of issue #223, which lacks a concrete affected node/log.
