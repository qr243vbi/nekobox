//! Generate Rust Thrift bindings from `core/server/gen/libcore.thrift`.
//!
//! Requires the Apache Thrift compiler (`thrift`) on PATH. The generated
//! code targets the `thrift` runtime crate.

use std::path::Path;
use std::process::Command;

fn main() {
    // Path relative to this crate's manifest dir:
    // crates/nrpc → crates → tui → src → <repo root>
    let thrift_file = "../../../../core/server/gen/libcore.thrift";
    println!("cargo:rerun-if-changed={thrift_file}");

    let out_dir = std::env::var("OUT_DIR").expect("OUT_DIR not set");

    let status = Command::new("thrift")
        .args(["-gen", "rs", "-out", &out_dir, thrift_file])
        .status()
        .expect(
            "failed to run `thrift` — install the Apache Thrift compiler \
             (e.g. `pacman -S thrift` / `apt install thrift-compiler`)",
        );
    assert!(status.success(), "thrift codegen failed for {thrift_file}");

    let generated = Path::new(&out_dir).join("libcore.rs");
    assert!(
        generated.exists(),
        "thrift did not produce {}",
        generated.display()
    );

    // The generated file starts with inner attributes (`#![...]`), which are
    // not allowed inside an inline `mod gen { include!(...) }` block.
    // Strip them into a cleaned copy; the enclosing module carries
    // equivalent outer `#[allow(...)]` attributes.
    let src = std::fs::read_to_string(&generated).expect("read generated bindings");
    let cleaned: String = src
        .lines()
        .filter(|line| !line.trim_start().starts_with("#!["))
        .collect::<Vec<_>>()
        .join("\n");
    let cleaned_path = Path::new(&out_dir).join("libcore_clean.rs");
    std::fs::write(&cleaned_path, cleaned).expect("write cleaned bindings");
}
