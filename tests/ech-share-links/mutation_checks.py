#!/usr/bin/env python3
"""Verify that each independent ECH share-link regression is detected."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
LINK = 'src/gharqad/configs/proxy/Link2Bean.cpp'
TROJAN = 'src/gharqad/configs/proxy/TrojanVLESSBean.cpp'
IMPORT = '''        auto ech_config = GetQueryValue(query, "ech_config");
        if ((stream->enable_ech = !ech_config.isEmpty())) {
            stream->ech_config = ech_config;
            stream->query_server_name = GetQueryValue(query, "query_server_name");
        }
'''
mutations = [
    ('omit manual ECH import', TROJAN, IMPORT, ''),
    ('omit manual ECH config', TROJAN, '            stream->ech_config = ech_config;\n', ''),
    ('enable ECH unconditionally', TROJAN, 'stream->enable_ech = !ech_config.isEmpty()', 'stream->enable_ech = true'),
    ('omit manual query-server-name', TROJAN, '            stream->query_server_name = GetQueryValue(query, "query_server_name");\n', ''),
]
for name, file, old, new in mutations:
    with tempfile.TemporaryDirectory(prefix='nekobox-ech-mutation-') as temp:
        repo = Path(temp)
        for source in (REPO / 'src/gharqad/configs/proxy').glob('*.cpp'):
            target = repo / source.relative_to(REPO)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        for rel in ('src/gharqad/dataStore/Utils.cpp', 'src/nekobox/dataStore/Utils.hpp',
                    'src/nekobox/configs/proxy/V2RayStreamSettings.hpp', 'src/nekobox/configs/proxy/AbstractBeanExtra.hpp'):
            target = repo / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / rel, target)
        target = repo / file
        source = target.read_text()
        assert source.count(old) == 1, name
        target.write_text(source.replace(old, new))
        result = subprocess.run([sys.executable, str(HERE / 'test_ech_share_links.py'), '--repo', str(repo)], capture_output=True, text=True)
        summaries = [line for line in result.stdout.splitlines() if line.startswith('SUMMARY')]
        if result.returncode != 1 or not summaries or 'failures=0' in summaries[-1]:
            print(result.stdout, result.stderr)
            raise SystemExit(f'Mutation not detected correctly: {name}')
        print(f'CAUGHT {name}: {summaries[-1]}', flush=True)
print('PASS: four independent mutations detected')
