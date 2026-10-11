# Live Clash/YAML ECH import regression

This addresses one remaining subscription-import path in
[#205](https://github.com/qr243vbi/nekobox/issues/205).
`RawUpdater::updateClash` performs its own protocol mapping and does not call
`From_Yaml::add_tls`. The new mapping is attached to that live updater, after
protocol parsing and before the profile is added.

## Mapping and validation

Authoritative input format:
- [Mihomo TLS options](https://wiki.metacubex.one/en/config/proxies/tls/#ech-opts)
- [Mihomo ECHOptions parser](https://github.com/MetaCubeX/mihomo/blob/Meta/adapter/outbound/ech.go)

For the existing HTTP, Trojan, VLESS, VMess, AnyTLS and ShadowTLS stream-backed
TLS imports:
- Missing `ech-opts` preserves existing behavior.
- `ech-opts.enable` uses YAML boolean parsing; omitted means false. Explicit
  false stays false even with nonempty config. True plus absent/empty config
  enables DNS discovery.
- `ech-opts.config` is base64 ECHConfigList data. Valid data becomes an
  `ECH CONFIGS` PEM block for the bundled sing-box core, without interpreting
  binary bytes as Unicode. Go-compatible CR/LF in Base64 is accepted; padding
  and other invalid characters are checked when ECH is enabled.
- `ech-opts.query-server-name` maps to the existing query-server-name setting.
- Disabled opaque invalid Base64 is retained as text inside the PEM wrapper,
  since Mihomo does not validate/use the config when disabled. It is not
  activated by import. A later manual enable is outside this import policy.
- Malformed active ECH data and wrong field shapes reject that profile, log a
  diagnostic, and allow later profiles in the batch to import. They do not
  silently disable requested ECH or turn malformed static config into DNS ECH.
- Validation completes before ECH fields are assigned. Non-TLS profiles do not
  gain TLS or ECH merely because the options are present.

This patch does not change transport/TLS defaults, add ECH to QUIC beans that
lack stream ECH fields, fix the separate unused Data::Node YAML helper, or add
other core ECH options. It does not claim the whole issue is resolved.

## Run

Requires Python 3, a C++20 compiler, Qt6 Core and yaml-cpp development files:

```sh
python3 tests/ech-yaml-import/test_ech_yaml_import.py
python3 tests/ech-yaml-import/mutation_checks.py
```

Normally dependencies are located through pkg-config. To use a locally
extracted Debian development package without installing it, set `YAML_CPP_ROOT`
to its extraction root; the runner links `usr/lib/x86_64-linux-gnu/libyaml-cpp.a`
from the same package as `usr/include/yaml-cpp`.

`--ref COMMIT` reads immutable production source. `--keep-generated /tmp/ech.cpp`
saves the generated translation unit. `EXTRA_CXXFLAGS` can enable sanitizers.

## Evidence boundaries

The runner compiles the full production `RawUpdater::updateClash`,
`RawUpdater_FixEnt`, YAML conversion helpers, strict Base64 implementation and
stream field declarations. YAML parsing, strings and Base64 serialization use
real yaml-cpp and Qt. The profile factory, storage locks, reflection accessor,
AddProxy and log destination are explicit inert adapters. Reserved `.invalid`
domains make the unrelated IP-detection branch unnecessary in this harness.

Fixtures cover six TLS protocol branches, absent/empty/true/false config,
query-only ECH, binary bytes containing Base64 `+`, `/`, `=`, multiline Base64,
malformed maps/values/Base64, disabled malformed config, and continuation after
rejected entries. Mutation checks independently break mapping, enable behavior,
PEM wrapping, query name, validation, disabled preservation, CR/LF and rejection.

This is an offline source-extracted test, not a full-app, database, Windows or
TLS-handshake test. No network request, subscription, core process, real user
profile or VPN/settings operation is performed.
