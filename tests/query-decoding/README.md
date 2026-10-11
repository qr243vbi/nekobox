# URL query decoding regressions

This prerequisite fixes share-link query tokenization and single decoding before
layering the remaining Trojan/VLESS ECH import change. It does not itself add
Trojan/VLESS or VMess ECH support or change boolean-value policy.

## Contract

- Construct the query from the URL while delimiters are still encoded.
- Decode each text field once through `GetQueryValue`, whose per-item retrieval
  uses `QUrl::FullyDecoded`.
  `%26allowInsecure%3D1` is field data, never another query item. `%2526` becomes
  literal `%26`, never `&`. `+` stays `+`, not form-encoded whitespace.
- `AddQueryString` accepts decoded application text. It protects literal `%`
  before passing text to Qt's encoded-input query API. Tailscale callers no
  longer pre-encode or explicitly decode a second time.
- QUIC text getters use the same contract. Its direct opaque string exports use
  the shared safe helper. Hysteria auth uses explicit percent encoding to preserve
  a present empty auth item; its existing percent-encoded obfs exports are already
  single encoded and remain unchanged.
- Missing/empty fallback, duplicate-first precedence, integer defaults, existing
  boolean-value interpretation, protocol aliases and legacy Base64 formats stay
  unchanged. This does not validate opaque ECH/PEM, cryptographic keys, transport
  settings or authentication data.

## Run

Requires Python 3, a C++20 compiler, pkg-config, and Qt6Core development files.
No packages are installed by these runners.

```sh
python3 tests/query-decoding/test_query_decoding.py
python3 tests/query-decoding/test_query_decoding.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
python3 tests/query-decoding/mutation_checks.py
python3 tests/test_link_tls.py
python3 tests/test_link_booleans.py
```

`--repo PATH` selects a checkout and `--ref COMMIT` reads immutable source.
`--keep-generated PATH` saves the source-extracted C++ translation unit.
The test reports hashes of source inputs and its Qt version. Mutation mode must
reach runtime assertions: compiler errors do not count as killed mutants.

## Coverage and limits

The runner extracts verbatim full `TryParseLink` and `ToShareLink` bodies for:
AnyTLS, HTTP, Juicity, Mieru, Naive, SSH, Shadowsocks, ShadowTLS, SOCKS, Tailscale,
Tor, Trojan/VLESS, TrustTunnel, VMess and WireGuard, plus QUIC's Hysteria, TUIC and
Hysteria2 branches. It extracts their field declarations, constructors where
applicable, query helpers, shared TLS helpers, JSON helpers and Base64 decoder.
The parser inventory check catches new untested `GetQuery` consumers.

Qt URL, query, JSON and byte/string operations are real Qt6. Bean ownership and
enum/storage types are test-only boundaries. WireGuard's alternate INI and
Amnezia VPN import helpers are explicit boundaries: URI fixtures reach the full
URI parser, while a non-URI call throws. Those alternate formats are not covered.
The actual URLParser header is compiled for QUIC's fallback dependency, but no
invalid-port/range fallback fixture runs. No full application/core, live
profile, subscription, settings, VPN or network action runs.

Assertions cover encoded `&`, `=`, `#`, `%`, raw and escaped Base64 `+/=`, literal
percent escapes (including malformed-looking application text), Unicode, LF and
CRLF PEM text, JSON-valued fields, explicit flags versus injected text, export
roundtrips, empty/default/duplicate behavior, ordinary protocol aliases and
legacy Shadowsocks Base64/VMess Base64-JSON forms. Reserved fixture destinations
are `example.invalid`, `query.invalid` and the documentation address 192.0.2.1.

The optional existing TLS ASCII adapter is only a helper-logic fallback; it is
not query-parsing evidence. Its signature adapter is updated for FullyDecoded.
The primary regression suite has no adapter mode.

### HTTP scheme regression (issue #68)

HTTP(S) fixtures additionally cover lowercase, uppercase and mixed-case schemes,
implicit ports (80 for HTTP, 443 for HTTPS), explicit ports, IPv6 authorities,
TLS and `insecure` preservation through share-link export/reimport. Credentials,
paths, names, SNI and JSON headers are checked alongside those fields. Unsupported
schemes and invalid URLs are rejected. This is an in-memory parser/exporter test;
no proxy connection is made.

The ports and case-insensitive scheme semantics follow
[RFC 9110, section 4.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-4.2).
Three independent `--mutation` cases exercise these regressions:
`http-default-443`, `http-raw-prefix`, and `http-no-scheme-guard`.
For GCC 16 with Qt 6.11.2, `EXTRA_CXXFLAGS=-Wno-error=sfinae-incomplete`
keeps Qt's header diagnostic as a warning while retaining `-Werror` for other
diagnostics. This is a compiler/header compatibility workaround, not a parser fix.

## Caller audit

- AnyTLS, HTTP(S), Juicity, Naive, ShadowTLS and TrustTunnel use shared TLS import.
  Their text/map getters inherit the helper fix; no bean-specific parse change is
  needed.
- Trojan/VLESS and VMess have independent TLS/transport importers. Their helper
  reads inherit the fix; direct fragment-delay text reads now use the helper.
- SSH, WireGuard, Shadowsocks and Mieru direct text reads are migrated. WireGuard
  list separators and Mieru normalization remain unchanged after text decoding.
- Tailscale's five explicit percent encoders and five explicit decoder calls are
  removed. Its sixth direct string getter (`exit_node`) uses the shared getter.
- Tor's JSON-list/map and path reads inherit the shared helper fix.
- QUIC's separate constructor is moved to `GetQuery`; text getters and opaque
  exporters follow the common contract. `insecure`, `allow_insecure`,
  `disable_sni`, `fragment`, `record_fragment`, `allowInsecure`, and the shared
  `set_boolean` still use their existing exact boolean rules.
- Shared security/fingerprint exports and VLESS encryption exports use the safe
  helper. The only remaining direct query exports in proxy source are fixed
  literal flags/protocol names and QUIC's explicitly percent-encoded obfs/auth
  text. Explicit empty Hysteria auth preserves string-auth mode on roundtrip.
- The separate internal subscription-deeplink reader in `mainwindow.cpp` already
  uses `QUrlQuery(url)` and does not call these shared getters/exporters. It is
  unchanged and not exercised by this suite.

The fake enum boundary deliberately does not assert validity of arbitrary enum
text or integer enum-to-name mapping. Some percent-rich probes in selector fields
check serialization/decoding only; they do not establish supported core values.
