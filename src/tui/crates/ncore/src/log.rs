//! Port of `src/gharqad/global/LogRouteHelper.cpp` — log line classification
//! and domain extraction, used by the Logs pane's error filter and its
//! "add domain to routing" action.

use regex::Regex;
use std::sync::OnceLock;

/// Whether a core log line looks like an error.
///
/// Mirrors `LogRoute::isErrorLine`.
pub fn is_error_line(line: &str) -> bool {
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| {
        Regex::new(
            r"(?i)(deadline exceeded|context deadline|i/o timeout|connection refused|connection reset|no such host|lookup .+ failed|name or service not known|tls handshake timeout|network is unreachable|operation timed out|dial tcp|dial udp|read tcp|write tcp|\.connect\(\)| handshake failed|remote error|protocol error|exchange failed|unexpected eof|\berror\b|\bfailed\b)",
        )
        .expect("static regex")
    })
    .is_match(line)
}

const DOMAIN: &str = r"[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?)+";

/// Extract candidate domains from a log line, de-duplicated and in
/// first-seen order. Mirrors `LogRoute::extractDomains`.
pub fn extract_domains(text: &str) -> Vec<String> {
    static RES: OnceLock<Vec<Regex>> = OnceLock::new();
    let patterns = RES.get_or_init(|| {
        [
            format!(r"(?i)(?:domain|host|sni|server name)[=:\s]+({DOMAIN})"),
            format!(r"https?://({DOMAIN})"),
            format!(r"(?i)(?:to |for |via |@)({DOMAIN})(?::\d+)?"),
            format!(r"(?i)dial (?:tcp|udp)(?:4|6)? ({DOMAIN})(?::\d+)?"),
        ]
        .iter()
        .map(|p| Regex::new(p).expect("static regex"))
        .collect()
    });

    let mut seen = std::collections::HashSet::new();
    let mut out = Vec::new();
    for re in patterns {
        for caps in re.captures_iter(text) {
            let candidate = caps[1].trim().to_lowercase();
            if looks_like_domain(&candidate) && seen.insert(candidate.clone()) {
                out.push(candidate);
            }
        }
    }
    out
}

/// Mirrors `looksLikeDomain`: 4..=253 chars, contains a dot, not an IPv4
/// literal, not a bracketed IPv6 address.
fn looks_like_domain(dom: &str) -> bool {
    if dom.len() < 4 || dom.len() > 253 || !dom.contains('.') || dom.starts_with('[') {
        return false;
    }
    if looks_like_ipv4(dom) {
        return false;
    }
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| Regex::new(&format!(r"^{DOMAIN}$")).expect("static regex"))
        .is_match(dom)
}

fn looks_like_ipv4(value: &str) -> bool {
    let parts: Vec<&str> = value.split('.').collect();
    parts.len() == 4
        && parts
            .iter()
            .all(|p| !p.is_empty() && p.len() <= 3 && p.parse::<u8>().is_ok())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn detects_error_lines() {
        assert!(is_error_line("dial tcp 1.2.3.4:443: i/o timeout"));
        assert!(is_error_line("DNS lookup example.com failed"));
        assert!(!is_error_line("inbound/mixed[mixed-in]: tcp connection"));
    }

    #[test]
    fn extracts_domains_and_skips_ips() {
        let line = "outbound/direct: dial tcp api.example.com:443 failed";
        assert_eq!(extract_domains(line), vec!["api.example.com"]);
        assert!(extract_domains("dial tcp 192.168.1.1:443").is_empty());
    }

    #[test]
    fn deduplicates_across_patterns() {
        // Matches both the `host=` and the `to ` pattern.
        let line = "connect to cdn.example.org host=cdn.example.org";
        assert_eq!(extract_domains(line), vec!["cdn.example.org"]);
    }
}
