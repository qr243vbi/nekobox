# ECH Trojan/VLESS share-link import regression

This addresses a bounded data-loss path relevant to
[#205](https://github.com/qr243vbi/nekobox/issues/205): Trojan/VLESS's manual
importer omits the existing `ech_config` and `query_server_name` fields that its
share-link encoder already emits.

## Prerequisite

Apply the separate single-decoding query-contract fix first. It tokenizes
encoded query boundaries, decodes field values once and protects literal
percent signs during export. Merely adding FullyDecoded to the old importer
is unsafe: the old whole-query decode can turn encoded delimiters into flags
and cause double decoding. This ECH delta uses the corrected GetQueryValue.

The local verified prerequisite production patch SHA256 is
b951d9c81488eee3bc08b375a96c111eccc754c11d05237067937dfea38248da,
based on a536846c9c74dd8fb104dce3319cbd9672920bb3. This test accompanies the
five-line ECH import delta, not an alternative query implementation.

## Existing format semantics

A nonempty `ech_config` enables ECH; missing or empty configuration does not.
Disabled profiles omit ECH config. PEM and opaque malformed text, including
literal percent signs and encoded query delimiters, are retained for downstream
validation. The existing shared helper's reused-stream behavior is preserved.
No URI keys or default TLS behavior are changed.

Enabled DNS-discovery profiles with empty ECH config cannot be represented by
this convention. Standard Xray `ech` values, YAML import, profile persistence and
network/TLS handshakes are separate work.

## Run

Requires Python3, a C++20 compiler, pkg-config and Qt6 Core development files:

```sh
python3 tests/ech-share-links/test_ech_share_links.py
python3 tests/ech-share-links/mutation_checks.py
python3 tests/query-decoding/test_query_decoding.py
python3 tests/test_link_tls.py
```

The query-decoding tests belong to the prerequisite. `--repo PATH` and
`--ref COMMIT` select alternate/immutable production source; the ECH suite
fails without the ECH import even when the query prerequisite is present.
`EXTRA_CXXFLAGS` accepts optional sanitizers. `--keep-generated /tmp/ech.cpp`
saves the extracted translation unit.

## Boundaries

The runner compiles verbatim full TrojanVLESSBean::TryParseLink and ToShareLink,
shared TLS/mux/default-field helpers, network helper bodies, GetQuery and query
utilities, plus production stream field declarations. Qt URL parsing and strings
are real. Bean ownership/storage and enum classes are explicit test-only
stand-ins. No app, database, core, subscription, real profile or network is used.

Cases cover both protocols, enabled/disabled/missing/empty ECH, multiline PEM
with Base64 +/=, opaque percent/delimiter text, query name, reused state and
adjacent TLS/transport/profile fields. Encoded allowInsecure text must stay data.
Four mutations remove import, config, query name or force unconditional enable.
