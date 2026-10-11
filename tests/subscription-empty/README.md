# Subscription post-processing completion

Local candidate based on `22b5abad885dfacc4bf6784f96e4aaf01270d9cc`.
No commit, push, publication, real database, network request, or VPN operation is
part of this work. This change is separate from the null-profile subscription
import guard associated with issue #260.

## Defect and bounded change

The invalid-profile stage in `MainWindow::post_update_job` locks a completion
mutex, launches one validator for each profile returned by `FillProfileEnts`,
and locks that mutex again. Only the last validator unlocks it. `FillProfileEnts`
skips IDs that cannot load, so both an empty group and a group containing only
unloadable IDs create no workers. The second lock then cannot complete, leaving
`AsyncUpdate` unable to emit its completion signal or invoke its finish callback.

For populated groups, the original code also unlocks the mutex from a different
thread than the thread that locked it, contrary to the
[QMutex ownership contract](https://doc.qt.io/qt-6/qmutex.html#unlock).
The original pool is created in the update worker with the GUI window as its
parent, contrary to [QObject parent/thread rules](https://doc.qt.io/qt-6/threads-qobject.html#qobject-reentrancy).

Replace that manual completion latch with a parentless, stack-owned pool and
`waitForDone()`. Declare the pool after the captured result list and its access
mutex. The pool joins all its workers before the callback continues, and its
destructor joins already queued workers before captured state is destroyed if
scheduling unwinds. `waitForDone()` and this destructor behavior are available
in [Qt 6.5](https://doc.qt.io/qt-6.5/qthreadpool.html#waitForDone).

No custom maximum thread count, expiry timeout, or priority was configured in
the original block. The replacement keeps the same QThreadPool defaults and
still submits one worker per loaded list entry, including repeated IDs. It
preserves the 3000-ID cap, invalid-profile deletion behavior, zero-deletion log,
later URL-test initiation, background update execution, and GUI completion
delivery. No new timeout, cancellation, or exception-recovery policy is added.

## Run

Requirements: Python 3 and GCC/Clang-compatible C++20 compiler. GCC/Clang UBSan is
enabled; no Qt development package is needed for this explicit adapter test.

```sh
python3 tests/test_subscription_empty_callback.py
python3 tests/subscription-empty/mutation_checks.py
python3 tests/test_subscription_empty_callback.py --repo /path/to/git-checkout --ref 22b5abad885dfacc4bf6784f96e4aaf01270d9cc
```

The runner uses the exact production post-update lambda, `FillProfileEnts`,
`AsyncUpdate`, and subscription menu handler. It fails if the extraction seams
change. Production excerpts are hashed in the output. Import/network preparation,
storage, `IsValid`, and Qt/GUI boundaries are synthetic. The updater job runs on a
real `std::thread`; validation scheduling is a deterministic cooperative model,
not a native QThreadPool. Mutex availability is modeled without pretending to
validate native thread ownership. An impossible wait raises a diagnostic rather
than hanging. Compilation is bounded to 60 seconds and each scenario process to
10 seconds.

Sixteen scenarios each run under FIFO, reverse, and finish-before-wait scheduling:
empty groups; all missing IDs; repeated missing IDs; valid/invalid singletons;
mixed missing/valid/invalid entries; all invalid; repeated loaded IDs; empty and
populated URL-test continuation; disabled validation; exactly 3000 and over-3000
IDs; preservation of the prior duplicate-stage log; repeated updates; and
scheduling-side unwinding after a worker was queued. The last scenario checks
resource lifetime only: it does not claim the application recovers from a thrown
scheduling exception. The duplicate-stage boundary is stubbed and is exercised
only on an empty list, not as a test of the deduplication algorithm.

## Evidence from this environment

- Untouched baseline: 18/45 schedule/scenario runs failed the initial zero-worker
  checks, all with zero completion signals and zero GUI callbacks.
- Final test suite against the same pinned baseline: 42/48 runs fail the
  zero-worker or pool-lifetime/unwinding assertions.
- Final candidate: 48/48 runs pass with UBSan and warnings treated as errors.
- Four source mutations are rejected: omit the join (10 failures), destroy the
  access mutex before the pool (2), launch only one worker (21), and return early
  for empty input (18). These exercise synchronization, captured-state lifetime,
  worker cardinality, and preservation of later stages.
- Existing standalone checks pass: subscription menu guard (27 assertions),
  process restart lock (53), ProfileFilter (12 cases and 12 rejected mutations),
  stream settings (26 assertions), DNS ASCII adapter (774 checks), link runtime
  adapter (251 checks), and Windows deployment (20 tests).
- `python3 -m unittest discover -s tests -v` runs 38 tests and reports one existing
  import error in `test_subscription_update_guard.py`, whose top-level argument
  parser rejects unittest's command-line arguments. Its standalone run passes.
- Full application/native Qt verification is **not completed**: this environment
  has Qt6 runtime libraries but lacks Qt development headers/pkg-config metadata
  and the `cmake` executable. Native Qt thread ownership, actual pool scheduling,
  GUI event delivery, and platform builds remain unverified.

## Deliberately unchanged risks

The patch is restricted to completion synchronization. `IsValid` still recursively
validates chain members without a visited set, and a non-chain null bean can be
dereferenced on its ordinary builder path. Core/RPC failure still returns false,
which existing invalid-profile cleanup treats as grounds for deletion. These
are source-level observations, not reproduced defects in this test suite.

Ordinary RPC silence already has configured connect/send/receive budgets of
2/5/20 seconds in `RPC.cpp`; it is not accurate to describe every RPC call as
unbounded. The pool join cannot make a validator that never returns finish.
Throwing inside a validator remains governed by the existing application/Qt
exception behavior. An arbitrary wait timeout would permit workers to outlive
their captured stack state, so none is introduced.
