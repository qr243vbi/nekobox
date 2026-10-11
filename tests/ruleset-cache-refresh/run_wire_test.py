#!/usr/bin/env python3
"""Exercise current and pre-refresh generated Thrift codecs in memory."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo',type=Path,default=ROOT)
parser.add_argument('--ref')
parser.add_argument('--legacy-ref',default='a536846c9c74dd8fb104dce3319cbd9672920bb3')
parser.add_argument('--go',default='go')
args=parser.parse_args()
path='core/server/gen/libcore.go'
legacy=subprocess.check_output(['git','-C',str(args.repo),'show',args.legacy_ref+':'+path],text=True)
current=subprocess.check_output(['git','-C',str(args.repo),'show',args.ref+':'+path],text=True) if args.ref else (args.repo/path).read_text()
print('CURRENT',hashlib.sha256(current.encode()).hexdigest(),'LEGACY',hashlib.sha256(legacy.encode()).hexdigest(),flush=True)
with tempfile.TemporaryDirectory(prefix='nekobox-refresh-wire-') as directory:
    root=Path(directory)
    (root/'legacy').mkdir()
    (root/'legacy/libcore.go').write_text(legacy.replace('package gen','package legacy',1))
    (root/'libcore.go').write_text(current)
    (root/'wire_test.go').write_text((HERE/'wire_test.go.in').read_text())
    (root/'request_test.go').write_text((ROOT/'core/server/gen/cache_refresh_test.go').read_text())
    (root/'go.mod').write_text('module cache_refresh_wire\n\ngo 1.27.1\n\nrequire github.com/apache/thrift v0.24.0\n')
    (root/'go.sum').write_text((args.repo/'core/server/go.sum').read_text())
    env=dict(os.environ,GOTOOLCHAIN='local',GOPROXY='off',GOSUMDB='off',GOWORK='off',GOCACHE=os.environ.get('GOCACHE',str(root/'go-build')))
    raise SystemExit(subprocess.run([args.go,'test','-race','-v','./...'],cwd=root,env=env,timeout=180).returncode)
