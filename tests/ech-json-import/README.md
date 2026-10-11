# Nested ECH JSON import regression

This focused, offline test covers the JSON field mismatch associated with
[issue #205](https://github.com/qr243vbi/nekobox/issues/205). It does not claim to
resolve every report in that issue.

## Import policy

- A present `tls.ech` key takes precedence over the old flat fields, including
  when the nested value is null or malformed.
- Nested `enabled` accepts a boolean. Missing means false. Explicit false stays
  false even if legacy `ech_config` is also present.
- Nested `config` accepts a string or an array containing only strings. Arrays
  are joined with newlines; import preserves the supplied text without base64
  decoding or PEM interpretation. Missing/empty config permits query-only ECH.
- Nested `query_server_name` accepts a string, including empty. Missing means
  empty. Valid config/query values are retained even when ECH is disabled.
- Wrong nested types, including explicit null for these supported fields, reject
  the profile. They must not silently turn requested ECH off or turn malformed
  inline config into DNS-only ECH. Validation precedes assignment in `add_tls`.
- Only when `tls.ech` is absent does the previous flat
  `tls.ech_config`/`tls.query_server_name` behavior apply, unchanged.

The helper uses `Data::Node::getBoolean()` for the new field. The current
`toBool()`/`toBoolean()` implementation does not correctly read boolean tags;
changing that shared conversion or other existing TLS fields is outside this fix.

Rejection is propagated by eight existing JSON bean callers: AnyTLS, Http,
Juicity, Naive, ShadowTLS, TrojanVLESS (both protocols), TrustTunnel and VMess.
TrustTunnel's existing YAML parser also calls this same JSON helper. Its guard
is necessary to prevent a helper rejection being reported as successful YAML
parsing with TLS unset. This adds no new Clash/YAML schema support.

`config_path`, other core ECH options, standalone JSON dispatch, editor binding,
storage persistence, and TLS verification behavior are outside this patch.
No referenced configuration file is opened. This is not full support for every
ECH option accepted by the core.

## Run

Requires Python 3 and a C++20 compiler (`CXX`, or `c++`):

```sh
python3 tests/ech-json-import/test_ech_json_import.py
python3 tests/ech-json-import/mutation_checks.py
```

Optional installed Qt5 fixture-parser cross-check:

```sh
/usr/bin/python3 tests/ech-json-import/test_ech_json_import.py --fixture-parser qt5
```

Optional Qt6 Core strings, containers and JSON byte parsing (development headers
and pkg-config metadata required):

```sh
python3 tests/ech-json-import/test_ech_json_import.py --qt
```

Run current tests against an immutable baseline without using its dirty files:

```sh
python3 tests/ech-json-import/test_ech_json_import.py \
  --repo /path/to/nekobox-git-checkout \
  --ref 22b5abad885dfacc4bf6784f96e4aaf01270d9cc
```

`--keep-generated /tmp/ech-json.cpp` saves the generated translation unit.
`EXTRA_CXXFLAGS` adds optional compiler flags, for example
`-fsanitize=undefined -fno-sanitize-recover=all`.

## What is tested

The runner extracts and compiles without editing their bodies:

- `From_Json::add_tls` from `Json2Bean.cpp`
- the ECH-producing block in `V2rayStreamSettings::BuildStreamSettingsSingBox`
- relevant `Data::Node` type checks and bool/string/list conversion methods
- all eight affected `TryParseJson` functions and TrustTunnel's `TryParseYaml`
- `RawUpdater::updateSingBox`, exercising an outbound envelope independently of
  standalone-JSON dispatch

The three ECH model declarations/defaults are also extracted from the production
header. Source hashes are printed. Extraction fails if expected anchors are not
unique. Tests cover all three fields through the actual ECH exporter block and
importer, nested true/false/absent values, query-only and empty config, string
arrays and blank lines, literal import whitespace, legacy inputs, conflicting
forms, malformed types, reused-helper state, all failure-propagating callers,
and rejection before updater fixup/AddProxy while later valid entries survive.

Existing exporter trimming/splitting is preserved; roundtrip cases use text for
which that existing normalization is lossless. Import-only checks cover literal
whitespace retention. Rejected `add_tls` calls leave the helper's target fields
unchanged, but whole-bean parsing is not transactional: callers can assign
unrelated fields before calling it. Normal subscription import uses fresh beans.

## Boundaries and limitations

Default mode uses explicit standard-library adapters for Qt strings/containers
and JSON values. Python parses `fixtures.json` into typed fixture values; it is
not the application's parser. Qt5 mode additionally parses those exact JSON bytes
with installed PyQt5 and checks agreement. It does not turn the C++ harness into
a native Qt5 application. Optional Qt6 mode uses real Qt JSON parsing for the
JSON fixtures and exporter roundtrips but still has the following boundaries:

- `Data::Node` construction and lookups are adapters. Actual conversion and type
  predicate bodies are compiled, including the existing `toBoolean` behavior.
- `EnumFieldName` compares literal keys. Production normalizes case, underscores
  and hyphens; normalization/collision behavior is not tested. All fixtures use
  canonical keys.
- Composite `Node::toQuoted` conversion is an unexercised stub. These tests do
  not claim general malformed-legacy coercion coverage.
- Unrelated bean field helpers, numeric/header access, entity factory, database,
  updater fixup and AddProxy storage are boundaries. Actual importer/caller/
  updater control flow executes; full protocol bean semantics do not.
- The exporter excerpt is hosted in a small wrapper and the surrounding TLS/
  transport builder is not executed. Default mode tests typed objects, not JSON
  byte serialization; the optional native Qt6 mode includes that extra step.

No application launch, real user profile, credentials, ECH DNS lookup, TLS
handshake, network connection, persistence/database test, GUI test, Windows test,
or complete NekoBox build is performed. Synthetic PEM strings are deliberately
not usable ECH configurations. Native application/core validation is still needed
before broader compatibility or connection-success claims.

## Recorded verification

On the supplied environment, current adapter checks and Qt5 JSON-fixture checks
pass; the immutable baseline fails. The mutation runner verifies that removing
any propagation guard, using the broken bool accessor, changing default/false
semantics, losing either text field, accepting mixed config arrays, or falling
back on malformed nested ECH is detected by runtime assertions.

Native Qt6 mode is blocked because Qt6 Core development metadata is unavailable.
Full application configuration is blocked because CMake is not installed. The
repository's `python3 -m unittest discover -s tests -v` command also reports an
existing discovery error: `test_subscription_update_guard.py` parses the discovery
arguments at import. Its standalone command and the other available standalone
regressions were run separately; this is not a full-suite pass.

## Source references

Pinned baseline:
[import helper](https://github.com/qr243vbi/nekobox/blob/22b5abad885dfacc4bf6784f96e4aaf01270d9cc/src/gharqad/configs/proxy/Json2Bean.cpp#L22-L48),
[ECH exporter](https://github.com/qr243vbi/nekobox/blob/22b5abad885dfacc4bf6784f96e4aaf01270d9cc/src/gharqad/configs/proxy/Bean2CoreObj_box.cpp#L234-L245),
[model defaults](https://github.com/qr243vbi/nekobox/blob/22b5abad885dfacc4bf6784f96e4aaf01270d9cc/src/nekobox/configs/proxy/V2RayStreamSettings.hpp#L65-L67),
[updater acceptance gate](https://github.com/qr243vbi/nekobox/blob/22b5abad885dfacc4bf6784f96e4aaf01270d9cc/src/gharqad/configs/sub/GroupUpdater.cpp#L1015-L1047).
Selected core:
[ECH option types](https://github.com/shtorm-7/sing-box-extended/blob/55faa763f986f4ca8a492d9b2719bc6330d2bef5/option/tls.go#L235-L245),
[enabled gate](https://github.com/shtorm-7/sing-box-extended/blob/55faa763f986f4ca8a492d9b2719bc6330d2bef5/common/tls/std_client.go#L247-L254),
[inline PEM/query behavior](https://github.com/shtorm-7/sing-box-extended/blob/55faa763f986f4ca8a492d9b2719bc6330d2bef5/common/tls/ech.go#L27-L55).
