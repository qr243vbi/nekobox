# Node single-key indexing regression

This inert, offline regression compiles real production `Node`/`EnumFieldName`
declarations and selected production method bodies against Qt6 Core. There are
no Node constructor, map, container, or index adapters. It exercises ordinary
in-memory synthetic data only.

## Defect and repair

Both `Node::at(const EnumFieldName&)` overloads call `this->at({t})`. The
single-element braced argument resolves to the same `EnumFieldName` overload,
not the `QList<EnumFieldName>` alternative. GCC 14.2 diagnoses both calls with
`-Winfinite-recursion`. Consequently named `operator[]` and the production
`QJsonObject` constructor (which calls `at(key)`) also reach that recursive path.

The two-line production correction names the intended argument type explicitly:

```cpp
return this->at(QList<EnumFieldName>{t});
```

Both const and mutable wrappers now delegate to the existing list-key overload.
No declaration, return type, normalization, insertion, or alternate-key policy
changes. The boolean conversion and map-range lifetime issues are separate and
are not changed or hidden by this regression.

## Running

Requires Python 3, a C++20 compiler supporting `-Winfinite-recursion`, and
already-available Qt6 Core development files. Nothing is installed. Tested with
GCC 14.2 and Qt 6.8.2.

```sh
python3 tests/node-index/test_node_index.py --qt-root /path/to/qt-root
python3 tests/node-index/mutation_checks.py --qt-root /path/to/qt-root
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' \
  python3 tests/node-index/test_node_index.py --qt-root /path/to/qt-root
EXTRA_CXXFLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  python3 tests/node-index/test_node_index.py --qt-root /path/to/qt-root
```

Without `--qt-root`, the runner uses installed `pkg-config Qt6Core` metadata;
`QT6_ROOT` is also accepted. `CXX` and `EXTRA_CXXFLAGS` support existing compiler
and instrumentation choices. `--keep-generated /tmp/node-index.cpp` retains the
extracted translation unit. Source hashes are printed for reproducibility.

The immutable failing baseline is:

```sh
python3 tests/node-index/test_node_index.py \
  --ref a536846c9c74dd8fb104dce3319cbd9672920bb3 --qt-root /path/to/qt-root
```

### Safe red/green proof

The original baseline is deliberately stopped at compilation by
`-Werror=infinite-recursion`. It reports both exact recursive production call
sites and returns 1, without running an unsafe executable or exhausting a
stack. Unrelated build failures return 2 and are not regression evidence. The
candidate compiles under the same guard and runs 149 runtime checks. Existing
signed/unsigned array-size comparison and deprecated-copy warnings are
suppressed; neither warning is related to overload resolution.

Mutation scoring requires a passing unmodified control. Three mutants restore
both/only-const/only-mutable recursion, and must produce the expected number of
compiler recursion diagnostics with no runtime execution. Four other mutants
misroute only one wrapper to a different key or numeric indexing. Those must
compile and fail actual runtime assertions; a compiler/launch/extraction error
is not counted as runtime detection. Mutation copies are temporary.

## Coverage

- Named `at` and `operator[]`, const and mutable, existing and missing keys
- Exact reference identity against explicit list-key access and mutations of
  the underlying stored value
- Production name normalization (case, underscores, hyphens), empty string keys,
  QString/C-string conversion, and absence of duplicate normalized entries
- Const missing lookups remain undefined without insertion; mutable missing
  lookups insert null at the requested key
- Existing multi-key fallback order and first-key insertion behavior
- Numeric array lookup, extension, null fill, const bounds, and numeric map misses
- Every index family on scalars, null, undefined, and mismatched map/array types
- Actual Qt JSON parsing followed by production QJsonObject/QJsonValue
  constructors, parseJsonValue, fromVariantMap, nested objects/arrays, scalar
  preservation through strict getters, and map copy-on-write isolation

## Scope and limits

This is a source-extracted regression, not a complete application build. It uses
the full production overload declarations so overload resolution is genuine;
every executed Node and EnumFieldName body is extracted unchanged. The unused
`KeyValueRange` declaration is forward-declared only. Map-range iteration,
serialization, quoting, boolean coercion, and empty mutable alternate-key-list
access are outside this test. In particular, this fix does not prove all map
serialization or end-to-end import paths are safe.

No core process, settings, user profile, network connection, or lifecycle test
runs. Full-suite discovery is intentionally excluded by the audit's scope, so
focused results must not be represented as a full-suite or full-app pass.

## Recorded focused result

- Baseline: both recursive wrappers diagnosed at compile time; no executable run.
- Candidate: 149/149 runtime checks pass with Qt 6.8.2 and GCC 14.2.
- UBSan: 149/149 pass with non-recovering undefined-behavior checks.
- ASan: 149/149 pass with `ASAN_OPTIONS=detect_leaks=0`. Default LeakSanitizer
  cannot operate under this executor's ptrace environment and terminates; leak
  checking is unsupported here, not a passing check.
- Mutations: 7/7 detected, separating compiler evidence from runtime assertions.
- Main `1c35d5fed1f167dcdb4c549bc2079dc5dd0a1da4`: the three extracted
  production files were independently fetched and their Git blob hashes
  verified. They match the pinned baseline; safe red and patched green repeat.
- Combined with the independently prepared boolean correction: index regression
  149/149 and existing boolean regression 217/217 pass. The latter retains its
  documented map-access adapters; it is a compatibility check, not extra proof
  of this indexing fix. The index regression itself has no such adapters.
