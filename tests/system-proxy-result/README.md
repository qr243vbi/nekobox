# System-proxy result regression

Run from the repository root:

```sh
python3 tests/test_system_proxy_result.py
python3 tests/system-proxy-result/mutation_checks.py
```

Requires Python 3, a GCC/Clang-compatible C++20 compiler (`CXX`, default `g++`),
`pkg-config`, and Qt6Core development files. An existing Debian-style extracted
Qt installation can be used with `--qt-prefix /path/to/root`. No dependencies are
installed by these scripts.

To demonstrate the original failure with the same test fixture:

```sh
python3 tests/test_system_proxy_result.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
```

## What executes

The runner extracts these production bodies without rewriting them:

- `MainWindow::set_spmode_system_proxy`
- `Client::EnableSystemProxy`, `Client::DisableSystemProxy`, `Client::onError`
- The RPC `Status` structure and `CHECK` / `NOT_OK` macros

Qt's actual `QString` and `QStringList` are used. The only RPC macro replaced is
`CHANNEL`: its transport/service responses are synthetic. The wrappers still
perform their real transport-status/service-error split. The source hashes are
printed so a run can be linked to the exact extracted code.

The fixture captures settings saves, log messages, and refresh observations in
memory. The legacy C++ configurator is a call recorder. It never opens a socket,
invokes OS proxy APIs, launches the application/core, or writes user settings.
There are no real proxy, DNS, VPN, or profile changes.

## Coverage

The runner builds and executes normal RPC, `DEBUG_MODE` RPC, and
`USE_CPP_PROXY_CONFIGURATOR` configurations. It covers:

- Service errors with successful transport, in both enable and disable directions
- Success, transport failure, and a stopped core
- Previous state, returned checkbox state, and state seen by refresh
- `save=true` / `save=false`, remember-last-profile on/off, and unrelated modes
- Error detail retention and absence of duplicate transport-error logs
- No-op requests, a successful retry after failure, HTTP/mixed/disabled inbounds
- Unchanged legacy configurator behavior with recorded calls

Mutation checks drop either service result, remove failure handling or logging,
and lose the prior state. Each must fail assertions in a disposable source copy.

## Limits

This is focused source-linked result-contract coverage, not a full application,
GUI, Thrift wire, Windows, or OS proxy integration test. Save and refresh are
observers, not the production persistence/UI implementations. The Go handler
contract is established by source inspection, not by this C++ harness. Retaining
the previously accepted application state does not verify or repair any partial
OS change after a failed system-proxy operation.

The defect overlaps symptoms in issues #169 and #173, but those reports lack
failure details needed to establish this as their original cause.
