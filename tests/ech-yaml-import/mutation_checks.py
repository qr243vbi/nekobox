#!/usr/bin/env python3
"""Detect independent production regressions with the real YAML/Qt test."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
UPDATER = 'src/gharqad/configs/sub/GroupUpdater.cpp'
mutations = [
    ('ignore all ECH', '  if (!ech.IsDefined()) return true;', '  return true;'),
    ('derive enable from config', 'stream->enable_ech = enable_ech;', 'stream->enable_ech = !ech_config.isEmpty();'),
    ('lose query name', 'stream->query_server_name = query_server_name;', 'stream->query_server_name = QString();'),
    ('lose PEM wrapper', 'stream->ech_config = ech_config;', 'stream->ech_config = encoded;'),
    ('accept unpadded Base64', 'normalized.size() % 4 == 0 && !decoded.isEmpty()', '!decoded.isEmpty()'),
    ('reject disabled malformed Base64', 'if (enable_ech && !valid_config) return false;', 'if (!valid_config) return false;'),
    ('lose CRLF support', "normalized.remove('\\r').remove('\\n');", '/* missing CRLF normalization */'),
    ('downgrade malformed profile', '          continue;\n        }\n      }\n\n      if (needFix)', '          /* missing rejection */\n        }\n      }\n\n      if (needFix)'),
]
for name, old, new in mutations:
    with tempfile.TemporaryDirectory(prefix='nekobox-ech-yaml-mutation-') as temp:
        repo = Path(temp)
        for rel in (UPDATER, 'src/gharqad/dataStore/Utils.cpp', 'src/gharqad/dataStore/base64.cpp',
                    'src/nekobox/dataStore/Utils.hpp', 'src/nekobox/configs/proxy/V2RayStreamSettings.hpp'):
            target = repo / rel; target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / rel, target)
        target = repo / UPDATER; source = target.read_text()
        assert source.count(old) == 1, name
        target.write_text(source.replace(old, new))
        result = subprocess.run([sys.executable, str(HERE / 'test_ech_yaml_import.py'), '--repo', str(repo)], capture_output=True, text=True)
        summaries = [line for line in result.stdout.splitlines() if line.startswith('SUMMARY')]
        if result.returncode != 1 or not summaries or 'failures=0' in summaries[-1]:
            print(result.stdout, result.stderr)
            raise SystemExit('Mutation not detected correctly: ' + name)
        print('CAUGHT', name, summaries[-1], flush=True)
print('PASS: all eight production mutations detected')
