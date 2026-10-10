#!/usr/bin/env python3
"""Extract unchanged production sorting/model/keeper/proxy bodies for a Qt6 test.

The caller and comparison lambda are copied verbatim on baseline and candidate.
Domain profiles/storage/display and unrelated header geometry are synthetic;
Qt's model/view/cache/selection/signals are real. No C++ Qt behavior is mocked.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-root', type=Path, default=HERE.parent.parent)
parser.add_argument('--build-dir', required=True, type=Path)
parser.add_argument('--extract-only', action='store_true')
args = parser.parse_args()
root, build = args.source_root.resolve(), args.build_dir.resolve()
generated = build / 'production-source'
generated.mkdir(parents=True, exist_ok=True)
paths = {'header': root / 'src/nekobox/ui/mainwindow_table.h',
         'table': root / 'src/gharqad/ui/mainwindow_table.cpp',
         'window': root / 'src/gharqad/ui/mainwindow.cpp',
         'sort_header': root / 'src/nekobox/ui/group/GroupSort.hpp'}
texts = {key: path.read_text() for key, path in paths.items()}
segments = {}

def extract(key, source, start, end):
    text = texts[source]
    if text.count(start) != 1:
        raise SystemExit(f'Changed source layout for {key}; review extraction')
    left = text.index(start)
    right = text.index(end, left + len(start))
    segments[key] = text[left:right]
    return segments[key]

classes = extract('classes', 'header', 'class ColumnFilterProxy :', 'class FilterHeader :')
keeper = extract('keeper', 'table', 'SelectionKeeper::SelectionKeeper(', 'FilterHeader::FilterHeader(')
proxy = extract('proxy', 'table', 'void ColumnFilterProxy::setEnabled(', 'void FilterHeader::setFilterCount(')
count = extract('count', 'table', 'int MyTableModel::rowCount(', 'bool MyTableModel::filterEnabled(')
columns = extract('columns', 'table', 'int MyTableModel::columnCount(', 'int MyTableModel::data_id(')
data_id = extract('data_id', 'table', 'int MyTableModel::data_id(const QModelIndex &index) const', 'const QString invalid =')
# data_id is overloaded; choose the unique exact declaration boundary above.
refresh = extract('refresh', 'table', 'void MyTableModel::refresh(){', 'void MyTableModel::notifyProfileChanged(')
m_data = extract('m_data', 'table', 'std::shared_ptr<Configs::Group> MyTableModel::m_data()', 'QVariant MyTableModel::headerData(')
window = extract('window', 'window', 'void MainWindow::refresh_proxy_list_impl(const int &id,', 'struct ProxyEntityComparator {')
window_refresh = extract('window_refresh', 'window', 'struct ProxyEntityComparator {', '/*\nvoid MainWindow::refresh_table_item(')
helper = ''
if 'void MyTableModel::sortProfiles(' in texts['table']:
    helper = extract('sort_profiles', 'table', 'void MyTableModel::sortProfiles(', 'void MyTableModel::refresh(){')
    model_decl = texts['header'][texts['header'].index('class MyTableModel :'):]
    if 'void sortProfiles(const std::function<bool(int, int)> &lessThan);' not in model_decl:
        raise SystemExit('Missing production model declaration')
    if 'tableModel->sortProfiles(' not in window:
        raise SystemExit('Caller does not use production helper')
else:
    if 'currentGroup->profiles.begin(), currentGroup->profiles.end(),' not in window:
        raise SystemExit('Baseline sort path changed; review extraction')

(generated / 'production_classes.h').write_text(
    '#pragma once\n#include <QSortFilterProxyModel>\n#include <QHash>\n#include <QTableView>\n'
    '#include <QSet>\n#include <QItemSelection>\n#define SELECTION_KEEPER_ROLE Qt::UserRole + 3\n\n' + classes)
(generated / 'GroupSort.hpp').write_text(texts['sort_header'])
(generated / 'production_source.cpp').write_text('#include "native_fixture.h"\n\n' +
    '\n'.join((keeper, proxy, count, columns, data_id, helper, refresh, m_data, window, window_refresh)))
manifest = {'source_root': str(root), 'has_helper': bool(helper),
            'source_sha256': {key: hashlib.sha256(path.read_bytes()).hexdigest() for key, path in paths.items()},
            'segment_sha256': {key: hashlib.sha256(text.encode()).hexdigest() for key, text in segments.items()},
            'transformation': 'Production class declarations and function bodies copied verbatim; fixture supplies synthetic domain and display dependencies.'}
(generated / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest, indent=2), flush=True)
if args.extract_only:
    raise SystemExit(0)
for command in ('cmake', 'ctest'):
    if not shutil.which(command):
        raise SystemExit(f'BLOCKED: {command} unavailable; production C++ was not compiled or run')
for command in (["cmake", "-S", str(HERE), "-B", str(build), f"-DSORT_SOURCE_DIR={generated}", "-DCMAKE_BUILD_TYPE=Debug"],
                ["cmake", "--build", str(build), "--config", "Debug"]):
    result = subprocess.run(command)
    if result.returncode:
        raise SystemExit(result.returncode)
listing = subprocess.run(['ctest', '-C', 'Debug', '--show-only=json-v1'], cwd=build,
                         text=True, stdout=subprocess.PIPE, check=True)
names = [test['name'] for test in json.loads(listing.stdout)['tests']]
if names != ['profile_sort_regression']:
    raise SystemExit(f'Unexpected/empty CTest registration: {names}')
raise SystemExit(subprocess.run(['ctest', '-C', 'Debug', '--output-on-failure'], cwd=build).returncode)
