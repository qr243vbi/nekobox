# Unresolved old-profile subscription regression

This is a defensive fix for the null-profile path correlated with
[issue #260](https://github.com/qr243vbi/nekobox/issues/260). It does not establish
why the reporter's stored IDs became unresolved or reproduce a Windows crash.

## Bounded behavior

Incremental subscription updates (`sub_clear == false`, at most 1,000 old list
entries) must resolve each old ID before inserting its profile into the matching
map. A null lookup can mean a failed load, not necessarily an absent record.
The guard therefore retains the unresolved ID and its position in the group,
excludes it from matching/deletion, and allows the remaining update to proceed.
A later successful lookup can reuse the original profile. No storage cleanup or
concurrency behavior is added.

The existing explicit clear mode and automatic clear mode above 1,000 entries
are unchanged. Those modes still clear old group membership. This change does
not promise unresolved-ID retention in clear mode.

`GetProfile` returns null after `LoadProxyEntity` cannot load a valid supported
type. `BatchDeleteProfiles` already skips null lookups and protected running
profiles. `Group::DropNulls` is a separate explicit cleanup operation; invoking
it here would permanently prune membership on a potentially transient failure.
The guard intentionally does not do that.

## Run

From the repository root, with Python 3 and a GCC/Clang-compatible C++20 compiler
and its UndefinedBehaviorSanitizer runtime:

```sh
python3 tests/test_subscription_null_profiles.py
```

`CXX` selects the compiler. To test an immutable source revision while keeping
these fixture files in the current checkout:

```sh
python3 tests/test_subscription_null_profiles.py --repo /path/to/git-checkout \
  --ref 22b5abad885dfacc4bf6784f96e4aaf01270d9cc
```

The runner supports unittest discovery. Each case runs in a separate temporary
executable process with a ten-second timeout and sanitizer recovery disabled.
A source-seam change causes extraction to fail rather than silently omit code.

## Actual production code executed

- The entire `GroupUpdater::Update` method, including its early return and both
  old-profile dereferences
- `RawUpdater::AddProxy`
- `ProfileFilter.cpp` unchanged, with production header declarations retained
- `ProfileManager::AddProfileBatch` and `BatchDeleteProfiles`
- `Group::RemoveProfile` and `HasProfile`

The adapters replace only boundaries: ASCII strings/containers, profile lookup,
synthetic bean content, HTTP response, parsed incoming records, scheduled-job
queue, clock, logging, completion UI, group-save snapshots, and record-drop
observations. The production matching and add/delete decision logic is not
rewritten. Storage jobs execute sequentially after each update; persistence
assertions concern the arguments and final state of these source-linked flows,
not real database durability, worker interleaving or Qt integration.
The synthetic entity `Save` records its assigned ID; it does not reproduce the
real entity's cache eviction or subsequent database reload. Lookup-registry
identity assertions therefore do not establish production cache lifetime.

## Coverage and red/green evidence

Thirteen cases cover one unresolved ID, two distinct unresolved IDs, repeated
unresolved IDs, mixed keep/add/remove/unresolved entries, equivalent valid
profiles with different IDs, unchanged input, 12 groups updated three times,
recovery on a later lookup, the 1,000/1,001-entry boundary, explicit clear mode,
a protected running profile, and a request failure before matching.

On immutable base `22b5abad885dfacc4bf6784f96e4aaf01270d9cc`, nine cases fail
with UBSan null-member-access diagnostics. Single-null inputs reach the later
removal sweep; two-null inputs reach the earlier duplicate branch. Four existing
non-null/clear/early-return cases pass. With the guard, all 13 cases pass
(386 assertions). Unresolved IDs and ordering remain in both live membership
and the synthetic saved snapshot. Valid matching/addition/removal still works,
and unresolved records are never sent to record deletion.

## Limits

No real app, subscription, user record, database, credentials, crash-dump data,
VPN, network request, Windows executable, or proxy core is used. Protocol
parsing, native Qt containers/event loop, real storage failures, concurrency,
and Windows GUI integration still require their own validation. This patch
prevents this source-level null route; it is not evidence that every cause of
multi-subscription instability is fixed.

In particular, the real `MainWindow::post_update_job` callback has separate
behavior when `sub_rm_invalid` is enabled: if `FillProfileEnts` yields no
loadable profiles, it schedules no workers before waiting on its mutex again
(`mainwindow.cpp:700–728` on the pinned base). Static review identified this
existing hang risk; it was not reproduced or changed here. The synthetic
completion callback in this fixture does not validate that path.
