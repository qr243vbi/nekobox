# Remember setting save-target regression

Related to [issue #141](https://github.com/qr243vbi/nekobox/issues/141).
Baseline: `a536846c9c74dd8fb104dce3319cbd9672920bb3`.

The Remember action changes `WindowSettings::remember_last_profile`, but its
handler previously called `DataStore::Save()`. That serializes a different store
and does not immediately persist the changed preference in `window.ini`.
The one-line patch saves the setting's actual owner, `WindowSettings`.

## Run

Requires Python 3, a C++20 compiler, pkg-config, and Qt6Core development files.

```sh
python3 tests/remember-setting/test_remember.py
python3 tests/remember-setting/test_remember.py --ref a536846c9c74dd8fb104dce3319cbd9672920bb3
python3 tests/remember-setting/mutation_checks.py
```

The regression compiles the exact production lambda, WindowSettings declarations,
field map, and SettingsStore serialization methods. QSettings writes and reads
only synthetic `window.ini` files in a new temporary directory. UI checkmark,
access guard, Shortcuts, and unrelated DataStore dependencies are fixture doubles.
It checks both initial/requested values, allowed/denied access, the resulting
checkmark, round-trip value, unrelated window settings, and absence of unrelated
DataStore writes. Each of three mutations must fail by runtime assertions.

Evidence: candidate 48/48 checks; baseline 42/48 checks (both value-changing
round trips fail, plus four unrelated-save checks). Three mutations rejected.

This does not simulate application startup/shutdown, a persistence race, real
profiles, registry, OS settings, or core execution. The full application's normal
commit-data handler already saves WindowSettings, so this patch alone does not
prove that issue #141's original startup symptom is resolved. The change simply
makes the action immediately save its own preference. Full application build was
unavailable because CMake is absent. A full suite was not run because lifecycle
and persistence-race tests are outside this task's permitted scope.
