# `profile_stop` duplicate-name compile regression

The production fix removes only the later `stoppingName` declaration from
`MainWindow::profile_stop`. The earlier snapshot still comes from
`stopping = runningCopy()`, and the existing worker captures it by value for its
log message. No stop control flow, synchronization, state, or cleanup is changed.

## Run

Requirements: Python 3 and a GCC/Clang-compatible C++20 driver (`CXX`, or `g++`).
`CXX` accepts shell-style words; this runner does not accept MSVC flags.

```sh
python3 tests/test_profile_stop_compile.py
python3 tests/test_profile_stop_compile.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
```

The second command deliberately tests the broken baseline and must fail. On GCC
14.2, its compiler diagnostic reports a conflicting declaration at line 937 and
the prior declaration at line 885. The fixed source passes both tests:

- The exact function definition, extracted without rewriting its body, passes
  `-std=c++20 -Wall -Wextra -Werror -fsyntax-only`.
- Focused source checks retain the snapshot from `stopping`, its position before
  worker dispatch, the worker's value capture, and its use in the log message.

The runner prints the extracted function's SHA256 and pads the temporary source
with blank lines so compiler diagnostic line numbers match the selected source
revision. Extraction requires the unique current signature and a closing brace
at column zero; a changed extraction seam needs a test update.

## Boundaries

`stubs.hpp` supplies synthetic declarations for Qt and application types. Only
the generic `connect` template needs an empty definition. There is no `main`,
linking, or execution: the check does not run a stop operation, a worker, GUI
events, RPC, a profile, or a VPN. Its source assertions describe bindings rather
than demonstrating runtime behavior.

This test catches the local C++ redeclaration and protects the existing snapshot
binding. It is not a native Qt/MSVC application build and does not establish
thread safety, object lifetime, memory use, shutdown behavior, or full CI status.
