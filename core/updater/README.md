# Windows NSIS updater tests

NSIS requires `/D=<destination>` to be the unquoted final command-line option,
including when the destination contains spaces. Passing it as an ordinary Go
argument quotes it, so NSIS can ignore the requested installation directory.
The updater constructs only this NSIS command explicitly; the executable still
uses Go's Windows quoting, and no command shell is involved. Paths must be
absolute and cannot contain quotes or control characters.

## Offline checks on any host

From the repository root, with a trusted Go compiler on PATH:

```sh
python3 -m unittest discover -s tests -p 'test_windows_updater_nsis.py' -v
```

Set `GO_TEST_BINARY=/path/to/go` if needed. The test runs the real argument helper
and extracts the production `main` and `LaunchInstaller` functions unchanged.
Only OS, process, flag and time boundaries are replaced with in-process fakes.
It verifies that invalid commands and failed installers cannot restart the old
application. It never runs an installer, package manager, PowerShell or NekoBox,
and disables Go module/network downloads.

The argument helper can also be tested directly from this directory:

```sh
go test -v nsis_args.go nsis_args_test.go
```

## Windows constructor checks

From this directory on Windows:

```sh
go test -v .
```

These tests inspect `exec.Cmd` without starting it. They cover executable
quoting, drive and UNC destinations, Unicode, cleaned paths, both Chocolatey
flags, and invalid paths. Cross-compiling with `GOOS=windows go test -c` checks
compilation only; it does not execute these tests or validate Windows/UAC/NSIS
runtime behavior. Full installer runtime testing remains a separate check.

The change is limited to direct NSIS command construction and its failure path.
Existing winget and Chocolatey-script failure handling is unchanged.
