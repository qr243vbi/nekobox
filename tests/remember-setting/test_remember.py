#!/usr/bin/env python3
"""Execute the verbatim Remember slot and real WindowSettings INI serialization.

Only Qt6Core and synthetic, temporary window.ini files are used. The unrelated
DataStore/UI/access-check dependencies are fixture doubles. No app/core, real
settings, OS startup integration, shutdown, or persistence-race code runs.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def source_code(read):
    header = read('src/nekobox/sys/Settings.h')
    header = header[header.index('#define CONFIG_INI_PATH'):header.index('// void updateEmojiFont();')]
    implementation = read('src/gharqad/sys/Settings.cpp')
    implementation = implementation[implementation.index('QSettings Configs::WindowSettings::settings()'):implementation.index('QSettings getGlobal()')]
    main = read('src/gharqad/ui/mainwindow.cpp')
    slot = main.split('connect(ui->actionRemember_last_proxy, &QAction::triggered, this,', 1)[1].split('connect(ui->actionStart_with_system,', 1)[0].strip()
    if not slot.endswith('});') or slot.count('[=, this](bool checked)') != 1:
        raise ValueError('Remember slot boundaries changed')
    slot = slot[:-2]  # Retain lambda ending brace, discard connect closing ).
    # Its production owner is WindowSettings; refuse silently moved mappings.
    configs = read('src/gharqad/dataStore/Configs.cpp')
    if 'ADD_LIST(remember_last_profile)' not in implementation or 'ADD_MAP("remember_last_profile"' in configs:
        raise ValueError('Remember store mapping changed')
    return r'''
#include <QDir>
#include <QSettings>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QVariant>
#include <memory>
#include <iostream>
namespace Configs { class Shortcuts {}; }
''' + header + implementation + r'''
namespace Configs {
std::shared_ptr<WindowSettings> windowSettings;
struct DataStore { int saves = 0; void Save() { ++saves; } } store;
DataStore *dataStore = &store;
}
struct Action { bool checked = false; void setChecked(bool value) { checked = value; } };
struct UI { Action action; Action *actionRemember_last_proxy = &action; };
struct Controller {
    UI holder;
    UI *ui = &holder;
    bool allowed = true;
    void trigger(bool checked) {
#define CHECK_SETTINGS_ACCESS_W if (!allowed) return;
        auto slot = ''' + slot + r''';
        slot(checked);
#undef CHECK_SETTINGS_ACCESS_W
    }
};
int main() {
    int total = 0, failures = 0;
    auto check = [&](bool ok, const char *label) { ++total; if (!ok) { ++failures; std::cout << "FAIL " << label << '\n'; } };
    for (bool saved : {false, true}) {
        for (bool requested : {false, true}) {
            for (bool allowed : {false, true}) {
                Configs::windowSettings = std::make_shared<Configs::WindowSettings>();
                auto &settings = *Configs::windowSettings;
                settings.remember_last_profile = saved;
                settings.theme = "synthetic sentinel";
                settings.Save();
                Configs::store.saves = 0;
                Controller controller;
                controller.allowed = allowed;
                // QAction::triggered carries the new checked value; denied
                // access restores its prior state (!requested).
                controller.trigger(requested);
                const bool expected = allowed ? requested : saved;
                Configs::WindowSettings reloaded;
                reloaded.Load();
                check(reloaded.remember_last_profile == expected, "INI round trip after Remember slot");
                check(settings.remember_last_profile == expected, "in-memory setting");
                check(controller.holder.action.checked == (allowed ? requested : !requested), "checkmark/denied-access state");
                check(reloaded.theme == "synthetic sentinel", "unrelated window setting preserved");
                check(Configs::store.saves == 0, "no unrelated DataStore save");
                check(reloaded.settings().status() == QSettings::NoError, "temporary INI status");
            }
        }
    }
    std::cout << "SUMMARY " << total << " checks; " << failures << " failures\n";
    return failures ? 1 : 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    args = parser.parse_args()
    def read(path):
        return subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
    code = source_code(read)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    with tempfile.TemporaryDirectory(prefix='nekobox-remember-setting-') as name:
        temp = Path(name)
        (temp / 'test.cpp').write_text(code)
        subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-fPIC', '-Wall', '-Wextra', '-Werror', str(temp / 'test.cpp'), '-o', str(temp / 'test'), *flags], check=True, timeout=60)
        return subprocess.run([str(temp / 'test')], cwd=temp, timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
