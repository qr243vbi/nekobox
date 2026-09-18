//! Debug helper: dump a NekoBox .cfg file (binary or JSON).
//!
//! Usage: cargo run -p ncore --example dump_cfg -- <file.cfg> [...]

use ncore::store::binary::{is_binary, parse, BinValue};

fn main() {
    for path in std::env::args().skip(1) {
        let data = std::fs::read(&path).expect("read file");
        println!("== {path}");
        if !is_binary(&data) {
            println!("  (not binary format; {} bytes)", data.len());
            continue;
        }
        match parse(&data) {
            Ok(records) => {
                for (name, value) in &records {
                    println!("  {name}: {}", fmt(value, 1));
                }
            }
            Err(e) => println!("  PARSE ERROR: {e:#}"),
        }
    }
}

fn fmt(v: &BinValue, indent: usize) -> String {
    let pad = "  ".repeat(indent);
    match v {
        BinValue::Int(x) => format!("int {x}"),
        BinValue::Long(x) => format!("long {x}"),
        BinValue::Str(s) => format!("str {s:?}"),
        BinValue::Bool(b) => format!("bool {b}"),
        BinValue::StrList(l) => format!("strlist {l:?}"),
        BinValue::IntList(l) => format!("intlist {l:?}"),
        BinValue::Double(d) => format!("double {d}"),
        BinValue::Enum(s) => format!("enum {s:?}"),
        BinValue::StrMap(m) => format!("strmap {} entries", m.len()),
        BinValue::Store(recs) => {
            let inner: Vec<String> = recs
                .iter()
                .map(|(k, v)| format!("\n{pad}  {k}: {}", fmt(v, indent + 1)))
                .collect();
            format!("store {{{}}}", inner.join(""))
        }
        BinValue::StoreList(l) => format!("storelist {} items", l.len()),
    }
}
