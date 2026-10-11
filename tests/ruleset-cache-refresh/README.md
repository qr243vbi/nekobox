# Ruleset-cache refresh results

This refresh-only follow-up is based on main
`1c35d5fed1f167dcdb4c549bc2079dc5dd0a1da4`, where PR #305's staged-download
preservation and PR #314's clear-command/error-reporting fix are already merged.
An explicit update must fetch every map URL again, replace only successful
downloads, and warn when any request fails or there is nothing to update.
Ordinary startup lookups keep reusing cached files.

## Compatible command contract

The Thrift request gains field 5, `optional bool refresh`. Missing means false.
The UI sets the optional field with `__set_refresh(true)` and always leaves
`clear=false`. Empty URLs fail before RPC. The core rejects contradictory
clear+refresh commands without clearing/downloading; clear-only remains unchanged.
Refresh never deletes an old file before its replacement is ready.

Refresh success requires transport success, a reply, a nonempty path and
`exists=true`. The existing field records a new download, not general cache
availability. An old core skips field 5 and can return a stale cache hit; its
`exists=false` response therefore warns instead of falsely claiming an update.
An old core's successful first download may pass because it really fetched new
content. Requests to original and intermediate clear-only cores never activate
clear, including when a map contains null/empty values. Old clients with a new
core retain ordinary lookup behavior because omitted refresh defaults false.

A completed batch continues after failed URLs and reports success only when all
URLs in a nonempty batch succeeded. Null/empty entries cause a warning without
issuing an RPC; other entries still run.

## Run

From the repository root:

    python3 tests/ruleset-cache-refresh/run_go_test.py --go go -- -race -v
    python3 tests/ruleset-cache-refresh/run_qt_test.py
    python3 tests/ruleset-cache-refresh/mutation_checks.py --go go
    python3 tests/ruleset-cache-refresh/run_wire_test.py --go go

From core/server with cached project dependencies:

    go test -race ./gen ./internal
    go test -race ./...

Qt6Widgets is discovered with pkg-config; `--qt-root PATH` selects an extracted
Debian-layout Qt installation. Source runners accept `--repo PATH --ref COMMIT`.
The wire runner accepts `--legacy-ref COMMIT` (default: main a536846). It needs
that Git object locally. It disables module downloads, so Apache Thrift v0.24.0
must already be in GOMODCACHE. Runners use temporary build caches unless GOCACHE
is supplied. All fixtures are offline and use synthetic data.

## What runs

- The Go command fixture compiles production CacheHTTP, DownloadFile, its close
  helper, path mapping, lookup, refresh and clear functions verbatim. Its small
  request struct has the same field/accessor semantics; generated-wire behavior
  is separately tested. The proxy factory panics if used. HTTP is a fake
  RoundTripper, with t.TempDir cache files and no sockets. Historical original
  and clear-only handler bodies are retained unchanged except receiver names.
- The Qt fixture compiles the production helper/update connection unchanged.
  Qt menus, actions, strings, containers and mutexes are native. RPC, schedulers,
  modal dialogs and progress-rendering boundaries are synthetic. It runs
  offscreen with isolated runtime/cache/config paths. Schedulers are synchronous,
  so no real cross-thread behavior is established.
- Native Go generated-binding tests cover missing/false/true optional fields,
  binary field identity, unchanged clear, equality, and wrong-type skipping.
  The wire runner compiles current and historical generated codecs, checking
  new-to-old and old-to-new binary compatibility entirely in memory.

The Go request block was generated using the unmodified Apache Thrift 0.24.0
Go generator (implemented in C++) and an equivalent field AST. Generating the original four-field
AST reproduced the existing block byte-for-byte before field 5 was added.
The normal build still generates C++ from the complete Thrift schema. Independent
checks also generated the C++ request and exercised defaults, setters and binary
round trips with the actual Apache runtime under UBSan.

## Evidence and limits

Original main reproduces unchanged cached bytes with no refresh request and
unconditional UI success on failed/missing replies or empty maps. The updated
fixtures also rejected a discarded draft that overloaded clear, catching
mixed-version and empty-value risks before publication. The candidate's safe
optional-flag design passes the command, UI, native binding and legacy-codec
checks, including rejected clear+refresh contradictions and unchanged ordinary
cache hits. Mutation checks protect the requested flags, empty guard, fresh-result
check, batch aggregation, retry and old-file preservation.

The complete Go suite still fails in existing setup/platform areas: missing
`elevated_resolvctl` embed input, generated remote-service imports of `gen`, and
Windows-only winipcfg imports on Linux. Focused gen/internal packages pass.

This does not implement selected-profile caching requested in issue #174,
discover URLs outside ruleSetMap, validate downloaded rulesets, alter path
mapping/proxy selection/timeouts, or resolve unrelated thread/lifecycle concerns.
No full-app, actual RPC socket or native Windows-runtime pass is claimed.
