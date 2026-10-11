# Cache download regression tests

`DownloadFile` must keep the destination unchanged until an HTTP download has
finished and its output file has closed successfully. Failed attempts must leave
no temporary file or false cache hit, and the next attempt must be able to retry.

## Run the native package tests

With the Go version and dependencies required by `core/server/go.mod`:

```sh
cd core/server
go test -race ./internal
go test ./...
```

## Run without proxy-engine dependencies

From the repository root, with Python 3 and Go installed:

```sh
python3 tests/cache-download/run.py -- -race -count=1 -v
```

Use `--go /absolute/path/to/go` to select a temporary Go toolchain. The runner
copies `DownloadFile`, `copyAndCloseDownload`, `urlToPath`, `fileExists`,
`CacheHttpBool`, and `CacheHttp` verbatim from the current `utils.go` into a
temporary, standard-library-only Go module. It copies the same Go test files
used by the native package. The only stand-in is the unused proxy-client factory;
it panics if called. All requests use a fake `http.RoundTripper`, and all files
use `testing.T.TempDir`. The tests do not open network sockets or modify a real
cache, system proxy, DNS configuration, or user data. Go module downloads are
disabled in this runner.

The focused runner verifies the download/cache functions; it does not compile
the rest of the proxy engine or validate GUI/RPC integration. Tests changing
`http.DefaultTransport` or `ruleset_cachedir` deliberately do not use
`t.Parallel`.

### Reproduce the pre-fix behavior

```sh
git show a536846c9c74dd8fb104dce3319cbd9672920bb3:core/server/internal/utils.go > /tmp/nekobox-utils-before.go
python3 tests/cache-download/run.py --source /tmp/nekobox-utils-before.go -- -v
```

This must fail the destination-preservation, retry and concurrent-download
tests. Helper-specific unit tests run only when the helper exists in the source
under test; baseline failure is established by the unchanged public functions.

## Coverage

- Malformed URLs/requests, transport errors, non-2xx responses and truncated
  response bodies leave an absent destination absent or a good destination intact.
- Successful downloads create nested directories, preserve GET/basic-auth
  behavior and replace an existing longer file without stale trailing bytes.
- A real rename failure (destination is a nonempty directory) preserves its
  contents and removes the downloaded temporary file and directory.
- New files retain the effective permissions of `os.Create` under the current
  umask; replacement files preserve the prior destination's permission bits.
- A failed cache attempt retries successfully; the next call is a cache hit.
- An in-flight failed download cannot expose partial data or overwrite the
  successful result of a concurrent download to the same destination.
- The copy/close helper reports write and close errors and closes the output
  even if copying fails. A copy error remains primary if both operations fail.

## Platform and scope notes

The temporary file is created in a private sibling directory so a successful
rename does not cross filesystems. It is closed before rename. Go's Windows
`os.Rename` uses `MoveFileEx` with `MOVEFILE_REPLACE_EXISTING`; no delete-first
fallback is used. A Windows file lock may still make rename fail, in which case
the previous cache remains. Cross-compilation does not replace Windows runtime
testing. Go does not promise atomic rename on every non-Unix platform.

New cache files use `os.Create`'s 0666 mode filtered by umask, as before; the
staging directory is owner-only. This keeps normal default cache permissions
readable across elevated and ordinary core runs, while existing restrictive
file permissions are preserved. Replacement creates a new inode; ownership,
extended ACLs and other inode metadata are not copied from an existing file.

The cache's existing lookup policy, already-corrupt cache entries, explicit
clear/refresh actions, response-content validation and power-loss durability are
outside this change. Concurrent successful downloads may finish in either order;
this change does not deduplicate or order requests.

References:
- https://go.dev/src/os/file_windows.go
- https://go.dev/src/internal/syscall/windows/syscall_windows.go
- https://pkg.go.dev/os#Rename
- https://pkg.go.dev/os#MkdirTemp
- https://pkg.go.dev/os#Create
