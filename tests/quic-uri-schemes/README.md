# Reject unhandled QUIC URI schemes

Base: upstream main `1c35d5fed1f167dcdb4c549bc2079dc5dd0a1da4`.

The factory normalizes punctuation before selecting a bean, but QUIC's URI
parser recognizes specific strings. An unhandled scheme previously reached
`return true` without parsing address or credentials. For example,
`hysteria-2://synthetic-auth@example.invalid:8443` was accepted as a Hysteria2
profile at `127.0.0.1:1080` with an empty password.

The two-line production change returns false when no existing parser branch
matches. It adds no URI aliases or protocol support and does not change the
recognized Hysteria, Hysteria2 or TUIC parser branches. Factory normalization
for other formats is unchanged. Authority-port validation and Hysteria2's
omitted-port default are separate changes.

## Run

Requires Python 3, a C++20 compiler, pkg-config and Qt6 Core development files.
No dependencies are installed. Set normal pkg-config sysroot variables if
using an extracted Qt toolchain.

```sh
python3 tests/quic-uri-schemes/run_test.py
python3 tests/quic-uri-schemes/run_test.py --ref BASE_COMMIT
python3 tests/quic-uri-schemes/mutation_checks.py
EXTRA_CXXFLAGS='-fsanitize=undefined -fno-sanitize-recover=all' python3 tests/quic-uri-schemes/run_test.py
```

`--repo PATH` selects source; `--ref COMMIT` reads immutable source;
`--keep-generated FILE` saves the compiled translation unit. Source hashes and
Qt version are printed. Verified native Qt6.8.2 result: 1,140 assertions,
46 expected baseline failures and zero candidate failures. The mutant restoring
success for unmatched schemes must compile and fail runtime checks.

Coverage includes actual factory selection, unsupported punctuation-normalized
schemes, direct parser rejection, Base64/JSON-string-array wrappers, and mixed
bad/good lines. Controls retain scalar/omitted ports, credentials and generated
config fields for supported Hysteria/Hysteria2/TUIC URI spellings over reserved
DNS, IPv4 and IPv6 addresses. Omitted-port values are deliberately not asserted;
PR323 independently owns the Hysteria2 port-443 default.

## Fidelity and limits

The runner extracts the real RawUpdater dispatch, factory, entity defaults,
bean construction, validity/type/display lookup, AddProxy body, full QUIC
parser and config builder, plus their production helpers. QString, QUrl,
QUrlQuery, JSON and Base64 operations use native Qt6. The regression does not
model scheme dispatch or the parser in test code.

Imports remain in memory; AddProxy uses an empty ignore map. Persistence is
inert, Load throws, unrelated formats/bean paths throw, enum storage is a text
stand-in at its default network state, and unrelated stream postprocessing is
a counting boundary. That postprocessor does not modify QUIC fields.

No application, core, network, user profile, setting or GUI is opened or run.
This is a focused extracted-production test, not a full application/Windows
build or live-connectivity test. The broader parser and exporter behavior is
outside this two-line rejection fix.
