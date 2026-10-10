# ECH settings persistence diagnostic

From a repository checkout, run:

```sh
python3 tests/streamsettings/test_ech_persistence.py
```

Requires Python 3 and a C++20 compiler (`CXX`, or `c++`). No network, application
launch, proxy connection, user profile, or credentials are used. Fixture text is
synthetic and deliberately not a valid ECH configuration.

The runner extracts and compiles the production stream settings declarations and
map, `MAP_BODY`/`NEW_MAP`/`STOP_MAP`/`ADD_MAP`, `_put_store`, applicable typed field
accessors, and `JsonStore::ToJson` / `JsonStore::FromJson` without editing their
bodies. It tests map-driven JSON-object roundtrips, enabled/disabled values,
multiline config retention, imported query names, old profiles missing the keys,
multiple object instances using the same cached field map, explicit false
loading into a previously true field, clearing text, and rejecting wrong JSON
value types.

Default mode uses explicit standard-library adapters for Qt strings, containers,
and JSON values. Hashing uses key identity. The unrelated transport enums are
stand-ins. This mode is a focused source-extracted diagnostic, not a native Qt
application, actual disk/database persistence, binary serialization, or GUI test.

With Qt6 Core development headers/libraries and pkg-config metadata installed:

```sh
python3 tests/streamsettings/test_ech_persistence.py --qt
```

That mode uses real Qt containers, MD5 hashing, and JSON byte encoding/parsing.
It still isolates the class/serializer excerpts and unrelated transport enums;
it does not build the complete application. This mode was not executed here:
`pkg-config --cflags --libs Qt6Core` failed because development metadata was not
found. This does not imply Qt6 runtime libraries are absent.

To check an immutable source revision from any Git checkout:

```sh
python3 tests/streamsettings/test_ech_persistence.py --repo /path/to/nekobox --ref 5561d8a4041d9caa3e3ec2a3f8a6b8940d59966d
```

Expected on that baseline: 26 checks, 14 failures. With the three-field map patch:
26 checks, zero failures. Removing each added entry independently fails 5, 4,
and 5 checks respectively (`ech_config`, `enable_ech`, `query_server_name`).

The patch does not validate ECH transport support or fix other subscription
parsing or editor issues. GUI reopening and binary/database persistence still
need native application validation before any broader resolution claim.
