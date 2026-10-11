# TLS URL exporter-alias compatibility

Trojan, VLESS and VMess URL exports use the shared TLS exporter, which writes
`insecure=1` for an explicitly enabled flag. Their manual URL importers formerly
read only `allowInsecure`, losing that flag on a fresh export/reimport.

## Prerequisites and security impact

This compatibility delta requires both strict-value fixes:

- [Trojan/VLESS #331](https://github.com/qr243vbi/nekobox/pull/331),
  `58cd3f508b5708436c6a59cd6f61a2582deb9042`
- [VMess URL #332](https://github.com/qr243vbi/nekobox/pull/332),
  `f4be31c77adad72fc3671ed4bfcf3db19f57ccce`

The prerequisites make the existing primary field accept exact `1`/`true` and
assign false for everything else. This patch does not duplicate those changes.
The local reference baseline combines them with upstream
`1c35d5fed1f167dcdb4c549bc2079dc5dd0a1da4`.

This is a security-impacting compatibility extension, not solely hardening:
alias-only `insecure=1` or `insecure=true` now selects certificate-verification
bypass where these three URL importers previously ignored the key. It preserves
the explicit value already emitted by NekoBox. It does not enable bypass for an
absent, empty, false, malformed, or unsupported selected value.

## Bounded policy

1. If Qt recognizes `allowInsecure` as present, use its first value, including
   empty, key-only and invalid values. Never fall through to the alias.
2. Otherwise use the first `insecure` value only if the original raw link passes
   `QUrl(link, QUrl::StrictMode).isValid()`. If not, set the field false. Do not
   reject the profile or change how its other fields are normally parsed.
3. Set the field true only for exact `1` or `true` after existing Qt retrieval;
   set it false otherwise, including on reused beans.

Consequently `allowInsecure=false&insecure=1` is false, while
`allowInsecure=1&insecure=false` stays true. Reversing key order changes neither
result. An empty/key-only/invalid primary blocks an affirmative alias. Duplicate
selection remains first-value, including percent-encoded equivalent keys.

OR or fallback-on-empty would newly override a secure primary value.
Secure-on-conflict would change the existing affirmative primary's meaning.
Changing the shared exporter would affect other protocols; changing only these
exporters would leave already-exported URLs unreadable. Presence-only fallback
is the narrowest repair preserving every recognized primary-key outcome.

The validity gate checks the original input, not the tolerant URL's reserialized
form. It prevents malformed percent escapes from hiding an encoded secure
primary and activating the alias. For example,
`allow%49nsecure=false&ignored=%&insecure=1` remains false even though Qt's
tolerant query parser no longer recognizes the encoded primary key.

There is a deliberate compatibility tradeoff: alias-only URLs with invalid raw
syntax, including unescaped spaces/control characters or malformed percent
escapes in an unrelated query, credential or fragment, remain secure even when
the normal tolerant parser accepts the profile. Properly encoded equivalents
and the application's `FullyEncoded` exports remain eligible. A recognized
primary value keeps its existing meaning even on such a tolerant-only URL.
An authority already rejected by the original parser is still rejected; the
gate adds no profile rejection path.

This is an application compatibility rule, not a claimed protocol standard.
The [VMess/VLESS proposal](https://github.com/XTLS/Xray-core/discussions/716)
defines case-sensitive parameters, prohibits duplicates, and does not standardize
either insecure alias or their precedence. The
[older Trojan scheme](https://github.com/trojan-gfw/trojan-url#url-scheme)
defines only the basic URL. Qt documents
[presence testing and first-duplicate retrieval](https://doc.qt.io/qt-6/qurlquery.html#queryItemValue).
Its [StrictMode documentation](https://doc.qt.io/qt-6/qurl.html#ParsingMode-enum)
describes the raw-syntax validity boundary used here.

## Run

Requires Python 3, C++20, pkg-config and existing Qt6Core development files.
No dependency installation or download is performed.

```sh
python3 tests/tls-url-alias/test_tls_url_alias.py
python3 tests/tls-url-alias/test_tls_url_alias.py --primary-only
python3 tests/tls-url-alias/mutation_checks.py
```

`--repo PATH` selects source; `--ref COMMIT` reads immutable source.
`--keep-generated PATH` saves the source-extracted C++ translation unit.
`EXTRA_CXXFLAGS` supports instrumentation such as
`-fsanitize=undefined -fno-sanitize-recover=undefined`.
`--primary-only` is the differential compatibility matrix: run unchanged tests
against the strict prerequisite baseline and this candidate. Both must pass.
The full suite must fail on that baseline's alias and roundtrip paths.

## Coverage and boundaries

The runner compiles verbatim complete `TryParseLink`, `ToShareLink` and outbound
JSON methods for Trojan/VLESS, VMess and AnyTLS, plus the complete stream builder,
shared TLS and query helpers, actual Base64 decoder, and source field/default
declarations. Qt URL/query/string/JSON/Base64 behavior is real. Only storage,
ownership and enum boundaries are inert stand-ins. Unused XHTTP/KCP branches
throw if reached; TCP fixtures do not model unrelated transports.

Tests cover strict positive/negative/absent/empty/key-only values; fresh and
already-true streams; cross-alias values and both orders; duplicate-first
behavior; encoded keys/values; double-encoding; malformed escapes; encoded
delimiter isolation; TLS-disabled controls; shared AnyTLS's unchanged policy;
and preservation of destination, port, credential, SNI and protocol in actual
serialized/reparsed outbound JSON. The global override stays false. Selected
cases print the exported URI and complete emitted JSON as evidence.
Additional raw-input checks cover malformed/truncated/nonhex escapes, spaces,
controls, query/credential/fragment validity, already-invalid authorities,
encoded-key hiding, valid encoded forms and preserved affirmative-primary
behavior. Tolerant-only alias input also resets an already-true bean to false.

An existing Qt TolerantMode detail is pinned explicitly: one malformed percent
escape in a query causes other percent escapes there to be treated literally.
An otherwise escaped primary `1`/`true` can therefore become false, and an
encoded primary key can stop matching. The new raw validity gate prevents the
latter case from enabling the alias. This patch does not change URL
normalization or add another decoding pass.

VMess Base64/native JSON behavior and the independent global override are
unchanged. The adjacent prerequisite VMess suite covers Base64 behavior and has
a separate native-JSON observation probe. The separate Data::Node boolean fix
is required to preserve native JSON boolean true on this pinned baseline.

Mutation controls separately target both production parsers for no fallback,
alias-first, empty fallback, OR, nonempty truthiness, always false, stale true,
last duplicate, case folding, trimming and secure-on-conflict. Per-parser cases
also include omission of the gate, validation after normalization, gating the
primary branch too, TolerantMode validation and query-only validation. Shared mutations
cover decode-before-split, omitted JSON flag and omitted exporter flag. Every
mutation must compile and fail runtime assertions; compiler errors do not count.
The prerequisite strict suites retain their own eight-mutation commands: their
test-only anchors accept both the original strict assignment and its later
alias-selected form, while rejecting unknown or multiple anchors.

These are offline source-method tests. No application/core binary, live TLS or
network connection, real user profile, saved settings, DNS policy, subscription,
VPN, system proxy or lifecycle operation runs. No full-app test pass is claimed.
