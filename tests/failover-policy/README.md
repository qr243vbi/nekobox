# Failover policy and queued-switch regression tests

Related to [issue #279](https://github.com/qr243vbi/nekobox/issues/279).
These tests cover the TUN checkbox and cancellation of queued failover requests;
they do not fulfill lowest-latency selection or establish real TUN connectivity.

The TUN checkbox gates TUN failover while proxy-mode behavior stays unchanged.
Failure marking, persistence requests and removal from the working pool still
occur when switching is disabled. Before a queued switch executes, production
checks the tester lifetime/generation, auto-test setting, core/shutdown state,
active profile ID, current TUN policy and backup existence/health/pool membership.
`Stop()` invalidates pending requests. Pool accesses and health-status accesses
inside the tester use a shared recursive mutex; RPC and profile-start calls run
outside that mutex.

## What executes

The runner extracts production definitions verbatim: `Stop`, `isEnabled`,
`canDeliverFailover`, `isBackupAvailable`, `GetWorkingProxies`, `IsProxyWorking`,
`updateWorkingPool`, `HandleProxyFailure`, `GetNextWorkingProxy`,
`isTunFailoverEnabled`, `getPoolSize`, and `pruneWorkingPool`. New helpers are
optional when testing older revisions. It also extracts production
`runOnUiThread` (renaming only its function declaration to avoid the test post
counter wrapper).

The generated C++20 executable uses real Qt6Core, QCoreApplication, QObject
lifetime, QPointer, QTimer, posted MetaCall events, QRecursiveMutex and worker
threads. Application boundaries are inert: settings/profiles are in-memory,
`Save` is a counter with an optional synchronization hook, window starts record
arguments, and signals/logs are recorders. No application headers or native
MainWindow are linked; no real profile/configuration store is loaded.

## Coverage and limits

The candidate runs 51 scenarios / 416 assertions:

- All 32 combinations of TUN/proxy mode, checkbox value, active/inactive failed
  profile, backup/empty pool, and present/missing failed profile.
- Failure bookkeeping, pruning, changed settings, and absent main window.
- Pending switches cancelled after manual switch/stop, settings changes,
  shutdown/core stop, backup deletion/illness/removal, tester Stop/destruction.
- Only the later generation delivers after Stop; stale no-backup log suppressed.
- Real concurrent pool updates and snapshots (20,000 iterations per thread).
- Backup validation waits for a worker's in-flight failure-status/pool update.

Nine behavioral mutations must fail through assertions, including removal of
policy, backup validity, and generation checks. Historical baseline
`a536846c9c74dd8fb104dce3319cbd9672920bb3` also must fail. Baselines lack the
new backup-validation helper and therefore skip the two concurrency scenarios (their unsynchronized pool
access can crash instead of producing a policy assertion failure).

The surrounding health/RPC workers, profile manager, serialization outside the
tester, native signal delivery, and real tunnel switching remain outside this
harness. This is not a complete concurrency audit. Late worker results and
queued no-active-profile auto-start are separate paths. The native GUI build and
startup-thread regression tests are verified separately. No installed application
or running VPN is changed by these tests.

## Run

```sh
CXX=clang++ python3 tests/failover-policy/test_failover_policy.py
CXX=clang++ python3 tests/failover-policy/mutation_checks.py
CXX='clang++ -fsanitize=undefined -fno-sanitize-recover=all' \
  python3 tests/failover-policy/test_failover_policy.py
```

Qt6Core development files must be available through pkg-config. `--qt-root`
allows an extracted Qt development root; `--keep-generated /path/policy.cpp`
saves the assembled translation unit. `--ref REVISION` tests committed source.
The runner prints source SHA256 and Qt compile/runtime versions. It never
installs packages or accesses the network.
