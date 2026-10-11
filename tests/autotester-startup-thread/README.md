# Auto-tester startup thread regression

Related to [issue #279](https://github.com/qr243vbi/nekobox/issues/279) and the
reported absence of automatic TUN switching. This verifies one prerequisite:
periodic backup and active health checks actually start after RPC setup.

The main-window constructor invokes `setup_rpc()` through `runOnThread` on
`DS_cores`. That method calls `ProxyAutoTester::Start()`, but the tester and its
two QTimers were constructed on the GUI thread. Qt refuses to start those timers
from the core thread. The initial test can execute once while both periodic
timers remain inactive, preventing later active-node failure detection.

The test compiles the verbatim production tester header with moc, constructor,
destructor, Start/Stop, configuration getters, timer slots and RPC setup. It uses
real QCoreApplication, QThread and QTimer objects. Only traffic loops, RPC client
construction, logging, and the test/health work at timer callback entry are inert
boundaries. No settings, installed app, TUN interface, route or network request
is touched.

Seven cases cover worker startup in TUN/proxy modes, GUI startup, disabled
settings, disabling before queued delivery, destruction before queued delivery,
and stopping periodic callbacks. Tests check both timers are active, their
production intervals are retained, repeated test/health callbacks execute on
the owner thread, and Qt emits no thread warnings. After checking the original
intervals, the fixture accelerates timers to 5 ms to keep the test short.

```sh
python -m unittest discover -s tests -p 'test_autotester_startup_thread.py' -v
AUTOTESTER_REF=3b72239e python -m unittest discover -s tests -p 'test_autotester_startup_thread.py' -v
```

The baseline fails five of seven cases with `Timers cannot be started from
another thread`; both timers remain inactive. The fix passes all seven cases.
Queued startup uses the tester as its context so destruction cancels the call,
and rechecks current settings when it reaches the GUI thread.

This test does not prove health-RPC correctness, pool selection, actual profile
switching, backup reachability or a live TUN failover. Those require separate
coverage; this change does not fulfill the entire #279 feature request.
