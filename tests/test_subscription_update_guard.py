#!/usr/bin/env python3
"""Focused source-extracted regression for NekoBox issue #277.

Usage: python3 test_subscription_update_guard.py --repo /path/to/nekobox [--ref HEAD]
Requires Python 3 and a C++20 compiler (CXX or g++). Uses no network or VPN.

Compiles the repository's verbatim menu handler, Settings handler and AsyncUpdate
method. Qt types, GUI dispatch, group preparation and Update itself are isolated
stubs. The updater worker executes on a real std::thread; GUI dispatch is a
controlled queue, NOT a real Qt event loop. The early-return test represents an
Update error return; it does not make an HTTP request or run the real importer.
This is a companion diagnostic, not a full application/integration test.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=Path.cwd())
parser.add_argument('--ref', help='Read this git ref instead of working files')
args = parser.parse_args()
repo = args.repo.resolve()
def read(path):
    if args.ref:
        return subprocess.check_output(['git', '-C', str(repo), 'show', f'{args.ref}:{path}'], text=True)
    return (repo / path).read_text()
def section(text, start, end):
    assert text.count(start) == 1, start
    return start + text.split(start, 1)[1].split(end, 1)[0]
main = section(read('src/gharqad/ui/mainwindow.cpp'), 'bool mw_sub_updating = false;',
               'void MainWindow::on_menu_remove_unavailable_triggered()')
settings = section(read('src/gharqad/ui/group/GroupItem.cpp'),
                   'void GroupItem::on_update_sub_clicked()', 'void GroupItem::on_edit_clicked()')
async_update = section(read('src/gharqad/configs/sub/GroupUpdater.cpp'),
                       'void GroupUpdater::AsyncUpdate(', 'void GroupUpdater::Update(')
preamble = r'''
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#define CHECK_SETTINGS_ACCESS_W
#define SKIP_JS_UPDATER
#define emit
struct QString : std::string {
  using std::string::string;
  QString(const std::string& value) : std::string(value) {}
  QString trimmed() const { return *this; } // Fixtures contain no whitespace.
  bool startsWith(const char* prefix) const { return starts_with(prefix); }
  bool isEmpty() const { return empty(); }
};
using QByteArray = std::string;
template<class K, class V> using QMap = std::map<K, V>;
using Info = std::function<QString(bool*, bool*, const QString&)>;
namespace Configs {
struct GroupExtra { QString url = "https://example.invalid/sub"; int id = 1; };
struct Group {
  bool is_subscription = true;
  int id = 1;
  QString name;
  std::shared_ptr<GroupExtra> extra = std::make_shared<GroupExtra>();
  auto getExtra() { return extra; }
  auto getExtraUnlocked() { return extra; }
};
struct ProfileManager {
  std::shared_ptr<Group> current;
  auto CurrentGroup() { return current; }
  static auto NewGroup() { return std::make_shared<Group>(); }
  void AddGroup(std::shared_ptr<Group>) {}
};
ProfileManager manager;
ProfileManager* profileManager = &manager;
}
int chooserCalls = 0;
QString chooseUpdateGroup(bool* ok, bool*, const QString&) {
  ++chooserCalls; *ok = false; return "";
}
void MW_dialog_message(const char*, const char*) {}
std::deque<std::function<void()>> workerQueue;
std::deque<std::function<void()>> guiQueue;
std::mutex guiMutex;
const auto guiThread = std::this_thread::get_id();
int workerPosts = 0;
bool appAlive = true;
void runOnNewThread(const std::function<void()>& callback) { workerQueue.push_back(callback); }
void runOnUiThread(const std::function<void()>& callback) {
  if (!appAlive) return; // Mirrors helper's qApp-null no-op.
  std::lock_guard lock(guiMutex);
  if (std::this_thread::get_id() != guiThread) ++workerPosts;
  guiQueue.push_back(callback);
}
void finishWorker() {
  if (workerQueue.empty()) return;
  auto callback = std::move(workerQueue.front()); workerQueue.pop_front();
  std::thread worker(std::move(callback)); worker.join();
}
void processGuiQueue() {
  while (!guiQueue.empty()) {
    auto callback = std::move(guiQueue.front()); guiQueue.pop_front(); callback();
  }
}
namespace Subscription {
struct GroupUpdater {
  int menuRequests = 0;
  int updateCalls = 0;
  int earlyReturns = 0;
  int completionSignals = 0;
  bool returnEarly = false;
  using PreFinish = std::function<void(std::shared_ptr<Configs::Group>)>;
  void AsyncUpdateGroup(std::shared_ptr<Configs::Group> group, PreFinish preFinish,
      const Info& info, std::function<void()> finish,
      std::function<std::shared_ptr<const Configs::GroupExtra>(std::shared_ptr<const Configs::GroupExtra>)>) {
    ++menuRequests;
    // Stub only group/header/payload preparation. Execute actual AsyncUpdate below.
    AsyncUpdate(preFinish, group->extra->url, info, group->id, finish);
  }
  void AsyncUpdate(const PreFinish, const QString&, const Info&, int = -1,
      const std::function<void()>& = nullptr, const QMap<QString, QString>& = {},
      const QByteArray& = {}, bool = false);
  void Update(PreFinish, const QString&, int, bool, bool,
      const QMap<QString, QString>&, const QByteArray&, bool) {
    ++updateCalls;
    if (returnEarly) { ++earlyReturns; return; }
  }
  void asyncUpdateCallback(int) { ++completionSignals; }
};
GroupUpdater updater;
GroupUpdater* groupUpdater = &updater;
}
struct MainWindow {
  Subscription::GroupUpdater::PreFinish post_update_job;
  void on_menu_update_subscription_triggered();
};
MainWindow mainWindow;
MainWindow* GetMainWindow() { return &mainWindow; }
struct GroupItem {
  std::shared_ptr<Configs::Group> ent;
  void on_update_sub_clicked();
};
'''
checks = r'''
int failures = 0, assertions = 0;
void check(const std::string& label, bool condition) {
  ++assertions;
  std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
  if (!condition) ++failures;
}
void reset() {
  mw_sub_updating = false; Subscription::updater = {};
  workerQueue.clear(); guiQueue.clear(); chooserCalls = workerPosts = 0; appAlive = true;
}
int main() {
  auto a = std::make_shared<Configs::Group>();
  auto b = std::make_shared<Configs::Group>(); b->id = b->extra->id = 2;
  auto& u = Subscription::updater;
  Configs::manager.current = a;
  reset(); a->is_subscription = false;
  mainWindow.on_menu_update_subscription_triggered();
  check("ordinary group does not enqueue an update", workerQueue.empty() && !mw_sub_updating);
  a->is_subscription = true; mw_sub_updating = true;
  mainWindow.on_menu_update_subscription_triggered();
  check("pre-existing busy guard blocks request", workerQueue.empty());
  for (bool earlyReturn : {false, true}) {
    const std::string label = earlyReturn ? "early-return completion: " : "normal completion: ";
    reset(); Configs::manager.current = a; u.returnEarly = earlyReturn;
    mainWindow.on_menu_update_subscription_triggered();
    check(label + "first request starts", u.menuRequests == 1 && workerQueue.size() == 1 && mw_sub_updating);
    mainWindow.on_menu_update_subscription_triggered();
    check(label + "pending repeat is blocked", u.menuRequests == 1);
    finishWorker();
    check(label + "actual AsyncUpdate emits completion", u.updateCalls == 1 && u.completionSignals == 1);
    check(label + "selected Update return branch ran", u.earlyReturns == (earlyReturn ? 1 : 0));
    check(label + "existing group skips import chooser", chooserCalls == 0);
    check(label + "worker posts exactly one GUI callback", guiQueue.size() == 1 && workerPosts == 1);
    check(label + "guard remains set until GUI delivery", mw_sub_updating);
    mainWindow.on_menu_update_subscription_triggered();
    check(label + "request before GUI delivery is blocked", u.menuRequests == 1);
    processGuiQueue();
    check(label + "GUI completion releases guard", !mw_sub_updating);
    mainWindow.on_menu_update_subscription_triggered();
    check(label + "same group can update again", u.menuRequests == 2);
    finishWorker(); processGuiQueue();
    Configs::manager.current = b;
    mainWindow.on_menu_update_subscription_triggered();
    check(label + "another group can update next", u.menuRequests == 3);
  }
  reset(); Configs::manager.current = a;
  mainWindow.on_menu_update_subscription_triggered();
  GroupItem item{b}; item.on_update_sub_clicked(); item.on_update_sub_clicked();
  check("Settings handler bypasses menu guard twice", workerQueue.size() == 3 && u.menuRequests == 1);
  reset();
  { MainWindow temporaryWindow; temporaryWindow.on_menu_update_subscription_triggered(); }
  finishWorker(); processGuiQueue();
  check("guard-release callback does not need the originating window", !mw_sub_updating);
  reset(); mainWindow.on_menu_update_subscription_triggered(); appAlive = false;
  finishWorker();
  check("absent app drops delivery without dereferencing a window", guiQueue.empty());
  std::cout << "SUMMARY assertions=" << assertions << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
'''
code = preamble + '\nnamespace Subscription {\n' + async_update + '\n}\n' + main + settings + checks
with tempfile.TemporaryDirectory(prefix='nekobox-277-') as tmp:
    cpp = Path(tmp) / 'regression.cpp'
    exe = Path(tmp) / 'regression'
    cpp.write_text(code)
    cmd = shlex.split(os.environ.get('CXX', 'g++')) + ['-std=c++20', '-pthread', '-Wall', '-Wextra', '-Werror', '-O0', str(cpp), '-o', str(exe)]
    print('SOURCE', args.ref or 'working tree', 'in', repo, flush=True)
    print('COMPILE', shlex.join(cmd), flush=True)
    subprocess.run(cmd, check=True)
    result = subprocess.run([str(exe)])
    raise SystemExit(result.returncode)
