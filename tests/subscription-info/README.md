# Subscription metadata display regressions

From the repository root:

```sh
python3 tests/test_subscription_info.py
python3 tests/test_subscription_info.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
python3 tests/subscription-info/mutation_checks.py
```

Requirements: Python 3, pkg-config, Qt6 Core development headers/libraries, and
GCC or Clang with C++20 and UndefinedBehaviorSanitizer support. Set `CXX` to choose
a compiler. A locally extracted Qt development tree can be selected with
`PKG_CONFIG_PATH` and `PKG_CONFIG_SYSROOT_DIR`.

The runner extracts `ParseSubInfo`, `ReadableSize`, `DisplayTime`, the real
`EnumFieldName` declaration/comparison methods, the response-header map conversion,
and `NetworkRequestHelper::GetHeader` directly from production sources. It compiles
those excerpts against real Qt6 Core. No alternate parser or string/date adapter
is used. `--ref` reads immutable production source while using the same fixtures.

The 39 display cases cover canonical metadata, optional fields, zero sentinels,
whitespace and semicolon separation, exact key/value boundaries, partial counters,
malformed/negative/overflowing values, sum/time overflow, duplicate keys, and empty
input. Five additional checks cover HTTP header-name casing and absent headers.
Each display case runs separately so the original overflow failures do not hide
other regressions. Five mutation checks reject restoration of the total gate,
epoch fallback, missing-total/unlimited conflation, or removal of either numeric
overflow guard.

Scope: in-memory metadata only. These are helper and header-lookup tests, not a
full application or HTTP-library integration build, GUI snapshot, subscription
fetch, profile import, storage, settings, lifecycle, persistence, or VPN test.

Display contract:

- Keep the existing translated `Used: %1 Remain: %2 Expire: %3` row.
- `total=0` still shows unlimited remaining quota (`∞`). Missing or invalid total
  remains unknown (`-`) rather than asserting unlimited service.
- Missing, zero, invalid or unrepresentable expiry shows `-`, never Unix epoch.
- A single omitted upload/download counter retains the previous zero fallback.
  If a supplied counter is malformed or their sum is not representable, usage
  and finite remaining quota are unknown. Both counters absent are also unknown.
- Only complete non-negative decimal fields delimited by semicolons are used;
  surrounding whitespace is allowed. First occurrence wins, including invalid
  duplicate fields, so later duplicates cannot disguise a malformed first value.
- Empty input and input with no valid recognized fields return an empty string.

The optional-field and zero-expiry checks address a proven current display bug.
They do not establish the historical cause of issue #183 without its original
response header and version-specific reproduction.
