#!/usr/bin/env python3
"""Compile verbatim production tab/sync/save/model code with real Qt6 widgets.

The constructor is a focused assembly of production initialization and signal
connections, not the complete application. See README.md for excluded services.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent


def between(source, start, end):
    if source.count(start) != 1:
        raise ValueError(f"Expected one start marker: {start}")
    left = source.index(start)
    return source[left:source.index(end, left + len(start))]


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f"Expected one function: {signature}")
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def generate(root, build):
    paths = {
        'editor': root / 'src/gharqad/ui/setting/RouteItem.cpp',
        'editor_header': root / 'src/nekobox/ui/setting/RouteItem.h',
        'model': root / 'src/gharqad/dataStore/RouteEntity.cpp',
        'model_header': root / 'src/nekobox/dataStore/RouteEntity.h',
        'ui': root / 'src/nekobox/ui/setting/RouteItem.ui',
        'autocomplete_cpp': root / 'src/3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.cpp',
        'autocomplete_header': root / 'src/3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp',
    }
    text = {key: path.read_text() for key, path in paths.items()}
    editor, model, header = text['editor'], text['model'], text['model_header']
    chunks = {}
    chunks['enums'] = between(header, '    enum outboundID', '    QString get_rule_set_name_1')
    chunks['rule_fields'] = between(header, '        QString name =', '        [[nodiscard]] QJsonObject get_rule_json')
    chunks['box_members'] = between(text['editor_header'], '    AutoCompleteTextEdit* directIp;', '    [[nodiscard]] int getIndexOf')
    chunks['chain_initialization'] = between(editor, '    chain = std::make_shared<Configs::RoutingChain>(*routeChain);', '    // add the default rule')
    chunks['boxes_setup'] = between(editor, '    directIp = new AutoCompleteTextEdit', '    connect(ui->custom_routes_button')
    chunks['tab_connection'] = between(editor, '    connect(ui->tabWidget', '    connect(ui->route_import_json')
    chunks['edit_connections'] = between(editor, '    connect(ui->rule_name,', '    deleteShortcut =')
    chunks['chain_copy'] = function(model, 'RoutingChain::RoutingChain(const RoutingChain& other)')
    chunks['rule_selection'] = between(function(editor, 'void RouteItem::updateRuleSection()'),
                                       '    if (currentIndex == -1) return;', '    auto currentAttr =')
    model_functions = [
        'QJsonArray get_as_array(', 'bool isValidStrArray(',
        'RouteRule::RouteRule()', 'RouteRule::RouteRule(const RouteRule& other)',
        'QJsonObject RouteRule::get_rule_json(', 'QStringList filterEmpty(',
        'void RouteRule::set_field_value(', 'bool RouteRule::isEmpty()',
        'QString RoutingChain::GetSimpleRules(', 'QString RoutingChain::UpdateSimpleRules(',
        'bool RoutingChain::add_simple_rule(', 'bool RoutingChain::add_simple_address_rule(',
        'bool RoutingChain::add_simple_process_rule(',
    ]
    for signature in model_functions:
        chunks[signature] = function(model, signature)
    for signature in ['static void splitSimpleRules(', 'void RouteItem::loadSimpleBoxes()',
                      'QString RouteItem::commitSimpleBoxes()', 'void RouteItem::accept()',
                      'void RouteItem::updateRouteItemsView()']:
        chunks[signature] = function(editor, signature)
    template = (HERE / 'fixture.cpp.in').read_text()
    generated = template
    for key, value in chunks.items():
        generated = generated.replace('@' + key + '@', value)
    # Function collections are still verbatim: no source expressions are changed.
    generated = generated.replace('@MODEL_FUNCTIONS@', '\n\n'.join(chunks[key] for key in model_functions))
    generated = generated.replace('@EDITOR_FUNCTIONS@', '\n\n'.join(chunks[key] for key in [
        'static void splitSimpleRules(', 'void RouteItem::loadSimpleBoxes()',
        'QString RouteItem::commitSimpleBoxes()', 'void RouteItem::accept()',
        'void RouteItem::updateRouteItemsView()']))
    (build / 'regression.cpp').write_text(generated)
    manifest = {
        'source_root': str(root),
        'source_sha256': {key: hashlib.sha256(path.read_bytes()).hexdigest() for key, path in paths.items()},
        'segment_sha256': {key: hashlib.sha256(value.encode()).hexdigest() for key, value in chunks.items()},
        'boundary': 'Verbatim production bodies/fields/selected constructor blocks; focused constructor orchestration, synthetic application services, a bounds-checked production selection lookup, and no-op remaining Advanced display helpers. Production autocomplete class compiled unchanged. Real Qt6 UI, signals, model data types, JSON, and dialogs; emitted in-memory chain reopen, not disk persistence.',
    }
    (build / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=HERE.parent.parent)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--extract-only', action='store_true')
    parser.add_argument('--qt-prefix', type=Path, help='Optional extracted Debian package root')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    paths = generate(args.source_root.resolve(), build)
    if args.extract_only:
        print('EXTRACTED verbatim production segments; not compiled or executed')
        return 0
    if args.qt_prefix:
        prefix = args.qt_prefix.resolve()
        include = prefix / 'usr/include/x86_64-linux-gnu/qt6'
        lib = prefix / 'usr/lib/x86_64-linux-gnu'
        uic, moc = (prefix / 'usr/lib/qt6/libexec' / name for name in ('uic', 'moc'))
        flags = [f'-I{include}', *(f'-I{include / name}' for name in ('QtCore', 'QtGui', 'QtWidgets')),
                 f'-L{lib}', f'-Wl,-rpath,{lib}', '-l:libQt6Widgets.so.6', '-l:libQt6Gui.so.6', '-l:libQt6Core.so.6']
    else:
        flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Widgets'], text=True))
        libexec = Path(subprocess.check_output(['pkg-config', '--variable=libexecdir', 'Qt6Core'], text=True).strip())
        uic, moc = libexec / 'uic', libexec / 'moc'
    cxx = os.environ.get('CXX', 'g++')
    commands = [[str(uic), str(paths['ui']), '-o', str(build / 'ui_RouteItem.h')],
                [str(moc), str(paths['autocomplete_header']), '-o', str(build / 'moc_autocomplete.cpp')],
                [str(moc), str(build / 'regression.cpp'), '-o', str(build / 'regression.moc')],
                [cxx, '-std=c++20', '-g', '-O0', '-fPIC', '-Wall', '-Wextra',
                 '-Wno-unused-parameter', f'-I{args.source_root.resolve() / "src"}', str(build / 'regression.cpp'),
                 str(paths['autocomplete_cpp']), str(build / 'moc_autocomplete.cpp'), '-o', str(build / 'route-tab-regression'), *flags]]
    for command in commands:
        print('+', shlex.join(command), flush=True)
        subprocess.run(command, check=True)
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
    runtime = build / 'runtime'
    runtime.mkdir(mode=0o700, exist_ok=True)
    env['XDG_RUNTIME_DIR'] = str(runtime)
    cache = build / 'cache'
    cache.mkdir(exist_ok=True)
    env['XDG_CACHE_HOME'] = str(cache)
    result = subprocess.run([str(build / 'route-tab-regression')], env=env)
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
