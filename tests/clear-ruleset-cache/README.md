# Clear ruleset-cache command contract

These offline regressions are related to the Clear → Update workaround suggested
in issue #174. They do not implement selected-profile caching or claim to solve
the original report, and they do not change download refresh policy.

The production RPC handler must honor `clear`, stop before cache lookup or HTTP,
and propagate filesystem failures. The UI helper must report transport/missing
reply failures so the menu can display a warning. A missing configured directory
is an idempotent success; an unconfigured cache is a reported failure.

Run from the repository root:

    python3 tests/clear-ruleset-cache/run_go_test.py --go go -- -race -v
    python3 tests/clear-ruleset-cache/run_qt_test.py

The Qt runner uses Qt6Widgets through pkg-config by default. A locally extracted
Debian-layout Qt installation can be passed with `--qt-root PATH`. Both runners
support `--repo PATH --ref COMMIT` to test the same fixture against a source
revision. The Go runner disables dependency downloads, uses only the standard
library, and respects an explicit GOCACHE or defaults to its temporary workspace.

The Go harness compiles the exact `server.CacheHTTP` and `ClearRulesets` functions.
It replaces the generated request/result structs with corresponding fields and
stubs the ordinary cache lookup, allowing the clear command to prove that it
never invokes a fetch. Clear filesystem operations run only within temporary
folders containing synthetic files. The unrelated-file and failure sentinels
are never user data.

The Qt harness compiles the exact helper and menu connection and retains the
header declaration. QAction, QMenu, QObject connections and QString are native
Qt6; the RPC and message-box boundaries are synthetic. It never connects to a
core or opens a real application/settings profile. It runs offscreen with
isolated runtime, cache and config paths.

On main a536846c9c74dd8fb104dce3319cbd9672920bb3 the Go harness fails four of
five cases: clear leaves all cache files, falls through to lookup, and suppresses
filesystem/unconfigured errors. The Qt fixture fails both error-notification
cases. The candidate passes both fixtures. No full-app, Thrift transport or
native Windows-runtime pass is implied.
