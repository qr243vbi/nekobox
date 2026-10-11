# Preserve existing VLESS XHTTP fields from Clash YAML

Partial work for [issue #228](https://github.com/qr243vbi/nekobox/issues/228).
The active `RawUpdater::updateClash` records `network: xhttp`, but previously
never read `xhttp-opts`. Consequently its existing path, host and mode fields
were lost before the sing-box JSON builder ran.

## Bounded change

For VLESS with the active network `xhttp`, map the scalar `xhttp-opts.path`,
`host` and `mode` into the existing stream fields. An absent mode preserves the
existing `auto` default. Other active networks and other protocols are unchanged.
The existing YAML conversion helpers and validation policy are retained.

The current [Mihomo transport documentation](https://wiki.metacubex.one/en/config/proxies/transport/#xhttp-opts)
defines these fields and limits XHTTP to VLESS. This patch does **not** resolve
all of #228. In particular, the issue's example includes `download-settings`,
which remains unmapped. Extra headers, reuse/padding settings, inheritance of
separate-download TLS settings, and unsupported-option diagnostics require
separate work. No connection success is claimed.

A separate decoder-only check of the pinned sing-box-extended
`v1.14.1-extended-2.7.2` also rejects the default emitted XHTTP transport without
`x_padding_bytes` (`x_padding_bytes cannot be disabled`). Explicit 100–1000
padding passes that decoder. This prerequisite affects existing XHTTP output
beyond YAML and is not changed by this field-preservation patch.

## Run

Requires Python 3, C++20, Qt6 Core, yaml-cpp development files and Boost headers:

```sh
python3 tests/clash-xhttp-import/test_clash_xhttp_import.py
python3 tests/clash-xhttp-import/mutation_checks.py
python3 tests/clash-xhttp-import/test_clash_xhttp_import.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
```

Dependencies normally use pkg-config. For an extracted Debian yaml-cpp package,
set `YAML_CPP_ROOT` to the package root: the runner uses `usr/include` and
`usr/lib/x86_64-linux-gnu/libyaml-cpp.a`. It does not install anything.
`EXTRA_CXXFLAGS` supports sanitizer options; `--keep-generated FILE` saves the
translation unit; `--repo PATH` selects a separate source snapshot.

Expected results: 273 checks, zero failures with the fix. Seven independent
behavior mutations must fail assertions: path, host, mode, missing-mode fallback,
network guard, protocol guard and mode JSON emission. An eighth mutation restores
the invalid direct enum/string comparison and must fail compilation at that
comparison before any assertion runs. Dependency errors cannot count as a caught
mutation. `EXTRA_CXXFLAGS=-I/path/to/boost/include` supports non-system headers.

## Actual enum compile regression

The original fixture replaced `V2RAYTransportsEnum` with `QString`. That adapter
masked the invalid `*bean->stream->network == "xhttp"` comparison introduced by
this import change: the fixture passed while the native application did not
compile. The production wrapper derives from `JsonEnum`, not `QString`.

The runner now uses the verbatim production `JsonEnum` and `EnumFieldName`
declarations and implementations, enum macros, preset lists and stream enum
declarations with real Boost maps. Static assertions reject a QString-based
transport substitute. The pre-fix main source fails compilation with this runner;
the one-line explicit QString conversion fixes the comparison and preserves all
273 behavior checks. Unchanged enum implementations have narrowly scoped warning
suppressions for unused parameters/variables and catch-by-value. Import and
emission bodies still compile with `-Wall -Wextra -Werror`.

This is a focused production-type regression, not a full native application
build. Windows/MSVC verification remains the separate CI build.

## Evidence boundaries

The runner compiles verbatim production `RawUpdater::updateClash`,
`RawUpdater_FixEnt`, YAML conversion helpers, stream field declarations,
`GetHeaderPairs`, full `BuildStreamSettingsSingBox` and its JSON helper bodies.
YAML parsing is real yaml-cpp; Qt strings, containers, JSON serialization and
reparsing are real Qt6. If the separate ECH YAML patch is present, its parsing
helper and actual Base64 dependencies are also compiled, so both candidates can
be tested together. The combined-source run passes both XHTTP (273 checks) and
ECH (593 checks). It does not replace the XHTTP mapping or emission logic
with a test reimplementation.

The profile factory, storage locks, reflection accessor, bean-level network,
`AddProxy` and log destination are explicit inert test adapters. Stream transport
and packet-encoding enum storage and conversions are production code. Reserved
`.invalid` domains keep IP detection out of scope. The stream and KCP fields
come from production declarations; this is not a full bean/database build.

Fixtures cover all four documented modes, absent/empty/non-map options,
missing mode, empty strings, Unicode, literal URL punctuation, TLS/SNI/ALPN,
UUID, independent profiles, inactive XHTTP options and other protocols.
The larger issue-shaped fixture checks only top-level field preservation;
its download settings deliberately remain outside the patch.

No app/core process, real profile, subscription, settings change, proxy/VPN
connection or network request is used. A native application build, persistent
roundtrip, Windows behavior and end-to-end connection remain unverified.
