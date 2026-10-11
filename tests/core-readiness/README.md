# Stdout readiness marker regression

Base: `a536846c9c74dd8fb104dce3319cbd9672920bb3`.

## Defect and scope

`CoreProcess` checked each `readAllStandardOutput()` result separately for
`Core listening`. A marker split between callbacks was therefore missed, and
`CoreStarted,<remembered ID>` was never emitted. The main window starts the
remembered profile after that message. Qt exposes process output as a sequential
byte stream; callback boundaries are not a record framing guarantee:
[QProcess channel documentation](https://doc.qt.io/qt-6/qprocess.html#communicating-via-channels).

The candidate retains the last 13 bytes, one byte shorter than the 14-byte
marker, and checks the overlap plus the next chunk. Retaining less than a full
marker prevents later chunks from replaying an already reported marker. The
carry is cleared after the existing `Start()` guard and before a fresh launch.
Normal log display still receives the original chunks; failure/crash marker
handling and the per-callback notification policy are unchanged. A genuine new
marker may still emit `CoreStarted,-1`, as before, but cannot reuse a consumed
remembered profile ID.

This is a confirmed parser defect related to the startup path discussed in
[#141](https://github.com/qr243vbi/nekobox/issues/141), **not an established cause
or full reproduction of that historical report**. The issue provides no chunk
trace or equivalent diagnostic evidence. Autorun arguments, Remember-toggle
persistence, core lifecycle, and other issue hypotheses are outside this patch.

## Run

Prerequisites: Python 3, a GCC/Clang-compatible C++20 compiler, pkg-config, and
Qt6 Core development files. No extra packages are installed by these scripts.

```sh
python3 tests/core-readiness/run_test.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
python3 tests/core-readiness/run_test.py
python3 tests/core-readiness/mutation_checks.py
```

The first command is expected to fail. `--repo PATH` chooses another checkout;
`--ref` reads that checkout's immutable Git source. `CXX` overrides the compiler.
For an extracted Qt toolchain, configure pkg-config's normal search/sysroot
environment variables before running these commands.

## What executes

The runner copies the production stdout/stderr connections and log-limit lambda
verbatim, plus the production pending-ID and byte-array member declarations.
Qt6 `QByteArray`, `QString`, and `QAtomicInt` are real. Synthetic process reads
supply chosen chunks, and synthetic GUI/log/kill boundaries record decisions.
There is no duplicated marker parser in the fixture.

Checks cover every one of the 8,192 partitions of the 14-byte readiness marker,
every two-chunk split with remembered profile IDs 42, 0, and -1, byte-at-a-time
input, missing/partial/case-sensitive markers, no required trailing newline,
empty chunks, large output, near matches, stdout/stderr separation, log-limit
behavior, duplicate prevention, separate objects, stderr interleaving, and preserved existing whole
failure/crash marker decisions. Coalesced markers retain one notification per
callback. Readiness retains precedence over co-occurring whole error markers.
Six deliberate mutations must be rejected.

The Start reset is checked for source placement only, including a missing-reset
mutation. Neither `Start()` nor `Restart()` executes. This does not verify live
process restarts, timing, QProcess delivery behavior, or operating-system boot.

No app, core, proxy, network, settings store, or lifecycle routine is launched.
This focused extracted-production test is not a full application build or GUI
integration test. The full repository suite was not run for this task because
verification was explicitly limited to isolated readiness parsing.

## Verified result

With Qt 6.8.2 and GCC 14.2.0:

- Base: 32,973 assertions, 24,691 failures (expected split-marker failures).
- Candidate: 32,975 assertions, 0 failures.
- Mutation checks: all 6 caught.
- The actual production `Process.hpp` also passes a Qt6 syntax-only compile.

The candidate has two extra bounded-carry assertions because that member does
not exist in the base. Baseline failures include repeated assertions for the
same lost-readiness consequences; the count is not a count of distinct defects.
