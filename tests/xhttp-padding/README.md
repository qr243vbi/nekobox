# Emit the XHTTP padding default before core validation

The pinned sing-box-extended `v1.14.1-extended-2.7.2` rejects an XHTTP transport
with omitted `x_padding_bytes`: `x_padding_bytes cannot be disabled`. Its
`GetNormalizedXPaddingBytes` supplies 100–1000, but option decoding rejects the
zero-value range before transport construction can use that normalization.

The emitter now inserts `{ "from": 100, "to": 1000 }` only if the key is absent
after the existing extra-options parser. It does not overwrite explicit values,
change range conversion, relax decoder validation, change the dependency pin,
or persist anything to the profile. It is confined to the existing XHTTP branch.

This addresses a decoder prerequisite affecting all existing XHTTP emission,
including the separate partial Clash YAML mapping candidate for issue #228.
It does not add split-download support, map any additional YAML fields, or
establish that a connection works.

## Run

Requires Python 3, a C++20 compiler, Qt6 Core development files discoverable
through pkg-config, the Go toolchain required by `core/server/go.mod`, and its
already-populated dependency cache:

```sh
python3 tests/xhttp-padding/test_xhttp_padding.py
python3 tests/xhttp-padding/mutation_checks.py
python3 tests/xhttp-padding/test_xhttp_padding.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
```

Set `GO`, `GOMODCACHE`, and `GOCACHE` to use existing local toolchain/cache paths.
The runner forces `GOPROXY=off`, `GOSUMDB=off`, `GOTOOLCHAIN=local`, `GOWORK=off`,
`CGO_ENABLED=0`, and `-mod=readonly`. It copies the exact production module files
into a temporary fixture directory, changing only the module name. Missing
dependencies fail rather than downloading. `--repo` selects a source snapshot;
`--artifacts DIR` preserves emitted JSON, generated C++, and production hashes.
`EXTRA_CXXFLAGS` supports sanitizer flags.

Expected baseline: 582 Qt assertions with 26 failures, and 25 failing decoder
fixtures. Candidate: 582 passing Qt assertions, all 68 decoder fixtures passing,
and the independent pinned-core contract control passing. All eight mutations
must fail actual assertions; compiler or dependency failures do not count.

## What executes

The harness extracts verbatim the full production `BuildStreamSettingsSingBox`,
`parseExtraXhttp`, range/xmux/KCP/domain helpers, production stream/KCP field
declarations, `GetHeaderPairs`, and applicable utility helpers. Real Qt6 performs
all string, container, JSON conversion, serialization, and parsing operations.
The actual Qt-emitted transport bytes are then decoded by the actual pinned Go
`option.V2RayTransportOptions`, without reconstructing the transport in Go.

Only enum storage, reflection declarations, logging destination, and global
preferences are inert adapters. The test uses an isolated stream class; it is
not a complete bean/database/application build. The core is used only through
option decoding and the default-range getter. No transport client, core process,
app process, real profile/settings, proxy/VPN connection, or network request runs.

Fixtures cover omitted/empty extras; all four modes plus the empty default mode;
VLESS/VMess/Trojan emission; case/underscore padding aliases; scalar, string and
object ranges; explicit zero/null/false/array/malformed-range/negative/reversed
values; invalid mode/placement/method; unrelated options and TLS; repeated
emission; independent instances; and non-XHTTP/naive isolation. The existing
parser continues ignoring malformed/non-object extra JSON; this change does not
add extra-JSON validation. Explicit invalid parsed padding still reaches the
decoder unchanged by this fix. KCP's pre-existing pinned-decoder rejection is
characterized, not repaired.

## Core contract sources

As checked on 2026-10-11, the relevant option file is byte-identical between the
pinned tag (commit `55faa763f986f4ca8a492d9b2719bc6330d2bef5`) and the current
extended branch (commit `5956780ba3f140feff771d003914a1b82ea9d5d0`):

- [Pinned decoder and normalization](https://github.com/shtorm-7/sing-box-extended/blob/55faa763f986f4ca8a492d9b2719bc6330d2bef5/option/v2ray_transport.go)
- [Current extended option contract](https://github.com/shtorm-7/sing-box-extended/blob/5956780ba3f140feff771d003914a1b82ea9d5d0/option/v2ray_transport.go)

The tests establish an offline emission/decoding boundary only. A full native
application build, persistent roundtrip, Windows behavior, and end-to-end
connectivity require separate verification.
