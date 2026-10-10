#!/usr/bin/env python3
"""Extract and compile the actual BuildDnsObject, without a network or VPN.

Default: native Qt6Core (requires development headers and pkg-config).
Explicit --mode ascii-adapter: portable, ASCII-only helper-logic validation.
The adapter is not evidence of Qt6 integration or a full application build.
"""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent


def cases():
    rows = []

    def add(address, server=None, port=None, kind="udp", path=None, expected=None):
        if expected is None:
            expected = {"type": kind, "server": server if server is not None else address}
            if port is not None:
                expected["server_port"] = port
            if path:
                expected["path"] = path
            if kind in ("tls", "https", "h3"):
                expected["tls"] = {}
        rows.append((address, expected))

    # The reported case must fail if the conventional port split is removed.
    for prefix in ("", "udp://", "tcp://", "tls://", "quic://"):
        kind = prefix[:-3] if prefix else "udp"
        for host in ("192.168.5.1", "dns.example"):
            add(prefix + host, host, kind=kind)
            for text, port in (("5335", 5335), ("53", 53), ("1", 1), ("65535", 65535), ("0053", 53)):
                add(prefix + host + ":" + text, host, port, kind)
            # Existing semicolon syntax remains accepted.
            add(prefix + host + ";5335", host, 5335, kind)
        for host in ("2001:db8::1", "::1", "::ffff:192.0.2.1", "fe80::1%eth0"):
            add(prefix + host, host, kind=kind)
            add(prefix + "[" + host + "]", host, kind=kind)
            add(prefix + "[" + host + "]:5335", host, 5335, kind)
            add(prefix + host + ";5335", host, 5335, kind)
        # An unbracketed IPv6 suffix must never become a port.
        add(prefix + "2001:db8::5335", "2001:db8::5335", kind=kind)
        # Invalid conventional ports are preserved in full for core validation.
        for host in ("dns.example", "192.168.5.1", "[2001:db8::1]"):
            for port in ("", "abc", "0", "-1", "65536", "2147483648", "999999999999999999999", "+53", " 53", "53 ", "53x"):
                value = host + ":" + port
                add(prefix + value, value, kind=kind)

    for value in ("", ":53", "[]:53", "[::1", "[::1]suffix:53", "[::1]:53:54", "dns.example:53/path", "[dns.example]:53", "[::1]:", "[::1]extra"):
        add(value)
    # Freeze the legacy parser's behavior, even when not a valid network port.
    # This patch does not claim to validate or repair semicolon inputs.
    for suffix, port, server in (("0", 0, "dns.example"), ("65536", 65536, "dns.example"), ("-2", -2, "dns.example"), ("-1", None, "dns.example"), ("+53", 53, "dns.example"), (" 53 ", 53, "dns.example"), ("abc", None, "dns.example;abc"), ("", None, "dns.example;"), ("2147483648", None, "dns.example;2147483648")):
        add("dns.example;" + suffix, server, port)
    add("dns.example:53;5353", "dns.example:53", 5353)
    add("[2001:db8::1];5335", "[2001:db8::1]", 5335)

    # The shared authority parsing works after HTTPS/H3 path extraction;
    # existing last-segment-only path semantics and TLS shape are unchanged.
    for scheme in ("https", "h3"):
        add(scheme + "://dns.example/dns-query", "dns.example", kind=scheme, path="dns-query")
        add(scheme + "://dns.example:8443/dns-query", "dns.example", 8443, scheme, "dns-query")
        add(scheme + "://[2001:db8::1]:8443/dns-query", "2001:db8::1", 8443, scheme, "dns-query")
        add(scheme + "://dns.example;8443/a/b?x=1", "dns.example", 8443, scheme, "b?x=1")
        add(scheme + "://dns.example/a/b", "dns.example", kind=scheme, path="b")
        add(scheme + "://dns.example/", "dns.example", kind=scheme)
        add(scheme + "://[2001:db8::1]", "2001:db8::1", kind=scheme)
        add(scheme + "://[2001:db8::1]/dns-query", "2001:db8::1", kind=scheme, path="dns-query")
        for host in ("dns.example", "[2001:db8::1]"):
            for port in ("", "abc", "0", "65536", "2147483648", "+53"):
                authority = host + ":" + port
                add(scheme + "://" + authority + "/dns-query", authority, kind=scheme, path="dns-query")
    for value in ("local", "local:anything", "localhost"):
        add(value, expected={"type": "local"})
    for value, interface in (("dhcp://auto", ""), ("dhcp://eth0", "eth0"), ("dhcp://", "")):
        add(value, expected={"type": "dhcp", "interface": interface})
    assert len({address for address, _ in rows}) == len(rows), "Duplicate input"
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "src/gharqad/configs/ConfigBuilder.cpp")
    parser.add_argument("--mode", choices=("qt6", "ascii-adapter"), default="qt6")
    args = parser.parse_args()
    source = args.source.read_text()
    start = source.index("QJsonObject BuildDnsObject(QString address, bool tunEnabled)")
    stop = source.index("\nQJsonObject BuildTunInbound", start)
    function = source[start:stop].strip()
    if not function.endswith("}"):
        raise SystemExit("Cannot identify complete BuildDnsObject")
    flags = []
    if args.mode == "qt6":
        if not shutil.which("pkg-config"):
            raise SystemExit("BLOCKED: pkg-config unavailable; use --mode ascii-adapter for limited checks.")
        qt = subprocess.run(["pkg-config", "--cflags", "--libs", "Qt6Core"], capture_output=True, text=True)
        if qt.returncode:
            raise SystemExit("BLOCKED: Qt6Core development package unavailable; use --mode ascii-adapter for limited checks.")
        flags = shlex.split(qt.stdout)
        headers = '''#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
std::string dump_json(const QJsonObject &o) {
  return QJsonDocument(o).toJson(QJsonDocument::Compact).toStdString();
}
'''
    else:
        headers = (HERE / "dns_qt_ascii_adapter.hpp").read_text()
    driver = '''
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    const bool tunEnabled = line.at(0) == '1';
    std::cout << dump_json(BuildDnsObject(QString(line.c_str() + 1), tunEnabled)) << '\\n';
  }
}
'''
    rows = [(address, tun, expected) for address, expected in cases() for tun in (False, True)]
    with tempfile.TemporaryDirectory(prefix="nekobox-dns-test-") as directory:
        unit = Path(directory) / "dns.cpp"
        binary = Path(directory) / "dns-test"
        unit.write_text("#include <iostream>\n" + headers + "\n" + function + driver)
        # tunEnabled is unused in the existing implementation, before and after.
        command = ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", str(unit), "-o", str(binary)] + flags
        subprocess.run(command, check=True)
        completed = subprocess.run([str(binary)], input="".join(str(int(tun)) + address + "\n" for address, tun, _ in rows), text=True, capture_output=True, check=True)
    outputs = completed.stdout.splitlines()
    if len(outputs) != len(rows):
        raise SystemExit(f"Expected {len(rows)} responses, got {len(outputs)}")
    failed = 0
    for (address, tun, expected), raw in zip(rows, outputs):
        actual = json.loads(raw)
        if actual != expected:
            failed += 1
            if failed <= 20:
                print(f"FAIL address={address!r} tun={tun}: expected={expected}, actual={actual}")
    print(f"Mode: {args.mode}; source: {args.source}")
    print(f"{len(rows) - failed}/{len(rows)} checks passed; {failed} failed ({len(cases())} inputs, two tun flags).")
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())
