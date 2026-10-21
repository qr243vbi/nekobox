# Optional file log (`NEKOBOX_LOG_FILE`)

`src/nekobox/global/LogFile.hpp` appends what goes to the log tab, the output of
nekobox_core and the Qt messages to the file named by `NEKOBOX_LOG_FILE`. Without
the variable nothing is written.

## Run

Requires Python 3, a C++20 compiler (`CXX`, or `c++`), pkg-config and Qt6 Core
development files. No application, core, profile, setting or network is used.

```sh
python3 tests/log-file/test_log_file.py
python3 tests/log-file/mutation_checks.py
```

On Windows, run from a developer prompt with `CXX=cl` and `QT_DIR` set to the
Qt kit root (for example `D:/Qt/6.10.3/msvc2022_64`) instead of pkg-config.

## What is real and what is a boundary

The production `LogFile.hpp` and the production `sanitizeLog()` body from
`Utils.cpp` are compiled; a stand-in `Utils.hpp` declares only that function.
Each case runs in a fresh process, because the path is resolved once per
process, and the test reads the file the process leaves.

Covered: nothing is written without the variable; every line gets a timestamp,
is trimmed, has colour codes removed, and blank lines are skipped; a relative
path is taken from the start directory even after the working directory
changes; an existing file is appended to; Qt messages are labelled and still
passed to the previous handler; eight threads writing at once keep whole lines;
a path that cannot be opened (a directory) is harmless. Five mutations of the
header must make a case fail.

Where the file log is fed is covered elsewhere: `tests/core-readiness` checks
that core output over the rate limit of the log tab still reaches the file. The
calls in `main.cpp` and in `enqueueLog()` of the main window are not exercised
here; they were checked by running the application with the variable set.
