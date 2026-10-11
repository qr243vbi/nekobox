#!/usr/bin/env python3
"""Offline native-Qt regression for full-config DNS bootstrap tag collisions.

Extracts complete production sanitizer, DNS builder, normalizer and IP helpers.
No DNS, core/transport startup, profile writes or application lifecycle runs.
Use --ref HEAD for a baseline; QT6_ROOT or pkg-config supplies real Qt6Core.
"""
import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent / "dns-bootstrap"
REPO, REF, OUTPUT = ROOT, None, None


def section(source, start, stop):
    if source.count(start) != 1:
        raise AssertionError(f"Expected one source anchor: {start}")
    begin = source.index(start)
    return source[begin:source.index(stop, begin)]


def read(path):
    if REF:
        return subprocess.check_output(
            ["git", "-C", str(REPO), "show", f"{REF}:{path}"], text=True)
    return (REPO / path).read_text()


def qt_flags():
    if os.environ.get("QT6_ROOT"):
        root = Path(os.environ["QT6_ROOT"])
        include = root / "usr/include/x86_64-linux-gnu/qt6"
        lib = root / "usr/lib/x86_64-linux-gnu"
        return [f"-I{include}", f"-I{include}/QtCore", f"-L{lib}",
                f"-Wl,-rpath,{lib}", "-l:libQt6Core.so.6"]
    return shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True))


def server(tag, kind="local", **fields):
    return dict(type=kind, tag=tag, **fields)


def config(servers):
    return {"dns": {"servers": servers},
            "outbounds": [{"tag": "proxy", "type": "socks",
                           "server": "proxy.invalid", "server_port": 1080}],
            "route": {"final": "proxy"}}


# Explicit expected bootstrap allocation; suffix-looking tags are not parsed
# as integers, normalized, trimmed, or renamed.
VARIANTS = [
    ("no_collision", [server("upstream")], "dns-local"),
    ("existing_local", [server("dns-local")], "dns-local-1"),
    ("existing_ip", [server("dns-local", "udp", server="192.0.2.1")], "dns-local-1"),
    ("existing_fakeip", [server("dns-local", "fakeip", inet4_range="198.18.0.0/15")], "dns-local-1"),
    ("suffix_collision", [server("dns-local"), server("dns-local-1")], "dns-local-2"),
    ("suffix_gap", [server("dns-local-3"), server("dns-local"), server("dns-local-1")], "dns-local-2"),
    ("suffix_only", [server("dns-local-1")], "dns-local"),
    ("suffix_lookalikes", [server(x) for x in ["dns-local", "dns-local-0", "dns-local-01", "dns-local-１", "dns-local-1 "]], "dns-local-1"),
    ("local_first", [server("dns-local"), server("upstream")], "dns-local-1"),
    ("local_last", [server("upstream"), server("dns-local")], "dns-local-1"),
    ("fakeip_first", [server("fake", "fakeip", inet4_range="198.18.0.0/15"), server("dns-local")], "dns-local-1"),
    ("empty", [], "dns-local"),
]
REMOTES = [
    ("tls://resolver.invalid", {"type": "tls", "server": "resolver.invalid", "tls": {}}, True),
    ("https://resolver.invalid/dns-query", {"type": "https", "server": "resolver.invalid", "path": "dns-query", "tls": {}}, True),
    ("udp://resolver.invalid:5353", {"type": "udp", "server": "resolver.invalid", "server_port": 5353}, True),
    ("tcp://resolver.invalid", {"type": "tcp", "server": "resolver.invalid"}, True),
    ("quic://resolver.invalid", {"type": "quic", "server": "resolver.invalid"}, True),
    ("h3://resolver.invalid/dns-query", {"type": "h3", "server": "resolver.invalid", "path": "dns-query", "tls": {}}, True),
    ("tls://192.0.2.53", {"type": "tls", "server": "192.0.2.53", "tls": {}}, False),
    ("[2001:db8::53]:5353", {"type": "udp", "server": "2001:db8::53", "server_port": 5353}, False),
    ("", {"type": "tls", "server": "8.8.8.8", "tls": {}}, False),
    ("local", {"type": "tls", "server": "8.8.8.8", "tls": {}}, False),
    ("localhost", {"type": "tls", "server": "8.8.8.8", "tls": {}}, False),
]


class DnsBootstrap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="nekobox-dns-bootstrap-")
        cls.addClassCleanup(cls.temp.cleanup)
        temp = Path(cls.temp.name)
        cb = read("src/gharqad/configs/ConfigBuilder.cpp")
        gu = read("src/gharqad/configs/sub/GroupUpdater.cpp")
        utils = read("src/gharqad/dataStore/Utils.cpp")
        functions = {
            "IsIpAddress": section(utils, "bool IsIpAddressV4(", "\nQVariantMap QMapString2QVariantMap"),
            "BuildDnsObject": section(cb, "QJsonObject BuildDnsObject(QString address, bool tunEnabled)", "\nQJsonObject BuildTunInbound"),
            "import_helpers": section(gu, "static QString jsonFirstString(", "void RawUpdater::update(const QString &str3)"),
            "normalizer": section(cb, "static void NormalizeFullConfigDnsForRuntime(", "\nstatic void NormalizeFullConfigOutboundForRuntime"),
        }
        extracted = "\n".join(functions.values())
        (temp / "production.inc").write_text(extracted)
        cls.binary = temp / "regression"
        command = shlex.split(os.environ.get("CXX", "c++")) + [
            "-std=c++20", "-fPIC", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-O1",
            "-I", str(temp), str(HERE / "driver.cpp"), "-o", str(cls.binary)]
        command += shlex.split(os.environ.get("EXTRA_CXXFLAGS", "")) + qt_flags()
        subprocess.run(command, check=True, timeout=90)
        cls.observations = []
        cls.addClassCleanup(cls.save_observations)
        print(f"SOURCE {REF or 'working tree'}; native Qt6; extracted SHA256 "
              f"{hashlib.sha256(extracted.encode()).hexdigest()}", flush=True)

    @classmethod
    def save_observations(cls):
        if OUTPUT:
            OUTPUT.write_text("".join(json.dumps(x) + "\n" for x in cls.observations))

    def run_row(self, name, source, remote="tls://resolver.invalid", sanitize=True, passes=1, schema=True):
        row = dict(name=name, input=source, remote=remote, sanitize=sanitize, passes=passes, schema=schema)
        done = subprocess.run([str(self.binary)], input=json.dumps(row) + "\n", text=True,
                              capture_output=True, check=True, timeout=10)
        result = json.loads(done.stdout)
        self.observations.append(result)
        self.assertEqual(result["passes"][0], result["repeated_original"], "same original input must be deterministic")
        return result

    def assert_bootstrap(self, actual, expected_tag):
        self.assertIn("dns", actual)
        self.assertIsInstance(actual["dns"], dict)
        self.assertIn("servers", actual["dns"])
        servers = actual["dns"]["servers"]
        self.assertTrue(servers, "a real local bootstrap must be appended")
        self.assertEqual(servers[-1], {"type": "local", "tag": expected_tag})
        self.assertEqual(sum(isinstance(s, dict) and s.get("tag") == expected_tag for s in servers), 1)
        self.assertEqual(sum(isinstance(s, dict) and s.get("type") == "local" for s in servers), 1)
        for item in servers:
            if isinstance(item, dict) and "domain_resolver" in item:
                self.assertEqual(item["domain_resolver"], expected_tag)
                self.assertNotEqual(item["tag"], expected_tag)
        return servers

    def test_remote_policy_and_collision_matrix(self):
        for name, incoming, bootstrap in VARIANTS:
            for remote, expected_remote, needs_resolver in REMOTES:
                with self.subTest(variant=name, remote=remote):
                    row = self.run_row(name + " " + remote, config(incoming), remote)
                    actual = row["actual"]
                    servers = self.assert_bootstrap(actual, bootstrap)
                    self.assertEqual(len(servers), len(incoming) + 1)
                    self.assertEqual([s["tag"] for s in servers[:-1]], [s["tag"] for s in incoming])
                    first_remote = None
                    for source, normalized in zip(incoming, servers):
                        if source["type"] == "fakeip":
                            self.assertEqual(normalized, source)
                        else:
                            expected = dict(expected_remote, tag=source["tag"], detour="proxy")
                            if needs_resolver:
                                expected["domain_resolver"] = bootstrap
                            self.assertEqual(normalized, expected)
                            if first_remote is None:
                                first_remote = source["tag"]
                    if first_remote:
                        self.assertEqual(actual["dns"]["final"], first_remote)
                        self.assertEqual(actual["route"]["default_domain_resolver"], {"server": first_remote})

    def test_original_references_and_order_remain(self):
        source = config([server("dns-local-1"), server("dns-local"), server("remote")])
        source["dns"]["rules"] = [{"domain": ["example.invalid"], "server": "dns-local"},
                                     {"domain_suffix": ["invalid"], "server": "dns-local-1"}]
        source["dns"]["final"] = "remote"
        source["route"]["rules"] = [{"domain": ["example.invalid"], "action": "route", "outbound": "proxy"}]
        row = self.run_row("references", source)
        actual = row["actual"]
        self.assert_bootstrap(actual, "dns-local-2")
        self.assertEqual(actual["dns"]["rules"], row["imported"]["dns"]["rules"])
        self.assertEqual(actual["route"]["rules"], row["imported"]["route"]["rules"])
        self.assertEqual(actual["dns"]["final"], "dns-local-1")
        reversed_source = deepcopy(source)
        reversed_source["dns"]["servers"].reverse()
        other = self.run_row("reverse order", reversed_source)["actual"]
        self.assert_bootstrap(other, "dns-local-2")
        self.assertEqual([s["tag"] for s in other["dns"]["servers"][:-1]], ["remote", "dns-local", "dns-local-1"])
        self.assertEqual(other["dns"]["final"], "remote")

    def test_repeated_normalization_keeps_resolvers_safe(self):
        # BuildConfig starts with saved config_simple every time. Deliberately
        # feeding generated output back is also safe, but not idempotent: the
        # existing override policy rewrites the previous bootstrap on each pass.
        for name, incoming, _ in VARIANTS:
            for remote in ("tls://resolver.invalid", "tls://192.0.2.53"):
                with self.subTest(variant=name, remote=remote):
                    row = self.run_row("repeated " + name + " " + remote, config(incoming), remote, passes=5)
                    for index, actual in enumerate(row["passes"]):
                        servers = actual.get("dns", {}).get("servers", [])
                        self.assertTrue(servers, "a real local bootstrap must be appended on every pass")
                        self.assert_bootstrap(actual, servers[-1]["tag"])
                        self.assertEqual(len(servers), len(incoming) + index + 1)
                        self.assertEqual(len({s["tag"] for s in servers}), len(servers))
                        if index:
                            self.assertEqual([s["tag"] for s in servers[:-1]],
                                             [s["tag"] for s in row["passes"][index - 1]["dns"]["servers"]])

    def test_fakeip_legacy_and_resolver_fields(self):
        for sanitize in (False, True):
            source = config([dict(tag="dns-local", address="fakeip", inet4_range="198.18.0.0/15",
                                  domain_resolver="old", address_resolver="old", detour="proxy"),
                             dict(tag="dns-local-1", address="local", address_resolver="old")])
            row = self.run_row("legacy fakeip " + str(sanitize), source, sanitize=sanitize)
            servers = self.assert_bootstrap(row["actual"], "dns-local-2")
            self.assertEqual(servers[0], {"tag": "dns-local", "type": "fakeip", "inet4_range": "198.18.0.0/15"})
            self.assertEqual(row["actual"]["dns"]["final"], "dns-local-1")
            self.assertTrue(all("address_resolver" not in s for s in servers))

    def test_empty_missing_and_malformed_container_boundaries(self):
        for index, source in enumerate([{}, {"dns": None}, {"dns": []}, {"dns": {}},
                                       {"dns": {"servers": None}}, {"dns": {"servers": {}}},
                                       {"dns": {"servers": []}}]):
            for sanitize in (False, True):
                with self.subTest(index=index, sanitize=sanitize):
                    row = self.run_row(f"empty boundary {index} {sanitize}", source, sanitize=sanitize)
                    self.assert_bootstrap(row["actual"], "dns-local")
                    self.assertEqual(row["actual"]["dns"]["servers"], [{"type": "local", "tag": "dns-local"}])
                    self.assertNotIn("final", row["actual"]["dns"])
                    self.assertNotIn("default_domain_resolver", row["actual"].get("route", {}))

    def test_nonstring_and_empty_tags_do_not_claim_bootstrap(self):
        for tag in (None, "", 12, True, {}, []):
            for sanitize in (False, True):
                with self.subTest(tag=tag, sanitize=sanitize):
                    row = self.run_row("nonstring tag " + repr(tag) + str(sanitize),
                                       config([server(tag), server("dns-local-1")]), sanitize=sanitize)
                    servers = self.assert_bootstrap(row["actual"], "dns-local")
                    self.assertEqual([s["tag"] for s in servers], ["dns-0", "dns-local-1", "dns-local"])

    def test_malformed_entries_preserved_for_core_validation(self):
        # This fix neither repairs duplicate user tags nor silently drops bad
        # direct-normalizer entries; core schema/reference checks still reject.
        source = config([None, 12, True, [], "dns-local", server("dns-local"), server("dns-local")])
        row = self.run_row("malformed entries direct", source, sanitize=False, schema=False)
        servers = self.assert_bootstrap(row["actual"], "dns-local-1")
        self.assertEqual(servers[:5], source["dns"]["servers"][:5])
        self.assertEqual([s["tag"] for s in servers[5:]], ["dns-local", "dns-local", "dns-local-1"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--ref")
    parser.add_argument("--output", type=Path, help="Keep generated rows for pinned option decoding")
    args = parser.parse_args()
    REPO, REF, OUTPUT = args.repo.resolve(), args.ref, args.output
    unittest.main(argv=[__file__], verbosity=2)
