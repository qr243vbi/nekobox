# nekobox-tui

A keyboard/mouse-driven terminal front-end for NekoBox. It is a drop-in
companion for the Qt GUI: it reads and writes the **same configuration
directory** (`profiles/`, `groups/`, `beans/`, `route_profiles/`,
`nekobox.cfg`, `default_route_profile.cfg`, `window.ini`) in the GUI's own
formats — both the current binary QDataStream layout and legacy JSON — and
drives the same `nekobox_core` over its Thrift RPC.

## Layout

| crate  | role |
|--------|------|
| `nrpc` | synchronous Thrift client for `nekobox_core` + process launcher |
| `ncore`| ported middle layer: data model, `.cfg` store, ConfigBuilder, share links / subscriptions |
| `nui`  | the `nekobox-tui` binary (ratatui): menus, profile table, logs/connections/graph panes, dialogs |

## Build

Requires:

- a stable Rust toolchain (see `rust-toolchain.toml`)
- the **Apache Thrift compiler** on `PATH` (`thrift-compiler` on Debian/Ubuntu,
  `thrift` on Arch) — `nrpc/build.rs` generates bindings from
  `core/server/gen/libcore.thrift` at build time

```sh
cd src/tui
cargo build --release      # binary: target/release/nekobox-tui
cargo test --workspace     # unit + integration tests
cargo clippy --workspace --all-targets -- -D warnings
```

## Run

The TUI needs a running `nekobox_core` RPC endpoint (default
`127.0.0.1:19810`). It can launch one itself:

```sh
# in the same directory the GUI uses as its config dir
nekobox-tui --launch                        # finds nekobox_core in $PATH
nekobox-tui --launch --core-binary /path/to/nekobox_core
```

or connect to an already running core:

```sh
nekobox-tui                                # TCP 127.0.0.1:19810
nekobox-tui --core-port 19811
nekobox-tui --core-uds-path /tmp/nekobox.sock
```

Useful flags:

- `-c, --config-dir <dir>` — configuration directory (default: current
  directory, same as the GUI)
- `--core-address`, `--core-port`, `--core-uds-path`
- `RUST_LOG=debug nekobox-tui` — write a `nekobox-tui.log` next to the CWD

Press `?` inside for the full key map.

## Notes / known limitations

- Routing rules referencing **named** rule sets (resolved by the GUI from the
  downloaded `srslist.json` ruleSetMap) are not resolved yet; URL-referenced
  rule sets work, including the jsdelivr mirrors.
- SIP008 / Clash YAML / sing-box JSON subscription wire formats are not
  parsed; raw link lists (plain or base64) are.
