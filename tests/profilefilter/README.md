# ProfileFilter portable regression checks

Requirements: Python 3 and a GCC/Clang-compatible C++20 compiler. No Qt,
application build, network, user configuration, or running proxy core is needed.

From the repository root:

```sh
python3 tests/profilefilter/test_profilefilter.py
python3 tests/profilefilter/mutation_checks.py
```

Set `CXX` to choose a compatible compiler. `PROFILE_FILTER_SOURCE` can point to
an alternative `ProfileFilter.cpp` for baseline and mutation validation.

## What is actually executed

The runner compiles the production `src/gharqad/dataStore/ProfileFilter.cpp`
unchanged, including its real comparison, factory, `Uniq`, and `Common`
implementations. Production header declarations are copied verbatim except
that the `ProxyEntity.hpp` include is replaced with a synthetic dependency
header. The copied header and compiled executable live in a temporary directory.

The test doubles model only the boundaries needed by this algorithm:

- ASCII `std::string` endpoint fields stand in for `QString`.
- A small `std::vector` adapter implements the used `QList` operations.
- Synthetic bean fields represent credential, transport, and custom-config
  distinctions. All strings are dummy values, never usable credentials.
- Synthetic entity comparison includes metadata and deliberately excludes the
  bean. This mirrors the boundary in `ProxyEntity::_map()`, where the bean is
  not registered. It is not a reimplementation of the real storage comparator.

No production comparison logic is copied into the test doubles.

## Coverage

Twelve cases exercise crossed endpoint fields; strict ordering laws over 34
keys (including null direct keys and both modes); consistency with all equality
and relational operators; different credentials and transports; duplicate beans
with different IDs/names; address-only behavior; the custom-profile exception;
each endpoint field; first/last retention; empty and singleton input; existing
`c_cfg`/`c_out` exclusions; `Common`; and all 24 input permutations of four
synthetic profiles.

The optional mutation script changes temporary copies only. It checks twelve
specific regressions: disjunctive ordering, inverted mode flag, entity comparison
in both/either operator, lost custom exception, ignored type/address/port/mode,
and loss of either existing excluded field.

## Limits

These tests are source-linked algorithm tests, not Qt/storage integration tests.
They do not validate actual `JsonStore::compare`, lazy bean loading, every
protocol's bean map, non-ASCII `QString` behavior, GUI sorting, profile deletion,
Windows behavior, or application startup. They do not establish that the two
other symptoms in issue #115 (column-sort crash and stale selection) are fixed.
A Qt-enabled application build and real in-memory bean tests remain necessary
before claiming full application validation. No real user profiles should be
used or deleted for validation.
