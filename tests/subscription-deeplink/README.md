# Subscription deeplink query decoding

Related to [issue #20](https://github.com/qr243vbi/nekobox/issues/20).
Baseline: `a536846c9c74dd8fb104dce3319cbd9672920bb3`.

## Defect and scope

The existing subscription parser recognizes the `nekoray`, `nekobox`, and
`v2raytun` schemes. An ordinary percent-encoded query value such as
`nekobox://import?url=https%3A%2F%2Fexample.invalid%2Fsub` nevertheless returns
an empty result on the baseline. `QUrlQuery::queryItemValue()` defaults to
`QUrl::PrettyDecoded`, leaving the encoded scheme separators in the value.

The fix requests `QUrl::FullyDecoded` for the individual query value after Qt
has split the outer query. This removes exactly one outer encoding layer.
An inner URL containing `%2F`, `%26`, `%3D`, or `%2525` retains those escapes
when the entire inner URL is encoded as the outer parameter value. Decoding
the whole outer query before splitting it would incorrectly split an inner
encoded ampersand. Decoding the extracted value twice would corrupt inner
escapes. Both errors are covered by regression cases.

The query value is an encoded URL component. For an inner URL containing
literal percent escapes, callers must encode the percent signs as part of
the outer layer, e.g. inner `%2F` becomes `%252F`. A literal plus remains a
plus; this parser does not apply HTML form plus-to-space conversion.

Reference: [Qt query value encoding](https://doc.qt.io/qt-6/qurlquery.html#queryItemValue).

## Run

Requires Python 3, pkg-config, a C++20 compiler, and Qt6 Core development files:

```sh
python3 tests/test_subscription_deeplink.py
python3 tests/test_subscription_deeplink.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
```

`--repo PATH` selects another source checkout. `--ref` reads its immutable Git
revision. Standard pkg-config environment variables can select a non-system
Qt installation; `CXX` selects the compiler. Missing Qt development files
are an error, not a successful skipped run.

The runner extracts both production helper functions verbatim and compiles
them against real Qt6 Core. It does not substitute QString, QUrl, QUrlQuery,
or Base64 behavior. The test's expected URLs are synthetic reserved-domain
strings. No HTTP request or application process is started.

## Scenario matrix

The same three schemes and seven keys (`url`, `uri`, `link`, `subscription`,
`sub`, `target`, `config`) are covered throughout:

| Category | Scenarios | Checks |
| --- | --- | ---: |
| Every scheme/key combination | Plain HTTP/HTTPS/Happ; percent-encoded HTTP/HTTPS/Happ; nested escapes; Unicode; unpadded, padded, percent-encoded and `-`/`_` Base64URL; blank, whitespace, invalid Base64, unsupported scheme, double encoding, outer whitespace | 378 |
| Every ordered key-priority pair per scheme | Earlier key wins regardless of parameter position; invalid and empty earlier values fall through to the later key | 189 |
| Per-scheme integration within the helper | Query before path/fragment; path before fragment; encoded, plain and Base64URL path; encoded and Base64URL fragment; invalid/empty query fallback; uppercase scheme; unknown/uppercase keys; empty link; no candidate; duplicate-key first-value behavior; literal plus | 51 |
| Other inputs | Empty, no scheme, ordinary HTTP, unrelated scheme, incomplete percent, incomplete escape | 6 |
| Total | | 624 |

Observed with Qt 6.8.2:

- Baseline: 624 checks, 327 assertion failures, exit 1.
- Candidate: 624 checks, zero failures, exit 0.
- Temporary production-source mutations were detected: reverting to default
  decoding (327 failures), decoding extracted values twice (111), and decoding
  the outer query before splitting (132).

## Limits and unresolved behavior

This change does not validate target URL syntax or tighten Qt's permissive
Base64 decoding. For example, `https://` still passes the existing prefix
check, and an otherwise valid Base64 value with an appended `!` can still
decode. Ordinary malformed/empty input cases are covered, but there is no
claim that every malformed input is rejected.

The helper's host fallback passes through `QUrl::host()`, which case-folds a
host-only Base64 payload. The observed `nekobox://<Base64URL(subscription)>`
form therefore fails for the sample examined. The repository README does not
specify that subscription syntax; this patch leaves it for a separate format
decision. Query, path and fragment Base64URL cases above are verified.

Windows and Unix handler registration and the startup/IPC call sites were
read in source only. No registry entry, desktop file, MIME association,
application setting, database, subscription group, core, or network state
was changed. Browser dispatch, OS quoting, existing-instance delivery, the
subscription UI and a full application build are not runtime-tested here.
This is a focused parser fix, not evidence that the whole historical issue
can be closed.
