#!/usr/bin/env python3
"""Compile only the production command serializers and app-data flag parser.

Qt6Core/pkg-config and a C++17 compiler are required. No autorun writer, registry,
application, core, network, profile, or lifecycle code is compiled or run.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = 'src/gharqad/sys/AutoRun.cpp'
MAIN = 'src/gharqad/main.cpp'


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError('Expected one function: ' + signature)
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    # These small functions contain no brace literals/comments.
    for end in range(opening, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if depth == 0:
            return source[start:end + 1]
    raise ValueError('Unterminated function: ' + signature)


def desktop_arguments(text):
    # Desktop Entry string-value escapes are decoded before Exec quoting.
    text = text.replace('\\\\', '\x01').replace('\\s', ' ').replace('\\n', '\n').replace('\\r', '\r').replace('\\t', '\t').replace('\x01', '\\')
    tokens, token, quoted, active, pos = [], '', False, False, 0
    while pos < len(text):
        ch = text[pos]
        if ch == '"':
            quoted = not quoted
            active = True
        elif ch == '\\' and pos + 1 < len(text):
            pos += 1
            token += text[pos]
            active = True
        elif ch in ' \t\n' and not quoted:
            if active:
                tokens.append(token)
                token, active = '', False
        else:
            token += ch
            active = True
        pos += 1
    if quoted:
        raise ValueError('Unclosed desktop quote')
    if active:
        tokens.append(token)
    result = []
    for token in tokens:
        i, value = 0, ''
        while i < len(token):
            if token[i] == '%':
                if token[i:i + 2] != '%%':
                    raise ValueError('Unexpected desktop field code: ' + token)
                value += '%'
                i += 2
            else:
                value += token[i]
                i += 1
        result.append(value)
    return result


def windows_arguments(text):
    # Independent implementation of the documented CRT argv grammar, not an
    # invocation of Windows/CommandLineToArgvW. Inputs use native separators.
    result, pos = [], 0
    while pos < len(text):
        while pos < len(text) and text[pos] in ' \t':
            pos += 1
        if pos == len(text):
            break
        arg, quoted = '', False
        while pos < len(text) and (quoted or text[pos] not in ' \t'):
            slashes = 0
            while pos < len(text) and text[pos] == '\\':
                slashes += 1
                pos += 1
            if pos < len(text) and text[pos] == '"':
                arg += '\\' * (slashes // 2)
                if slashes % 2:
                    arg += '"'
                else:
                    quoted = not quoted
                pos += 1
            else:
                arg += '\\' * slashes
                if pos < len(text) and (quoted or text[pos] not in ' \t'):
                    arg += text[pos]
                    pos += 1
        result.append(arg)
    return result


def build_source(source, main):
    code = r'''
#include <QString>
#include <QStringList>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <iostream>
namespace Configs {
struct DataStore { bool flag_use_appdata = false; QString appdataDir; QStringList argv; };
DataStore instance;
DataStore *dataStore = &instance;
}
QString applicationPath;
QString getApplicationPath() { return applicationPath; }
'''
    for signature in ('static QStringList AutoRun_Arguments(', 'static QString Windows_QuoteArgument(', 'static QString DesktopEntry_QuoteArgument('):
        if signature in source:
            code += function(source, signature) + '\n'
    code += function(source, 'QString Windows_GenAutoRunString(') + '\n'
    if 'QString Unix_GenAutoRunString(' in source:
        code += function(source, 'QString Unix_GenAutoRunString(') + '\n'
        unix = source.split('#ifdef Q_OS_UNIX', 1)[1]
        if '<< Unix_GenAutoRunString()' not in unix:
            raise ValueError('Unix writer no longer uses the tested serializer')
    else:
        body = source.split('    QStringList appCmdList;', 1)[1].split('\n    if (enable)', 1)[0]
        code += 'QString Unix_GenAutoRunString() { QStringList appCmdList;' + body + '\nreturn appCmdList.join(" "); }\n'
    flags = main.split('  if (Configs::dataStore->argv.contains("-appdata")) {', 1)[1].split('} if (Configs::dataStore->argv.contains("-deeplink"))', 1)[0]
    code += 'void parseAppData() { if (Configs::dataStore->argv.contains("-appdata")) {' + flags + '} }\n'
    code += r'''
int main() {
    QJsonArray rows;
    const QStringList unixPaths = {"", "/tmp/config", "/tmp/配置 dir", "/tmp/quote\"dir",
        "/tmp/back\\slash", "/tmp/dollar$dir`tick", "/tmp/percent%f%", "/tmp/line\nnext\ttab", "/tmp/end/"};
    const QStringList winPaths = {"", R"(C:\Data)", R"(C:\配置 dir)", R"(C:\Config Space\)", R"(\\server\share\)", R"(C:\percent%f%)"};
    for (bool windows : {false, true}) {
        const auto paths = windows ? winPaths : unixPaths;
        for (bool plainExecutable : {false, true}) {
        for (bool enabled : {false, true}) {
            for (const auto &path : paths) {
                Configs::dataStore->flag_use_appdata = enabled;
                Configs::dataStore->appdataDir = path;
                applicationPath = windows ? R"(C:\Program Files\NekoBox\nekobox.exe)" : "/tmp/Neko Box%/nekobox";
                if (plainExecutable) applicationPath = windows ? R"(C:\NekoBox\nekobox.exe)" : "/tmp/nekobox";
                QStringList expected{applicationPath, "-tray"};
                if (enabled) { expected << "-appdata"; if (!path.isEmpty()) expected << path; }
                auto command = windows ? Windows_GenAutoRunString() : Unix_GenAutoRunString();
                rows.append(QJsonObject{{"kind", windows ? "windows" : "desktop"}, {"command", command}, {"expected", QJsonArray::fromStringList(expected)}});
            }
        }
    }
    }
    for (const QString &path : QStringList{"relative dir", "../parent", "/tmp/custom data", "."}) {
        Configs::instance = {};
        Configs::dataStore->argv = QStringList{"nekobox", "-appdata", path, "-tray"};
        parseAppData();
        rows.append(QJsonObject{{"kind", "parse"}, {"actual", Configs::dataStore->appdataDir}, {"expected", QDir(path).absolutePath()}, {"enabled", Configs::dataStore->flag_use_appdata}});
    }
    for (const QStringList &args : {QStringList{"nekobox", "-appdata"}, QStringList{"nekobox", "-appdata", ""}, QStringList{"nekobox", "-appdata", "-tray"}, QStringList{"nekobox", "-tray"}}) {
        Configs::instance = {};
        Configs::dataStore->argv = args;
        parseAppData();
        rows.append(QJsonObject{{"kind", "parse"}, {"actual", Configs::dataStore->appdataDir}, {"expected", ""}, {"enabled", Configs::dataStore->flag_use_appdata == args.contains("-appdata")}});
    }
    std::cout << QJsonDocument(rows).toJson(QJsonDocument::Compact).constData();
}
'''
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    args = parser.parse_args()
    def read(path):
        return subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    code = build_source(read(SOURCE), read(MAIN))
    with tempfile.TemporaryDirectory(prefix='nekobox-autorun-args-') as name:
        temp = Path(name)
        (temp / 'test.cpp').write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++17', '-fPIC', '-Wall', '-Wextra', '-Werror', str(temp / 'test.cpp'), '-o', str(temp / 'test'), *flags]
        subprocess.run(command, check=True, timeout=60)
        rows = json.loads(subprocess.check_output([str(temp / 'test')], cwd=temp, text=True, timeout=10))
    failed = 0
    for row in rows:
        try:
            if row['kind'] == 'parse':
                actual = row['actual']
                passed = actual == row['expected'] and row['enabled']
            else:
                actual = (windows_arguments if row['kind'] == 'windows' else desktop_arguments)(row['command'])
                passed = actual == row['expected']
        except ValueError as error:
            actual, passed = str(error), False
        if not passed:
            failed += 1
            print('FAIL', row['kind'], 'expected=', repr(row['expected']), 'actual=', repr(actual))
    print(f'SUMMARY {len(rows)} cases; {failed} failures; source={args.ref or "working tree"}')
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
