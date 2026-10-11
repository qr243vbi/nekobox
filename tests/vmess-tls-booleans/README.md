# VMess URL TLS-boolean regression

The complete VMess URL parser previously treated every nonempty `allowInsecure`
value as true. Thus `false`, `0`, malformed values and unsupported spellings
could disable certificate verification in generated TLS configuration. Missing
or empty values also left an already-true bean unchanged.

The production change is only the URL-branch assignment: accept the existing
shared TLS helper's exact `1`/`true` values and assign false otherwise. It does
not add an alias, change duplicate-key selection, or alter native JSON, V2RayN
Base64 JSON, export formats, transport, global override or default settings.

## Run

With existing Qt6Core development files and pkg-config:

```sh
python3 tests/vmess-tls-booleans/test_vmess_tls.py
python3 tests/vmess-tls-booleans/test_vmess_tls.py --ref HEAD
python3 tests/vmess-tls-booleans/mutation_checks.py
python3 tests/vmess-tls-booleans/test_vmess_tls.py --native-json-probe
```

`--repo PATH` reads production files from another checkout; `--ref COMMIT`
reads them from an immutable revision. `--keep-generated PATH` saves the
translation unit. `EXTRA_CXXFLAGS` supports instrumentation such as
`-fsanitize=undefined -fno-sanitize-recover=undefined`. No dependency download
or installation is performed. The default test requires actual Qt6Core; there
is no string/URL/JSON/Base64 adapter mode.

## Scope and positive controls

The runner extracts complete VMess `TryParseLink`, `ToShareLink`,
`BuildCoreObjSingBox`, stream `BuildStreamSettingsSingBox`, shared TLS helpers,
URL/query utilities and Base64 decoder bodies without modifying their logic.
Stream fields/defaults and VMess constructor/declarations also come from source.
Storage and unrelated enum boundaries are inert test stand-ins. Unexercised
XHTTP/KCP branches throw instead of pretending to implement transport behavior.
Synthetic TCP fixtures use `example.invalid`, no user profiles or saved settings.

The 1,321 assertions cover:

- Fresh and already-true beans; repeated true → false/absent/zero imports
- Raw, empty, absent, numeric, false, malformed, case/whitespace, percent-encoded
  and double-encoded inputs using real Qt URL parsing
- Existing Qt first-value behavior for duplicate keys and delimiter isolation
- Agreement with the unchanged shared `insecure` helper's boolean semantics
- Production TLS JSON generation, serialization and reparse; explicit positive
  true/1 and isolated global `skip_cert=true` controls
- VMess type, UUID, server, port and SNI preservation
- Fresh Base64 V2RayN JSON imports with boolean/string/number/missing insecure
  fields; existing defaults, literal text and alternate-format values preserved

The suite does not claim that VMess URL export/reimport preserves true. The
existing exporter emits `insecure=1` while the URL importer reads `allowInsecure`;
the runner prints that mismatch as an observation. Neither alias handling nor
cross-alias precedence is changed here. Base64 V2RayN JSON does not read either
insecure key at all; only its existing fresh-bean secure default is asserted.
Reused Base64 imports retain old state, outside this URL-only fix.

## Native JSON compatibility probe and known independent bug

`--native-json-probe` additionally compiles complete VMess `TryParseJson`, shared
native JSON import helpers, actual scalar/array `Data::Node` constructors and
conversions, and the same complete JSON builders. Real Qt performs JSON parsing
and serialization. A separately declared map-construction/access boundary avoids
the existing independent `Node::at(EnumFieldName)` recursion; composite quoting
and map serialization throw if reached. This is not a full ConfigData integration
test. The production copy-constructor warning is suppressed only in this mode.

On the main snapshot used for this patch, `Node::toBoolean` uses `isNumber()>0`,
so native JSON boolean `tls.insecure=true` is already lost. The probe checks
false/default/reset behavior (34 checks) and prints true handling without
pretending that it is correct. Native JSON code is unchanged by this patch.

With the separate Node boolean fix (`toNumber()>0`, draft PR #322) overlaid in
an isolated checkout, add `--expect-native-json-true` to require the native true
flag survive import and emitted JSON. This optional compatibility run has 36
checks. The flag intentionally fails twice on the unchanged main snapshot.
The Node fix is not included in this patch.

## Mutation controls

Every mutation must compile and then fail runtime assertions: original nonempty
parsing, always-false, stale-true preservation, one-only, case-insensitive,
whitespace-trimming, decode-before-query-split, and emitted insecure flag loss.
Compile errors and absent summaries are not accepted as mutation kills.

These are focused, inert source-method tests. They do not build or run the
NekoBox application, start a proxy core, access the network, attempt TLS,
load/edit user settings, or exercise application lifecycle. Generated insecure
JSON is established; live connection or certificate behavior is not claimed.
