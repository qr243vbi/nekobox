#!/usr/bin/env python3
"""Execute the production cache refresh handler with fake HTTP and temporary files."""
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
names = ['SetRulesetCachedir', 'DownloadFile', 'copyAndCloseDownload', 'urlToPath', 'fileExists', 'CacheHttpBool', 'CacheHttp', 'ClearRulesets', 'RefreshHttp', 'cacheHttpBool']
functions = []
for name in names:
    if 'func ' + name + '(' in utils:
        functions.append(extract(utils, name))
    elif name not in ['copyAndCloseDownload', 'RefreshHttp', 'cacheHttpBool']:
        raise SystemExit('Missing production function: ' + name)
handler = extract(server, '(s *server) CacheHTTP')
print('SOURCE', args.ref or 'working tree', hashlib.sha256((''.join(functions) + handler).encode()).hexdigest(), flush=True)
with tempfile.TemporaryDirectory(prefix='nekobox-refresh-command-') as directory:
    root = Path(directory)
    (root / 'internal').mkdir()
    (root / 'gen').mkdir()
    (root / 'go.mod').write_text('module nekobox_core\n\ngo 1.20\n')
    (root / 'internal/cache.go').write_text('''package internal
import ("fmt"; "io"; "log"; "net/http"; "net/url"; "os"; "path/filepath"; "strings")
var ruleset_cachedir string
var BoxInstance *struct{}
func BoxCreateHttpClient(_ *struct{}) *http.Client { panic("proxy used by isolated test") }
''' + '\n'.join(functions))
    (root / 'gen/types.go').write_text('''package gen
 type CacheURLRequest struct { HTTPURL string; Clear, Filepath, UseDefaultOutbound bool; Refresh *bool }
func (p *CacheURLRequest) GetRefresh() bool { return p.Refresh != nil && *p.Refresh }
 type CacheURLResult_ struct { FilePath string; Exists bool }
''')
    (root / 'server.go').write_text('package main\nimport ("context"; "nekobox_core/gen"; "nekobox_core/internal"' + ('; "errors"' if 'errors.' in handler else '') + ')\ntype server struct {}\n' + handler)
    (root / 'server_test.go').write_text((HERE / 'server_test.go.in').read_text())
    (root / 'legacy_test.go').write_text((HERE / 'legacy_handler.go.in').read_text())
    env = dict(os.environ, GOTOOLCHAIN='local', GOPROXY='off', GOSUMDB='off', GOWORK='off', GOCACHE=os.environ.get('GOCACHE', str(root / 'go-build')))
    result = subprocess.run([args.go, 'test', *args.go_args], cwd=root, env=env, timeout=180)
    raise SystemExit(result.returncode)
