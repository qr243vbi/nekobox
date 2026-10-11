# Node boolean conversion regression

This offline regression exercises production `Data::Node` scalar and array
conversion, QVariant/JSON serialization, and selected JSON/YAML import helpers
using real Qt6. The production change is one expression in `ConfigData.cpp`:
`isNumber() > 0` becomes `toNumber() > 0`.

The helper checks supply valid synthetic Node maps through construction/indexing
adapters because the production single-key overload has a separate recursion
concern. Map serialization throws if reached and is not verified. These tests
do not establish end-to-end JSON/YAML subscription import.

## Root cause and conservative policy

The old expression asks whether the Node has a numeric tag. Every numeric value
therefore becomes true, including zero, negatives and NaN; every boolean or
string becomes false. `toBool()` delegates to this conversion and boolean
`toVariant()` delegates to `toBool()`, so a stored true serializes as false.
The strict `getBoolean(default)` accessor reads the tag correctly and is unchanged.

The candidate retains the existing `> 0` operator rather than introducing C++'s
nonzero truth rule. It uses the source's adjacent `toNumber()` implementation:

- Boolean tags map to 1 or 0.
- Numbers retain their value; only positive values compare greater than zero.
  Negative values, zero and NaN are false; positive infinity is true.
- Strings equal to `true` under production `EnumFieldName` normalization map to 1.
  That normalization ignores case, underscores and hyphens, but does not trim
  whitespace. Other strings use actual `QString::toDouble()`.
- Collections retain their original `count() > 0` behavior.
- Null and undefined remain false.

This is a source-supported conservative repair, not proof of a separately
published coercion specification. A policy of treating every nonzero numeric
value as true would require a separate behavior decision. No new string
normalization or numeric parsing is introduced.

## Run

Requires Python 3, a C++20 compiler, and existing Qt6 Core development files.
No package is installed. With an already-extracted Qt sysroot:

```sh
python3 tests/node-boolean/test_node_boolean.py --qt-root /path/to/qt-root
python3 tests/node-boolean/mutation_checks.py --qt-root /path/to/qt-root
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' \
  python3 tests/node-boolean/test_node_boolean.py --qt-root /path/to/qt-root
```

Without `--qt-root`, the runner uses installed `pkg-config Qt6Core` metadata.
`QT6_ROOT` is also supported. To reproduce the immutable red baseline:

```sh
python3 tests/node-boolean/test_node_boolean.py \
  --ref a536846c9c74dd8fb104dce3319cbd9672920bb3 --qt-root /path/to/qt-root
```

`--keep-generated /tmp/node-checks.cpp` retains the compiled source for review.
The runner prints hashes of every extracted source file. Mutation checks operate
on temporary copies, require an unmutated passing control, and count only runtime
assertion failures as detections (not extraction, compilation or launch errors).

## Coverage and limits

The 217 checks include factories and scalar constructors, the toBool alias,
strict-getter defaults, zero/negative/fractional/NaN/infinite numbers, normalized
true and numeric strings, null/undefined, collection emptiness, QVariant value
and type, actual JSON bytes, and nested-array round-trips. Seven mutations cover
the original defect, zero and negative threshold changes, omitted string or
boolean conversion, collection sum instead of count, and lost false values in
serialization. Actual JSON/YAML mux, UDP-over-TCP and TLS helper bodies run for
true and false input, including ECH strict-getter preservation and numeric UOT
versions. YAML helper inputs are synthetic Nodes, not parsed YAML bytes.

Real production declarations, `EnumFieldName` normalization/comparison,
scalar/array constructors and conversions are extracted without body changes.
Object construction and const key lookup have explicit test boundaries to avoid
independent map-access defects. Composite quoting and map serialization throw
if reached. These boundaries do not validate the production map constructor,
key-access recursion or map-range lifetime. No changes to those paths are made.
Import destination structs are minimal test-only boundaries; no protocol bean
lifecycle, core process, settings, user profile, network connection, or full
application is exercised. Full-suite discovery is deliberately not run because
this audit excludes lifecycle/settings tests; no full-suite claim is made.

The adjacent ECH harness now extracts the new `toNumber()` dependency. Its old
`getBoolean()` to `toBool()` mutation is equivalent after an isBool guard with
this repair, so it is replaced with an actual inverted-value mutation. Adapter
numeric parsing fails fast if reached; real Qt checks cover numeric strings.

## Caller impact

Runtime tests prove the conversion and selected helper behavior. The remaining
caller impact below is a static source trace, not a full application test:

- JSON `multiplex.enabled` and YAML `smux.enabled`: true now maps to enabled
  rather than disabled (`Json2Bean.cpp:18`, `Yaml2Bean.cpp:26`).
- JSON/YAML boolean UDP-over-TCP and JSON nested `uot.enabled`: true survives;
  explicit numeric versions keep their independent numeric branch
  (`Json2Bean.cpp:116`, `Yaml2Bean.cpp:12`).
- JSON TLS fragment, record-fragment and insecure flags, plus YAML TLS enable
  and insecure flags, receive the requested boolean value
  (`Json2Bean.cpp:64-69`, `Yaml2Bean.cpp:37-56`). Explicit true for an insecure
  flag starts being honored; absent or false remains false. This is a meaningful
  compatibility/security effect to review before integrating the shared fix.
- Tailscale JSON/YAML ephemeral/routing flags (`TailscaleBean.cpp:60-86`),
  VMess padding/authenticated-length (`VMessBean.cpp:35-67`), TrustTunnel health
  check (`TrustTunnelBean.cpp:39-56`), Snell reuse (`SnellBean.cpp:48-68`),
  WireGuard system/AWG integrated TUN (`WireguardBean.cpp:122-138,407-417`), and
  QUIC MTU/UOT/zero-RTT/TLS-related flags (`QUICBean.cpp:201-292`) call this same
  conversion. These individual bean flows are traced, not newly runtime-tested.
- Generic QUIC and YAML UDP-network helpers also call it
  (`AbstractBeanExtra.hpp:137-139,211-213`).
- Boolean `toVariant()` and recursive `toVariantList()` are runtime-tested.
  `toVariantMap()` also calls `toVariant()` but is excluded because of its
  independent map-range lifetime issue (`ConfigData.cpp:410-443`).
- `QJsonValue::toBool()` and `QVariant::toBool()` calls elsewhere are separate
  Qt APIs and must not be counted as affected Data::Node callers. Share-link
  boolean helper parsing and strict `getBoolean()` are separate as well.

## Recorded focused results

With Qt 6.8.2: baseline 165/217 checks passed (52 expected failures); candidate
217/217 passed; UBSan 217/217 passed. All seven Node mutations were detected.
The adjacent ECH adapter and native-Qt modes each passed 1,248/1,248 checks,
and all 16 ECH mutations were detected. These results do not establish full
application buildability, real import success, or network behavior.
