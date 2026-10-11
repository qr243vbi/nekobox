#!/usr/bin/env python3
"""Verify that focused regressions reject representative production regressions."""
import argparse
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--go', default='go')
parser.add_argument('--qt-root', type=Path)
args = parser.parse_args()
paths = ['src/gharqad/ui/mainwindow_rpc.cpp', 'src/gharqad/ui/mainwindow.cpp',
         'src/nekobox/ui/mainwindow.h', 'core/server/internal/utils.go', 'core/server/server.go']
rpc, window, _, utils, server = paths
cases = [
    ('helper ignores reply/path', rpc, 'return isok && result.has_value() && !result->file_path.empty() && result->exists;', 'return isok;', 'qt'),
    ('update omits refresh flag', rpc, 'req.__set_refresh(true);', 'req.__set_refresh(false);', 'qt'),
    ('update sends destructive clear', rpc, 'req.clear = false;\n    req.__set_refresh(true);', 'req.clear = true;\n    req.__set_refresh(true);', 'qt'),
    ('helper accepts stale legacy hit', rpc, '!result->file_path.empty() && result->exists;', '!result->file_path.empty();', 'qt'),
    ('helper sends empty URL', rpc, '    if (url.isEmpty()) {\n        return false;\n    }\n    bool isok = false;', '    bool isok = false;', 'qt'),
    ('batch ignores failed result', window, 'if (!this->fetch_ruleset_cache(url)) {\n                  cacheUpdated = false;\n                }', 'this->fetch_ruleset_cache(url);', 'qt'),
    ('batch short circuits after failure', window, 'if (!this->fetch_ruleset_cache(url)) {\n                  cacheUpdated = false;\n                }', 'cacheUpdated = cacheUpdated && this->fetch_ruleset_cache(url);', 'qt'),
    ('empty map claims success', window, 'bool cacheUpdated = !ruleSetMap.isEmpty();', 'bool cacheUpdated = true;', 'qt'),
    ('server refresh reuses stale entry', server, 'internal.RefreshHttp(in.HTTPURL, in.UseDefaultOutbound)', 'internal.CacheHttpBool(in.HTTPURL, in.UseDefaultOutbound, nil)', 'go'),
    ('server ignores refresh flag', server, 'if in.GetRefresh() {', 'if false && in.GetRefresh() {', 'go'),
    ('server accepts contradictory clear refresh', server, 'if in.Clear && in.GetRefresh() {', 'if false && in.Clear && in.GetRefresh() {', 'go'),
    ('server ignores clear flag', server, 'if in.Clear {', 'if false && in.Clear {', 'go'),
    ('failed refresh returns stale path', utils, 'log.Printf("Error while refreshing %s: %v", url, err)\n\t\treturn ""', 'log.Printf("Error while refreshing %s: %v", url, err)\n\t\treturn path', 'go'),
]
for name, path, old, new, kind in cases:
    with tempfile.TemporaryDirectory(prefix='nekobox-refresh-mutation-') as directory:
        root = Path(directory)
        for file in paths:
            target=root/file; target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text((ROOT/file).read_text())
        source=(root/path).read_text()
        if source.count(old)!=1:
            raise SystemExit('Mutation source anchor changed: '+name)
        (root/path).write_text(source.replace(old,new))
        command=['python3',str(HERE / ('run_'+kind+'_test.py')),'--repo',str(root)]
        if kind=='go': command += ['--go',args.go,'--','-race']
        elif args.qt_root: command += ['--qt-root',str(args.qt_root)]
        result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
        if result.returncode==0 or 'FAIL' not in result.stdout or '[build failed]' in result.stdout or 'error:' in result.stdout:
            print(result.stdout)
            raise SystemExit('Mutation did not produce the expected assertion failure: '+name)
        print('REJECTED '+name,flush=True)
print(str(len(cases))+'/'+str(len(cases))+' mutations rejected')
