# Preserve and validate Hysteria authority port unions

Base: upstream main `1c35d5fed1f167dcdb4c549bc2079dc5dd0a1da4`.

The QUIC invalid-port fallback passed authority text through a generic numeric
URL parser, which clears non-scalar ports. Splitting that empty result created
one empty range. Both `hy2://synthetic-auth@example.invalid:443-445` and a
malformed `:65536` thus imported with `server_ports: [":"]`. TUIC malformed
ports silently became 443 instead.

The production change is limited to QUICBean::TryParseLink's invalid-port
fallback. It validates comma-separated ports and ascending closed ranges,
retains the existing uint16 endpoint domain (0–65535), normalizes '-' to ':',
and reparses the original URI with only the port union replaced. Rebuilding
from the original prevents an invalid-host error from being accidentally
cleared. Malformed ports and TUIC range syntax return false.

[Hysteria2's URI specification](https://v2.hysteria.network/docs/developers/URI-Scheme/)
explicitly supports [port-hopping lists and ranges](https://v2.hysteria.network/docs/advanced/Port-Hopping/).
Existing export/config code already supports them. No new scheme or transport
support is added. Generic URLParser, valid scalar/omitted ports, `mport`, query
semantics, and exporter behavior are unchanged. PR323's port-443 default and
unhandled-scheme rejection are independent patches.

## Run

Requires Python 3, a C++20 compiler, pkg-config and Qt6 Core development files.
No dependencies are installed. Normal pkg-config sysroot variables support an
extracted Qt toolchain.

```sh
python3 tests/quic-authority-ranges/run_test.py
python3 tests/quic-authority-ranges/run_test.py --ref BASE_COMMIT
python3 tests/quic-authority-ranges/mutation_checks.py
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' python3 tests/quic-authority-ranges/run_test.py
```

`--repo PATH` selects source; `--ref COMMIT` reads immutable source;
`--keep-generated FILE` saves the translation unit. Source hashes and Qt version
are printed. Verified native Qt6.8.2 result: 4,537 assertions, 813 expected
baseline failures and zero candidate failures. Seven deliberate mutations must
compile and fail runtime assertions; compiler errors are not counted as kills.

Coverage includes lists/ranges and bounds; malformed, nonnumeric, overflow,
descending, empty and extra-separator inputs; reserved DNS/IPv4/IPv6 hosts;
masked invalid hosts; query/fragment/path boundaries; wrappers and mixed lines;
TUIC rejection; ordinary scalar/omitted-port controls; legacy mport; exact
server_port/server_ports output; and range/list export-and-reimport with query
fields. Combined with the separate scheme fix and PR323's exact production
change, all 4,583 combined checks and PR323's original 944 checks pass.

## Downstream meaning and boundaries

`":"` is not necessarily rejected downstream. The exact
[pinned sing-quic ParsePorts implementation](https://github.com/SagerNet/sing-quic/blob/6a3a24d65b99/hysteria/client.go)
expands empty lower/upper bounds over 0–65535; the
[pinned outbound](https://github.com/shtorm-7/sing-box-extended/blob/v1.14.1-extended-2.7.2/protocol/hysteria2/outbound.go)
passes ServerPorts through. Pins are in `core/server/go.mod`. This is source
analysis, not a live connection result or an executed core test.

The runner extracts actual RawUpdater dispatch, factory/entity behavior,
AddProxy, complete QUIC parser, exporter, config builder and helpers. Qt6
URL/query/string/JSON/Base64 behavior is native. Imports remain in memory with
an empty ignore map. Enum storage is a default-state text stand-in. Persistence
is inert, Load throws, unrelated import paths throw, and unrelated stream
postprocessing is a counting boundary that does not change QUIC fields.

No app, core, network, user profile, setting or GUI is launched or opened. This
is not a full application/Windows, persistence or live-connectivity test.

## Separate unpatched export limitation

Query-free Hysteria2 range export is already broken independently of this
parser change. `QUICBean.cpp:108-112` replaces only `":0?"`. After importing
`hy2://synthetic-auth@example.invalid:443-445`, the exact exporter returns
`hy2://synthetic-auth@example.invalid:0`. A fragment-only input similarly exports
`:0#synthetic`. These are observed failing roundtrips, not passing coverage.
They are recorded separately; this patch does not change the exporter.
