# Full-config DNS bootstrap tag regression

## Bug and bounded fix

`NormalizeFullConfigDnsForRuntime` replaces each non-fake-IP DNS server with the
configured remote DNS, keeping its original tag. Previously it assigned hostname
servers `domain_resolver: dns-local`, then treated any server named `dns-local`
as proof that a local bootstrap already existed. An imported server with that
tag therefore became its own resolver, and no real local resolver was appended.
The pinned core accepts that JSON as options but rejects DNS dependency cycles
when starting its transport manager (the tests here never start it).

The fix reserves the first unused exact tag (`dns-local`, `dns-local-1`, etc.)
before replacing any servers. Generated hostname references and a newly appended
local resolver use that tag. Existing tags/order, fake-IP behavior, remote DNS
overrides, original DNS rule references, and existing final/default-resolver
selection policy are unchanged. Imported local servers are still subject to the
existing override policy. This does not rename user tags or repair duplicate
input tags or arbitrary malformed configurations.

## Native Qt regression

From the repository root with a C++20 compiler and Qt6Core development files:

```sh
python3 tests/test_dns_bootstrap.py --output /tmp/dns-bootstrap.jsonl
```

`QT6_ROOT` can point to an already-extracted Debian-style Qt6 installation. The
runner otherwise uses `pkg-config Qt6Core`. `CXX` and `EXTRA_CXXFLAGS` are
supported. To include undefined-behavior instrumentation:

```sh
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' \
  python3 tests/test_dns_bootstrap.py --output /tmp/dns-bootstrap-ubsan.jsonl
```

The runner compiles complete, unmodified source excerpts of the sanitizer,
normalizer, DNS builder and IP-address helpers against **real Qt**. Only the
settings object that supplies in-memory test values is synthetic. Every case
also runs normalization again from the same original input and compares the
result. The test participates in `python3 -m unittest discover -s tests -v`.
`--repo PATH` and `--ref COMMIT` select alternate/immutable production sources.

Coverage includes:

- Existing `dns-local`, suffix collisions and gaps, similar-looking tags,
  different server order, pre-existing local/IP/fake-IP servers and empty arrays
- TLS, HTTPS, UDP, TCP, QUIC and H3 hostnames; IPv4/IPv6 and local/empty fallbacks
- Preservation of original tags, DNS/route rules and remote override behavior
- Legacy fake-IP cleanup, missing/empty/non-string tags, malformed containers
- Five consecutive normalization passes and deterministic identical-input builds
- Deliberately malformed entries and duplicate input tags retained as invalid

## Pinned Go option and reference checks

Requires a compatible Go compiler and the module cache for `core/server/go.mod`:

```sh
python3 tests/dns-bootstrap/run_options.py /tmp/dns-bootstrap.jsonl
```

`GO` may name an installed Go compiler; normal `GOMODCACHE`/`GOCACHE` settings are
honored. The runner copies the repository's Go module files into a temporary
directory, uses `-mod=readonly`, and explicitly disables network module fetching
and automatic toolchain downloads. It decodes DNS/route options through the real
pinned types with an options-only registry. A test-only graph walk checks tag
uniqueness, final/rule/default resolver references and DNS dependency cycles.
Seven negative controls prove that the validator rejects invalid schemas,
duplicates, dangling references and one-/two-node cycles.

No core, transport, DNS resolver or router is constructed. Passing these checks
is **not** evidence of successful core startup, a working resolver, or a full
application build.

## Determinism and repeated normalization

The runtime caller reparses the saved `customBean->config_simple` before each
normalization. Repeated builds from this original input are deterministic and
do not accumulate servers or write the transformed JSON back to the profile.

The whole function is intentionally **not idempotent**. Feeding its output back
as a new input rewrites the previous bootstrap under the existing remote override
policy and appends one new bootstrap. Five-pass tests require unique generated
tags and safe resolver references at every pass. Recognizing an allegedly owned
local server from its JSON shape would also exempt indistinguishable user-supplied
local servers, changing policy; this narrowly scoped fix does not do that.

## Verification scope

The accompanying local verification found 187 fixture rows / 470 normalizer
outputs, including 282 valid option-decoded passes / 997 servers and one expected
malformed rejection. The unchanged baseline failed 126 subcases; candidate tests
and UBSan passed. Independent review included 103 additional edge cases, including
257 occupied suffixes and embedded-NUL tags. Mutation controls cover hardcoded
references, ignored collisions, one-suffix allocation, missing bootstrap, local
policy expansion, tag renaming and final-policy changes.

The repository-wide discovery command has an existing unrelated import-time
argument-parser error in `test_subscription_update_guard.py`; its standalone
regression passes. Full application configuration remains untested where CMake
is unavailable. No lifecycle, transport/core startup, live DNS, profile/settings
writes, public changes, or deployment are performed by these tests.
