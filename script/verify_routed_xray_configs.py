"""CI-only parse checks of synthetic configs; never start a proxy or TUN."""
from pathlib import Path
import argparse
import subprocess
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--xray', type=Path)
args = parser.parse_args()
if args.xray:
    executable = args.xray.resolve()
else:
    archive = next(Path('deployment').glob('*windows64.zip'))
    executable = Path('build/config-check-xray.exe').resolve()
    with zipfile.ZipFile(archive) as package:
        executable.write_bytes(package.read('nekobox/xray.exe'))
configs = sorted(Path('build/synthetic-xray-configs').glob('fixture-routed-*.json'))
if len(configs) != 4:
    raise SystemExit(f'Expected four routed Xray fixtures, found {len(configs)}')
for config in configs:
    subprocess.run([str(executable), 'run', '-test', '-format', 'json', '-c', str(config.resolve())],
                   check=True, timeout=30)
print(f'Xray parse-only validation: {len(configs)} routed configs passed; no proxy started')
