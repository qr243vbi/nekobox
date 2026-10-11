# Shutdown thread regression

Related report: [NekoBox #255](https://github.com/qr243vbi/nekobox/issues/255).

```sh
python3 -m unittest discover -s tests -p 'test_shutdown_gui_thread.py' -v
SHUTDOWN_REF=74ae0602 python3 -m unittest discover -s tests -p 'test_shutdown_gui_thread.py' -v
CXX=clang++ EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' \
  python3 -m unittest discover -s tests -p 'test_shutdown_gui_thread.py' -v
```

Requires Python 3, a C++20 compiler, and Qt6 Widgets development tools exposed
through pkg-config. Every case runs in its own offscreen QApplication.

The fixture compiles the verbatim production exit and signal handlers, the
production overlay header and implementation, and real Qt timers/dialogs.
Only the overlay's icon lookup and the application cleanup entry are replaced.
The latter records its execution thread and uses a real nested event loop to
represent the event-processing wait already used by profile_stop.

Checks cover normal/fast exits, requests originating on a worker, repeated
requests during cleanup, declined exit, dismissed overlay, window destruction
before a queued worker request is delivered, signal-handler requests from either
thread, a signal handler request during cleanup, and normal exit immediately
followed by a session-end request. Fast/session-end requests must also complete
cleanup before returning to their GUI caller. They require one cleanup on the GUI thread,
the correct normal/fast path, no Qt thread-affinity warnings, and no surviving
overlay. A watchdog rejects a hung event loop.

The pre-fix revision fails all fourteen cases. Deferring GUI-thread cleanup
fails the completion-before-return cases, including a normal exit followed
immediately by a session-end request.

This fixture does not invoke a real operating-system signal handler, RPC,
settings persistence, system DNS/proxy changes, restart/updater executable,
or core process. It does not establish that all application exit hangs have
been resolved on FreeBSD/Windows. Existing synchronous DNS/proxy reset RPCs
can still delay GUI event handling during shutdown.
Native session-end requests reentered during active cleanup are coalesced,
not synchronously awaited inside the nested callback; that platform scenario
is not established by these tests.
