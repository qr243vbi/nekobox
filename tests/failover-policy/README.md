# TUN failover checkbox policy regression

This is a **local source-level candidate** related to
[issue #279](https://github.com/qr243vbi/nekobox/issues/279), against
`a536846c9c74dd8fb104dce3319cbd9672920bb3`. It does not establish end-to-end
failover behavior or fulfill the entire feature request.

## Policy and source evidence

`auto_test_tun_failover` is present in the settings model, serialized setting
mapping, and basic-settings load/save bindings. Its UI label is **Enable TUN
Failover** and its tooltip says it automatically switches when a **TUN mode**
proxy fails. `ProxyAutoTester::isTunFailoverEnabled()` reads this setting, but
baseline `HandleProxyFailure()` never consults it.

The candidate preserves failure marking and pool removal, then permits the
existing active-proxy failover branch only when:

- Proxy mode is active, preserving existing behavior regardless of this checkbox;
  or
- TUN mode is active and the TUN failover checkbox is checked.

An inactive failed profile never initiates failover. With TUN failover unchecked,
this branch does not select a replacement, emit a failover/no-backup signal, queue
a switching/no-backup UI notification, or reset the active failure count.
Failure bookkeeping still occurs.

Source anchors:

- `src/gharqad/stats/autotester/ProxyAutoTester.cpp`: failure handler, setting
  getter, pool selection and pruning
- `src/nekobox/ui/setting/dialog_basic_settings.ui`: checkbox label and tooltip
- `src/gharqad/ui/setting/dialog_basic_settings.cpp`: checkbox load/save bindings
- `src/nekobox/dataStore/DataStore.hpp`: default setting value
- `src/gharqad/dataStore/Configs.cpp`: serialized setting mapping

Issue #279 requests optional automatic switching and lowest-latency selection
within the active group. It had no comments when checked on 2026-10-11. This
candidate addresses only the ignored TUN-specific checkbox. The existing
separate no-active-profile auto-start path and pool selection order are unchanged.

## What the test executes

The runner extracts these **five actual production definitions verbatim** from
the working source or `git show --ref` source:

1. `HandleProxyFailure`
2. `GetNextWorkingProxy`
3. `isTunFailoverEnabled`
4. `getPoolSize`
5. `pruneWorkingPool`

They are compiled as C++20 with real Qt6Core containers/strings and
`-Wall -Wextra -Werror`. Test-only boundaries replace all application effects:
settings and profiles are plain in-memory structs; `Save()` increments a counter;
`profile_start()` appends arguments to an in-memory vector; UI callbacks are kept
in a vector and later invoked against that inert window; signals and logs are
in-memory recorders. No actual application headers are included or linked.

No NekoBox constructor, timer/event loop, core, RPC client, profile manager,
settings store, health request, real profile start, or network operation runs.
Qt's actual signal delivery, application settings persistence, thread safety,
queued-callback races, and real failover are **unverified**. A change of checkbox
between separate decisions is tested only by assigning an in-memory boolean.

## Coverage

36 scenarios / 377 assertions:

- All 32 combinations of TUN/proxy mode, checked/unchecked, active/inactive
  failed ID, backup/empty pool, and present/missing failed profile
- Duplicate failed IDs removed, failure bookkeeping and signal contents retained
- UI callback is initially queued, then invokes only the inert switch recorder
- Current checkbox value read on the next decision
- Missing/unhealthy backup pruned by the extracted production code
- Null main-window boundary remains safe

The baseline produces 19 failed assertions. Candidate produces zero.
Six mutations prove the tests reject:

- Ignoring the checkbox (19 failed assertions)
- Accidentally gating proxy mode too (20)
- Reversing checkbox polarity (42)
- Ignoring the active-profile condition (48)
- Skipping failure bookkeeping when TUN failover is off (24)
- Ignoring the setting getter's actual value (19)

Mutation results must be clean assertion failures, not compiler errors or crashes.

## Run without installing anything

With an already extracted Qt6 development root:

```sh
python3 tests/failover-policy/test_failover_policy.py --qt-root /path/to/qt-root
python3 tests/failover-policy/mutation_checks.py --qt-root /path/to/qt-root
```

Or omit `--qt-root` if Qt6Core development files are already available through
`pkg-config`. The runner prints source SHA256 and Qt compile/runtime versions.
It does not install packages. `--keep-generated /path/policy.cpp` saves the exact
assembled test translation unit for inspection.

For an explicit baseline check (exit status 1 expected):

```sh
python3 tests/failover-policy/test_failover_policy.py \
  --qt-root /path/to/qt-root \
  --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
```

Only these inert policy tests were run for this candidate. The full application
build and repository-wide tests were not run, because this audit explicitly
excludes real lifecycle/persistence/network diagnostics and unrelated execution.
