# Autorun app-data argument regression

Related to [issue #141](https://github.com/qr243vbi/nekobox/issues/141).
Baseline: `a536846c9c74dd8fb104dce3319cbd9672920bb3`.

## Defect and scope

Windows autorun previously emitted only the executable and `-tray`, discarding
`-appdata`. Unix autorun preserved the flag but discarded an explicit directory.
Consequently a generated startup command could select a different settings tree.
The patch preserves the effective flag and explicit directory on both platforms,
and quotes executable/arguments for their respective command-line formats.
Explicit relative directories are resolved before main changes the working
directory. An omitted or empty directory still selects the existing default.

This is a deterministic argument-serialization defect, not proof of the original
reporter's core-startup failure. Existing autorun entries are not automatically
rewritten; a user must re-enable startup with the patched application to replace
a previously generated entry. No change is made to core readiness, profile
selection, shutdown, settings races, or autorun enable/disable side effects.

## Run

Requires Python 3, a C++17 compiler, pkg-config, and Qt6Core development files.

```sh
python3 tests/autorun-arguments/test_arguments.py
python3 tests/autorun-arguments/test_arguments.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
python3 tests/autorun-arguments/mutation_checks.py
```

The test compiles the exact production pure command serializers and app-data
parser fragment. On the baseline, it extracts the Unix argument-building
fragment from the autorun writer without compiling any file-write code. Synthetic
inputs include portable/default/custom app-data modes, relative/absolute/empty
paths, spaces, Unicode, Windows trailing backslashes/UNC, and Unix quotes,
backslashes, dollar signs, backticks, percentages, tabs, and newlines. Assertions
compare decoded command arguments and parsed directories with expected values.
Six mutations must fail by runtime assertions, not compile or extraction errors.

All source/compiler outputs and test working directories are temporary. No real
settings, autostart files, registry entries, applications, cores, tunnels, or
network connections are accessed. The fixture substitutes DataStore and the
application path; it does not compile the complete application.

## Evidence and limits

- Native Qt6Core fixture: candidate 68/68 cases; baseline 27/68 cases.
- Six serializer/parser mutations rejected.
- Windows command decoding uses an independent test implementation of documented
  CRT argument rules on Linux. This is not native Windows API or startup testing.
- Unix decoding models Desktop Entry string escapes, Exec quoting, and literal
  percent expansion. This is not desktop-session autostart integration.
- Independent review also checked all 36 Unix commands with
  `desktop-file-validate` and GLib key-file/shell-argument parsers. These pass.
  A separate GIO 2.84.4 loader check rejects percent-containing executable paths,
  even when escaped; percent in an app-data argument is accepted. The old
  unescaped commands also have field-expansion problems, so this does not
  establish a working-baseline regression. The patch does not resolve that
  platform limitation, and serializer round trips must not be represented as
  proof that such executable paths launch successfully.
- Full application build was unavailable because CMake is absent. A full suite
  was not run: application lifecycle and persistence-race tests are outside this
  task's permitted scope. Existing native DNS, share-link boolean, and TLS
  regressions were run separately (774, 251, and 91 passing checks).

Format references:
- [Desktop Entry Exec](https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html)
- [Microsoft C command-line parsing](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments)
