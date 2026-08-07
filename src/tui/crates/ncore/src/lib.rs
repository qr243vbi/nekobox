//! `ncore` — Ported proxy logic layer (data model, store, config builder, subscriptions).
//!
//! This crate reimplements the C++ "middle layer" of NekoBox in Rust:
//!
//! - **model** — `ProxyEntity`, `Group`, `RoutingChain`, `RouteRule`, `DataStore`, `ConfigItem`
//! - **store** — GUI-compatible `.cfg` load/save (JSON, flat-file format)
//! - **config** — ConfigBuilder: model → sing-box JSON
//! - **sub** — share-link parsing + subscription updaters

pub mod model;
pub mod store;
pub mod config;
pub mod sub;

pub use model::*;
pub use store::*;
