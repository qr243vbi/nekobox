#!/usr/bin/env python3
"""Reject isolated command-serialization mutations using the native regression."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
spec = importlib.util.spec_from_file_location('arguments_regression', HERE / 'test_arguments.py')
regression = importlib.util.module_from_spec(spec)
spec.loader.exec_module(regression)
paths = (regression.SOURCE, regression.MAIN)
sources = {path: (ROOT / path).read_text() for path in paths}
windows_quote = regression.function(sources[regression.SOURCE], 'static QString Windows_QuoteArgument(')
mutations = [
    ('omit app-data flag', regression.SOURCE, 'arguments << "-appdata";', '(void)arguments;'),
    ('omit custom directory', regression.SOURCE, 'arguments << Configs::dataStore->appdataDir;', '(void)arguments;'),
    ('naive Windows quotes', regression.SOURCE, windows_quote, 'static QString Windows_QuoteArgument(const QString &argument) { return "\\\"" + argument + "\\\""; }'),
    ('omit desktop quoting', regression.SOURCE, 'argument = DesktopEntry_QuoteArgument(argument);', 'if (false) argument = DesktopEntry_QuoteArgument(argument);'),
    ('omit desktop percent escaping', regression.SOURCE, 'argument.replace("%", "%%");', '(void)argument;'),
    ('retain relative directory', regression.MAIN, 'QDir(appdataDir).absolutePath()', 'appdataDir'),
]
for name, path, old, new in mutations:
    if sources[path].count(old) != 1:
        raise SystemExit('Mutation anchor changed: ' + name)
    with tempfile.TemporaryDirectory(prefix='nekobox-autorun-mutation-') as temp:
        for file, source in sources.items():
            target = Path(temp) / file
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(source.replace(old, new) if file == path else source)
        result = subprocess.run([sys.executable, str(HERE / 'test_arguments.py'), '--repo', temp], text=True, capture_output=True, timeout=90)
    if result.returncode != 1 or 'FAIL ' not in result.stdout or 'SUMMARY ' not in result.stdout or result.stderr:
        print(result.stdout, result.stderr)
        raise SystemExit('Mutation not rejected by runtime assertions: ' + name)
    print('PASS rejected:', name, '|', result.stdout.splitlines()[-1])
print(f'SUMMARY {len(mutations)}/{len(mutations)} mutations rejected')
