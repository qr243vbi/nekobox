#!/usr/bin/env python3
"""Test the production subscription-deeplink helpers using real Qt6 Core.

Run: python3 tests/test_subscription_deeplink.py [--repo PATH] [--ref COMMIT]
Requires Python 3, pkg-config, a C++20 compiler, and Qt6 Core development files.
Only the two pure helper functions are compiled; no app, core, protocol-handler
registration, settings, subscription request, or network activity is involved.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


SOURCE = "src/gharqad/ui/mainwindow.cpp"
START = "static QString decodeMaybeBase64Url("
END = "void MainWindow::add_from_deeplink("

CHECKS = r'''
#include <iostream>
int checks = 0, failures = 0;
void expect(const QString &label, const QString &input, const QString &expected) {
    ++checks;
    const auto actual = subscriptionUrlFromInternalDeeplink(input);
    if (actual != expected) {
        ++failures;
        std::cout << "FAIL " << label.toStdString() << '\n'
                  << "  input: " << input.toStdString() << '\n'
                  << "  expected: " << expected.toStdString() << '\n'
                  << "  actual: " << actual.toStdString() << '\n';
    }
}
QString percent(const QString &value) {
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}
QString base64(const QString &value, bool padded = false) {
    return QString::fromLatin1(value.toUtf8().toBase64(
        padded ? QByteArray::Base64UrlEncoding
               : QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
int main() {
    std::cout << "Qt " << qVersion() << '\n';
    const QStringList schemes = {"nekoray", "nekobox", "v2raytun"};
    const QStringList keys = {"url", "uri", "link", "subscription", "sub", "target", "config"};
    const QString http = "http://example.invalid/sub";
    const QString https = "https://example.invalid/sub";
    const QString happ = "happ://example.invalid/synthetic-token";
    // All inner percent escapes must survive exactly one outer decode layer.
    const QString nested = "https://example.invalid/a%2Fb?token=x%26y%3Dz%2525&plus=one+two#tag";
    const QString unicode = QString::fromUtf8("https://example.invalid/路径?name=é&tag=~_");
    const QString urlSafeAlphabet = "https://example.invalid/~~~~~?";
    const QString next = "https://other.invalid/next";
    for (const auto &scheme : schemes) {
        const auto prefix = scheme + "://import";
        for (const auto &key : keys) {
            const auto label = scheme + "/" + key;
            const auto query = prefix + "?" + key + "=";
            expect(label + "/plain-http", query + http, http);
            expect(label + "/plain-https", query + https, https);
            expect(label + "/plain-happ", query + happ, happ);
            expect(label + "/encoded-http", query + percent(http), http);
            expect(label + "/encoded-https", query + percent(https), https);
            expect(label + "/encoded-happ", query + percent(happ), happ);
            expect(label + "/encoded-nested", query + percent(nested), nested);
            expect(label + "/encoded-unicode", query + percent(unicode), unicode);
            expect(label + "/base64url", query + base64(nested), nested);
            expect(label + "/base64url-dash-underscore", query + base64(urlSafeAlphabet), urlSafeAlphabet);
            expect(label + "/base64url-padded", query + base64(https, true), https);
            expect(label + "/encoded-base64url", query + percent(base64(https, true)), https);
            expect(label + "/blank", query, {});
            expect(label + "/empty-whitespace", query + "%20%09", {});
            expect(label + "/invalid-base64", query + "!!!", {});
            expect(label + "/unsupported-url", query + percent("ftp://example.invalid/sub"), {});
            expect(label + "/double-encoded", query + percent(percent(https)), {});
            expect(label + "/encoded-outer-whitespace", " \t" + query + percent(" \t" + https + "\n") + "\r\n", https);
        }
        // Every pair pins fixed key priority, independently of query ordering.
        for (qsizetype first = 0; first < keys.size(); ++first) {
            for (qsizetype later = first + 1; later < keys.size(); ++later) {
                const auto label = scheme + "/priority/" + keys[first] + "/" + keys[later];
                expect(label + "/valid", prefix + "?" + keys[later] + "=" + next + "&" + keys[first] + "=" + percent(nested), nested);
                expect(label + "/invalid-fallback", prefix + "?" + keys[first] + "=!!!&" + keys[later] + "=" + percent(next), next);
                expect(label + "/empty-fallback", prefix + "?" + keys[first] + "=&" + keys[later] + "=" + percent(next), next);
            }
        }
        expect(scheme + "/query-before-path-fragment", prefix + "/" + percent(next) + "?url=" + percent(nested) + "#" + percent(http), nested);
        expect(scheme + "/path-before-fragment", prefix + "/" + percent(nested) + "#" + percent(next), nested);
        expect(scheme + "/encoded-path", scheme + ":///" + percent(nested), nested);
        expect(scheme + "/base64url-path", prefix + "/" + base64(nested), nested);
        expect(scheme + "/plain-path", prefix + "/" + https, https);
        expect(scheme + "/encoded-fragment", prefix + "#" + percent(nested), nested);
        expect(scheme + "/base64url-fragment", prefix + "#" + base64(nested), nested);
        expect(scheme + "/empty-query-path-fallback", scheme + ":///" + base64(nested) + "?url=", nested);
        expect(scheme + "/bad-query-fragment-fallback", prefix + "?url=!!!#" + percent(nested), nested);
        expect(scheme + "/uppercase-scheme", scheme.toUpper() + "://import?url=" + percent(https), https);
        expect(scheme + "/unknown-key", prefix + "?unknown=" + percent(https), {});
        expect(scheme + "/uppercase-key", prefix + "?URL=" + percent(https), {});
        expect(scheme + "/empty-link", scheme + "://", {});
        expect(scheme + "/no-candidate", prefix, {});
        // Preserve QUrlQuery's first-value semantics for duplicate query keys.
        expect(scheme + "/duplicate-first-value", prefix + "?url=" + percent(nested) + "&url=" + percent(next), nested);
        expect(scheme + "/duplicate-empty-then-next-key", prefix + "?url=&url=" + percent(nested) + "&uri=" + percent(next), next);
        expect(scheme + "/literal-plus", prefix + "?url=https://example.invalid/sub?token=a+b", "https://example.invalid/sub?token=a+b");
    }
    expect("empty-input", {}, {});
    expect("no-scheme", "import?url=https://example.invalid/sub", {});
    expect("ordinary-http-input", http, {});
    expect("unrelated-scheme", "other://import?url=" + percent(https), {});
    expect("incomplete-percent-input", "nekobox://import?url=%", {});
    expect("incomplete-escape-input", "nekobox://import?url=https%3", {});
    std::cout << "SUMMARY checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--ref", help="Read immutable Git revision instead of working files")
    args = parser.parse_args()
    if args.ref:
        source = subprocess.check_output(
            ["git", "-C", str(args.repo), "show", f"{args.ref}:{SOURCE}"], text=True
        )
    else:
        source = (args.repo / SOURCE).read_text(encoding="utf-8")
    if source.count(START) != 1 or source.count(END) != 1:
        raise SystemExit("Production helper anchors changed; review extraction before running.")
    begin, end = source.index(START), source.index(END)
    if end <= begin:
        raise SystemExit("Production helper anchors are out of order.")
    helpers = source[begin:end]
    probe = subprocess.run(["pkg-config", "--cflags", "--libs", "Qt6Core"], capture_output=True, text=True)
    if probe.returncode:
        raise SystemExit("BLOCKED: Qt6 Core development files and pkg-config are required.\n" + probe.stderr)
    code = "#include <QByteArray>\n#include <QString>\n#include <QStringList>\n#include <QUrl>\n#include <QUrlQuery>\n" + helpers + CHECKS
    print("SOURCE:", args.ref or "working tree", "| helper SHA256:", hashlib.sha256(helpers.encode()).hexdigest(), flush=True)
    with tempfile.TemporaryDirectory(prefix="nekobox-deeplink-") as temp:
        cpp = Path(temp) / "regression.cpp"
        binary = Path(temp) / ("regression.exe" if os.name == "nt" else "regression")
        cpp.write_text(code, encoding="utf-8")
        command = shlex.split(os.environ.get("CXX", "c++"))
        command += ["-std=c++20", "-fPIC", "-Wall", "-Wextra", "-Werror", "-O0", str(cpp), "-o", str(binary), *shlex.split(probe.stdout)]
        subprocess.run(command, check=True, timeout=60)
        return subprocess.run([str(binary)], timeout=15).returncode


if __name__ == "__main__":
    raise SystemExit(main())
