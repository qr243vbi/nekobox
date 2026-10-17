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
python3 tests/detour-chain/mutation_checks.py
```

On Windows, run from a developer prompt with `CXX=cl` and `QT_DIR` set to the
Qt kit root (for example `D:/Qt/6.10.3/msvc2022_64`) instead of pkg-config.

`--ref` reads immutable source through `git show`; `--keep-generated PATH` saves
the compiled translation unit. Ten mutations must compile, run, and fail a
runtime check; compilation failure never counts as a killed mutation.

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
