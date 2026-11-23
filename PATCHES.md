# Patched Windows x64 builds

This is an unofficial fork of [qr243vbi/nekobox](https://github.com/qr243vbi/nekobox).
The original project's GPL-3.0 license and authorship are retained.

The `5.11.28.7-patched.1` release contains the user-tested Windows 11 x64 build.
Its application sources match the tested build; the optional CI workflows and
this documentation have been adapted for the public fork.

## Upstream fixes

The fixes have been submitted separately for upstream review:

- [PR #359: provider manager and initialization](https://github.com/qr243vbi/nekobox/pull/359)
- [PR #360: TUN DNS packet-reader deadlock](https://github.com/qr243vbi/nekobox/pull/360)
- [PR #361: Windows external-core DIRECT rules](https://github.com/qr243vbi/nekobox/pull/361)

The following independent branches are based on upstream commit
`5f6ebb01d28b6ab24c1ddb0ee35b0f66e234b9ef`:

- `codex/upstream-provider-manager`: restore provider manager registration,
  configuration and scoped lifecycle, plus durable traffic counters used by
  QueryStats. Addresses the startup crash reported in
  [upstream issue #357](https://github.com/qr243vbi/nekobox/issues/357).
- `codex/upstream-tun-dns`: keep asynchronous DNS transport connection setup
  off the TUN packet reader. Includes an in-memory regression using the pinned
  transport connection pool. No dependency downgrade is required.
- `codex/upstream-xray-bypass`: prioritize the runtime DIRECT rule for external
  cores and use native Windows process paths. Includes a Qt regression fixture.

The first two branches include the same small compatibility update to an
existing traffic-counter test for sing-box's scoped lifecycle API.

## Additional fork behavior

These Windows usability changes are separate from the upstream bug-fix branches:

- `nekobox.exe` uses `%LOCALAPPDATA%\nekobox\settings` by default.
- `NekoBox-Portable.cmd` / `nekobox.exe -portable` uses adjacent `settings`.
- `-appdata <directory>` selects a custom settings base directory.
- The existing option to hide core connections also covers Xray and routine
  internal log entries. Warnings and errors remain visible. Debug bypasses the
  GUI display filter; the optional file log remains unfiltered.

No settings are migrated automatically. Close other NekoBox instances before
switching builds.

## Validation and building

The release was compiled in GitHub Actions. ZIP contents, AMD64 executable
headers and static DLL imports were checked. The user confirmed startup,
System Proxy, both System and gVisor TUN stacks, and the usability changes on
Windows 11. This does not constitute validation of every protocol or platform.

The manual **Patch regression checks** workflow tests each upstream branch
separately and the combined release sources on Windows. It starts unit tests
and a Qt fixture only, without launching NekoBox or creating a TUN device.

[Successful independent and combined Windows checks](https://github.com/syrHamster/nekobox/actions/runs/45159167852)

The manual **Windows x64 patched build** workflow builds the complete portable
application and uploads an Actions artifact. It does not publish a release.
Other platforms are not built or tested by this fork.
