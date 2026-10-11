#!/usr/bin/env python3
"""Execute the production cache command handler/clear function in isolation."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=ROOT)
parser.add_argument('--ref')
parser.add_argument('--go', default='go')
parser.add_argument('go_args', nargs='*')
args = parser.parse_args()

def read(path):
    if args.ref:
        return subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True)
    return (args.repo / path).read_text()

def extract(text, name):
    matches = list(re.finditer(r'^func ' + re.escape(name) + r'\(.*?(?=^func |\Z)', text, re.M | re.S))
    if len(matches) != 1:
        raise SystemExit('Expected exactly one production function: ' + name)
    return matches[0].group(0)

utils = read('core/server/internal/utils.go')
server = read('core/server/server.go')
clear = extract(utils, 'ClearRulesets')
handler = extract(server, '(s *server) CacheHTTP')
print('SOURCE', args.ref or 'working tree', hashlib.sha256((clear + handler).encode()).hexdigest(), flush=True)
with tempfile.TemporaryDirectory(prefix='nekobox-clear-command-') as directory:
    root = Path(directory)
    (root / 'internal').mkdir()
    (root / 'gen').mkdir()
    (root / 'go.mod').write_text('module nekobox_core\n\ngo 1.20\n')
    imports = ['os', 'path/filepath'] + [name for name in ['errors', 'fmt'] if name + '.' in clear]
    (root / 'internal/clear.go').write_text('package internal\nimport (' + ''.join('\n"' + name + '"' for name in imports) + '\n)\n' + '''
var ruleset_cachedir string
func SetRulesetCachedir(path string) { ruleset_cachedir = path }
var Calls int
func RefreshHttp(string, bool) string { panic("unexpected per-URL refresh in clear-only fixture") }
func CacheHttpBool(url string, proxy bool, exists *bool) string {
    Calls++
    if url == "" { *exists = false; return "" }
    *exists = true
    return "synthetic-cache-path"
}
''' + clear)
    (root / 'gen/types.go').write_text('''package gen
 type CacheURLRequest struct { HTTPURL string; Clear, Filepath, UseDefaultOutbound bool; Refresh *bool }
func (p *CacheURLRequest) GetRefresh() bool { return p.Refresh != nil && *p.Refresh }
 type CacheURLResult_ struct { FilePath string; Exists bool }
''')
    (root / 'server.go').write_text('package main\nimport ("context"; "nekobox_core/gen"; "nekobox_core/internal"' + ('; "errors"' if 'errors.' in handler else '') + ')\ntype server struct {}\n' + handler)
    (root / 'server_test.go').write_text((HERE / 'server_test.go.in').read_text())
    env = dict(os.environ, GOTOOLCHAIN='local', GOPROXY='off', GOSUMDB='off', GOWORK='off', GOCACHE=os.environ.get('GOCACHE', str(root / 'go-build')))
    result = subprocess.run([args.go, 'test', *args.go_args], cwd=root, env=env, timeout=180)
    raise SystemExit(result.returncode)
