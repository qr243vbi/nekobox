# Sing-box outbounds linked by `detour`

A sing-box JSON with only `outbounds`/`endpoints` (no `route`, `dns` or
`inbounds`) used to become one independent profile per outbound, and `detour`
was dropped. When an outbound names another outbound of the same input in
`detour`, the importer now creates

- a profile for every outbound on the way (the *hops*), flagged `chain_link`,
- a `chain` profile over them, in the order the client dials them,
- a plain profile for every outbound that nothing is dialed through and that
  has no `detour` itself.

Inputs without a usable `detour` are imported exactly as before.

## Run

Requires Python 3, a C++20 compiler (`CXX`, or `c++`), pkg-config and Qt6 Core
development files. No application, core, profile storage or network is used.

```sh
python3 tests/detour-chain/test_detour_chain.py
python3 tests/detour-chain/test_detour_chain.py --ref <commit>
python3 tests/detour-chain/test_import_compat.py
python3 tests/detour-chain/test_import_compat.py --ref a83f27fc
python3 tests/detour-chain/mutation_checks.py
```

On Windows, run from a developer prompt with `CXX=cl` and `QT_DIR` set to the
Qt kit root (for example `D:/Qt/6.10.3/msvc2022_64`) instead of pkg-config.

`--ref` reads immutable source through `git show`; `--keep-generated PATH` saves
the compiled translation unit. Eighteen mutations (fifteen of the chain import
and duplicate removal, three of the dispatch) must compile, run, and fail a
runtime check; compilation failure never counts as a killed mutation.

## Imports that existed before

`test_import_compat.py` feeds whole input texts to `RawUpdater::update()`, with
the production base64 decoder, splitter, full-config sanitizer and the
sing-box and SIP008 importers, and checks the profiles that come out:

- a full config with `dns` and `route` (plain and as base64) stays one
  `internal-full` profile with its outbounds, `detour` and `route.final`
- outbounds without `detour` (plain and as base64) stay separate profiles
  named by their tags
- share links, also as single and nested base64 with CRLF and empty lines
- a JSON array subscription mixing an outbounds object and a link
- a single outbound object (`custom`/`internal`) and SIP008

The same inputs and expectations pass against a revision without chain import
(`--ref a83f27fc`, 23 checks) and against this branch, which adds three
checks for a chain through the same dispatch (26 checks). Three mutations of
the dispatch must fail it. Clash YAML and WireGuard files are not among its
inputs.

## What is real and what is a boundary

Extracted verbatim from the production sources:

- the `detour` resolution, `RawUpdater::updateSingBox`, `ProfileId` and
  `FinalizeChains` (hop/chain construction, naming, main entry, order, the
  subscription handling of existing profiles)
- `RawUpdater::AddProxy` and the `RawUpdater` declaration
- `ProfileFilterKey` comparison and `ProfileFilter_ent_key`
- `ResolveChainInternal`, to check which end of `ChainBean::list` becomes the
  `proxy` outbound

Test boundaries: profile and bean classes keep the parsed JSON instead of
building real beans, `ProfileManager` keeps profiles in memory (ids are handed
out in order, `ReplaceProfile` keeps the id), and V2Ray-style conversion is the
identity. How a bean turns into a sing-box outbound, and the Routes tab
handling, are not exercised here; they were checked by building the config of
an imported chain in the application in both system proxy and TUN mode.

Covered: the chain + Reality string and its variant without Reality; array
element input; chain order; naming; flags; main entry selection (the first
outbound that is not a hop, whatever its tag or kind); ordering inside the
group; plain outbounds, `direct`, unknown tags, self references and loops;
three-hop chains and a hop
shared by two chains; chains switched off by the caller; subscription update
with unchanged input, with a changed hop (chain keeps its id, old hop is
dropped) and with the chain gone; two inputs with the same loopback port in
one group; chains and hops being kept apart by "remove duplicates".

Added after review: profiles reused by a subscription update take the role
the new input gives them (in both directions, including an update from a
version without chains); a chain keeps its id when entries are reordered
and a shared hop changes at the same time, and a kept chain is relabelled;
"remove duplicates" keeps the hops of every remaining chain (Uniq and
OnlyInSrc_ByPointer as the main window calls them); a detour to a proxy
tagged "direct" builds a chain.
