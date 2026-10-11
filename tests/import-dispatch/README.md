# URI dispatch and Hysteria2 default-port regression

The production change is limited to the Hysteria2 branch's `url.port(443)`.
The [Hysteria2 URI specification](https://v2.hysteria.network/docs/developers/URI-Scheme/)
defines 443 when the authority omits a port. An empty `:`, which Qt treats as
an omitted port, receives the same fallback. Explicit ports remain unchanged.
This is an adjacent defect discovered while auditing issues
[#82](https://github.com/qr243vbi/nekobox/issues/82) and
[#206](https://github.com/qr243vbi/nekobox/issues/206), not a demonstrated fix for
either historical report.

## Run

Requires Python 3, a C++20 compiler, pkg-config and Qt6 Core development files.
No new dependencies are installed by these scripts.

```sh
python3 tests/import-dispatch/test_import_dispatch.py
python3 tests/import-dispatch/test_import_dispatch.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
python3 tests/import-dispatch/mutation_checks.py
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' python3 tests/import-dispatch/test_import_dispatch.py
```

The regression has 944 runtime assertions. The pinned baseline fails 49
assertions, all concerning missing/empty Hysteria2 ports; the candidate passes.
Six mutations must compile and then fail runtime checks. Compilation failure
never counts as a killed mutation. `--repo PATH` selects another checkout;
`--ref COMMIT` reads immutable source; `--keep-generated PATH` saves the exact
compiled translation unit. Source hashes and Qt version are printed.

`--probes` prints a separate 77-case observational port/alias matrix. It is a
diagnostic, not a passing conformance suite. In particular, its successful exit
does not mean the observed malformed-port or range behavior is correct.

## Fidelity and safe boundaries

These production bodies are extracted rather than rewritten:
- Full `RawUpdater::update`, `Disect` and JSON-splitter bodies
- Base64 implementation and subscription-text decoding helper
- `ProfileManager::NewProxyEntity`, including scheme canonicalization
- `ProxyEntity::bean`, `isValid`, constructor and unlock body
- Production entity name/address/port field defaults (port starts at 1080)
- Production outbound-type display map and display lookup
- QUIC and Trojan/VLESS constructors and full `TryParseLink` bodies
- Shared query/network/TLS/default-field helpers
- The actual URLParser header used by the QUIC invalid-port fallback

Qt URL/query/string/JSON operations are native Qt6, not model implementations.
Synthetic fixtures use reserved `.invalid`, 192.0.2.1 and 2001:db8::1 addresses.
They never connect to those addresses.

`AddProxy` captures entities in memory instead of persisting profiles. The
unrelated `RawUpdater_FixEnt` postprocessor is a counting boundary; it does not
change protocol selection or ports in production. Storage control is inert and
Load throws. Non-target bean constructors, full JSON/SIP/YAML/INI import, native
Nekoray input and encrypted Happ input throw if unexpectedly reached. Their
behavior is not inferred from this test. Enum storage is a text-only stand-in.
The tests do not launch NekoBox/core, operate live subscriptions, access profiles,
change settings or run network actions.

Coverage includes hy2/hysteria2 and uppercase spellings, absent/empty/explicit
ports, reserved DNS/IPv4/IPv6 hosts, userpass and optional authentication, plain
and nested Base64 text, JSON string-array wrapping, mixed CRLF/LF multiline
input, legacy `mport` preservation, VLESS/Trojan/TUIC controls and VLESS malformed
port rejection. Hysteria2 type, bean mode, display label, address, authentication
and port are asserted through the actual dispatch path.

## Deliberate limits and adjacent findings

- #82's placeholder `:port` fails QUrl validation; its comment's numeric or
  omitted VLESS port imports on current source. No actual failing historical
  configuration is supplied in the issue.
- #206 supplies no subscription or individual configuration, only an x3-ui
  report and a corroborating comment. Standard URI spellings in these tests
  remain Hysteria2. This does not establish behavior for the unknown reported
  subscription format or old released binary.
- Range/invalid authority ports remain a separate defect: URLParser clears
  nonnumeric/out-of-range text, and QUIC can import it as port 0 plus one empty
  range. The one-line candidate does not change that fallback.
- `hysteria-2://` is canonicalized by the factory but not recognized by the
  QUIC parser, producing default fields. It is not an official Hysteria2 URI
  alias; this separately observed parser/normalizer mismatch is unpatched.
- PR316's query changes do not alter the 77-case port/alias observations. The
  candidate is compatible with that local proposal: 944 dispatch checks and
  its 2241 query-contract checks pass when stacked. The proposal's QUIC blob
  matches published PR316 head ce2552508bc158eec19b3745e60cecb04bf236d3
  (blob 5d058e6241dd44fd2e42ee8c9e2421df29a69332).
- Native Qt6.8.2 Linux helper compilation is not a complete application build
  or a Windows integration test. Broad lifecycle/persistence/core suites were
  deliberately not run under this audit's constraints.
