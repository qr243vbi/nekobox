#!/usr/bin/env python3
"""Verify targeted regressions are caught without changing the candidate source."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
source = (ROOT / "src/gharqad/dataStore/ProfileFilter.cpp").read_text()
bean = 'key->bean()->compare(other.key->bean().get(), {"c_cfg", "c_out"})'
entity = 'key->compare(other.key.get(), {"c_cfg", "c_out"})'


def replace_once(old, new):
    if source.count(old) != 1:
        raise AssertionError(f"Mutation anchor changed: {old!r}")
    return source.replace(old, new, 1)

ordering_start = source.index('bool ProfileFilterKey::operator<(')
ordering_end = source.index('bool ProfileFilterKey::operator>(', ordering_start)
old_ordering = '''bool ProfileFilterKey::operator<(const ProfileFilterKey &other) const noexcept
{
  if (other.key == nullptr) return false;
  return key == nullptr
      || key->type < other.key->type
      || key->serverAddress < other.key->serverAddress
      || key->serverPort < other.key->serverPort
      || (skip_compare_beans && !other.skip_compare_beans)
      || (!skip_compare_beans && !other.skip_compare_beans
          && key->bean()->compare(other.key->bean().get(), {"c_cfg", "c_out"}) < 0);
}

'''
mutations = {
    "restore_disjunctive_ordering": source[:ordering_start] + old_ordering + source[ordering_end:],
    "reverse_address_flag": replace_once('ProfileFilterKey(ent, useAddressOnly)',
                                          'ProfileFilterKey(ent, !useAddressOnly)'),
    "compare_entities_both_operators": source.replace(bean, entity),
    "compare_entity_equality_only": replace_once(bean + ' == 0', entity + ' == 0'),
    "compare_entity_ordering_only": replace_once(bean + ' < 0', entity + ' < 0'),
    "remove_custom_exception": replace_once('by_address && ent->type != "custom"', 'by_address'),
    "ignore_type": replace_once('''  if (key->type != other.key->type) {
    return key->type < other.key->type;
  }
''', ''),
    "ignore_address": replace_once('''  if (key->serverAddress != other.key->serverAddress) {
    return key->serverAddress < other.key->serverAddress;
  }
''', ''),
    "ignore_port": replace_once('''  if (key->serverPort != other.key->serverPort) {
    return key->serverPort < other.key->serverPort;
  }
''', ''),
    "ignore_mode": replace_once('''  if (skip_compare_beans != other.skip_compare_beans) {
    return skip_compare_beans;
  }
''', ''),
    "remove_c_cfg_exclusion": source.replace('{"c_cfg", "c_out"}', '{"c_out"}'),
    "remove_c_out_exclusion": source.replace('{"c_cfg", "c_out"}', '{"c_cfg"}'),
}

with tempfile.TemporaryDirectory(prefix="profilefilter-mutations-") as temp:
    for name, mutant in mutations.items():
        path = Path(temp) / (name + '.cpp')
        path.write_text(mutant)
        environment = dict(os.environ, PROFILE_FILTER_SOURCE=str(path))
        result = subprocess.run([sys.executable, str(HERE / 'test_profilefilter.py')],
                                env=environment, text=True, capture_output=True)
        output = result.stdout + result.stderr
        if result.returncode == 0 or 'FAILED (failures=' not in output or 'errors=' in output:
            print(output)
            raise SystemExit(f"Mutant did not compile and fail tests as expected: {name}")
        failed_cases = [line.split(' ... ')[0] for line in output.splitlines()
                        if line.startswith('test_') and line.endswith(' ... FAIL')]
        print(f"KILLED {name}: {len(failed_cases)} failing case(s)")
        for case in failed_cases:
            print('  ' + case)
print(f"All {len(mutations)} targeted mutants were caught.")
