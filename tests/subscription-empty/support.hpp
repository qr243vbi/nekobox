#pragma once
// Synthetic boundaries only. Production control flow is in production.inc.
#include <algorithm>
#include <atomic>
#include <deque>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define CHECK_SETTINGS_ACCESS_W
#define SKIP_JS_UPDATER
#define emit

struct QString : std::string {
  using std::string::string;
  QString(const std::string &value) : std::string(value) {}
  bool isEmpty() const { return empty(); }
  bool startsWith(const char *prefix) const { return starts_with(prefix); }
  QString trimmed() const { return *this; } // Trimmed ASCII-only inputs.
  QString arg(int value) const {
    auto result = *this;
    auto offset = result.find("%1");
    if (offset != npos) result.replace(offset, 2, std::to_string(value));
    return result;
  }
};
using QByteArray = std::string;
template <typename T> struct QList : std::vector<T> {
  using std::vector<T>::vector;
  int size() const { return static_cast<int>(std::vector<T>::size()); }
  int count() const { return size(); }
  int length() const { return size(); }
  bool isEmpty() const { return this->empty(); }
  QList &operator+=(const T &value) { this->push_back(value); return *this; }
  QList &operator<<(const T &value) { return *this += value; }
};
template <typename K, typename V> using QMap = std::map<K, V>;
struct QObject { static QString tr(const char *value) { return value; } };

namespace Fixture {
inline std::string schedule;
inline std::deque<std::function<void()>> validationQueue, updateQueue, guiQueue;
inline QList<int> validated, deleted;
inline std::vector<std::string> events;
inline std::vector<QString> logs;
inline int scheduled = 0, started = 0, returned = 0, signalCount = 0, urlCount = 0;
inline const auto guiThread = std::this_thread::get_id();
inline bool sawBackgroundCallback = false;
inline int throwAfterEnqueues = -1;
struct SchedulingFailure : std::runtime_error {
  SchedulingFailure() : std::runtime_error("synthetic scheduling failure") {}
};

struct BlockedWait : std::runtime_error {
  BlockedWait() : std::runtime_error(
      "completion latch remains locked with no scheduled validation worker left") {}
};

inline void runValidation() {
  const bool reverse = schedule == "deferred_reverse";
  auto job = std::move(reverse ? validationQueue.back() : validationQueue.front());
  if (reverse) validationQueue.pop_back(); else validationQueue.pop_front();
  ++started;
  job();
  ++returned;
}
}

// A cooperative scheduler deliberately models only latch availability. A second
// lock drains the finite queued validation jobs; if none can unlock it, it throws
// instead of hanging. This DOES NOT certify QMutex ownership/thread correctness.
// Existing production cross-thread unlock behavior requires separate Qt review.
class QMutex {
  bool locked = false;
  inline static std::set<const QMutex *> live;
public:
  QMutex() { live.insert(this); }
  ~QMutex() { live.erase(this); }
  void lock() {
    if (!live.contains(this)) throw std::runtime_error("worker accessed a destroyed mutex");
    while (locked && !Fixture::validationQueue.empty()) Fixture::runValidation();
    if (locked) throw Fixture::BlockedWait();
    locked = true;
  }
  void unlock() {
    if (!locked) throw std::runtime_error("unlock of unlocked latch");
    locked = false;
  }
};

class QThreadPool {
public:
  inline static std::vector<QThreadPool *> allocations;
  explicit QThreadPool(void * = nullptr) { allocations.push_back(this); }
  ~QThreadPool() {
    waitForDone();
    std::erase(allocations, this);
  }
  template <typename F> void start(F job) {
    if (Fixture::scheduled == Fixture::throwAfterEnqueues) throw Fixture::SchedulingFailure();
    ++Fixture::scheduled;
    Fixture::validationQueue.emplace_back(std::move(job));
    if (Fixture::schedule == "finish_before_wait") Fixture::runValidation();
  }
  bool waitForDone() {
    while (!Fixture::validationQueue.empty()) Fixture::runValidation();
    return true;
  }
  static void cleanup() {
    while (!allocations.empty()) delete allocations.back();
  }
};

inline void runOnNewThread(std::function<void()> job) {
  Fixture::updateQueue.push_back(std::move(job));
}
inline void runOnUiThread(std::function<void()> job) {
  Fixture::events.emplace_back("gui_post");
  Fixture::guiQueue.push_back(std::move(job));
}
inline void MW_show_log(const QString &text) { Fixture::logs.push_back(text); }
inline void MW_dialog_message(const char *, const char *) {}
inline QString chooseUpdateGroup(bool *, bool *, const QString &) {
  throw std::runtime_error("unexpected import chooser");
}

namespace Configs {
struct ProxyEntity { int id = 0, gid = 1, latencyInt = 10; bool valid = true; };
using Profile = std::shared_ptr<ProxyEntity>;
struct GroupExtra { QString url = "https://example.invalid/subscription"; };
struct Group {
  bool is_subscription = true;
  int id = 1;
  QString name;
  QList<int> profiles;
  std::shared_ptr<GroupExtra> extra = std::make_shared<GroupExtra>();
  auto getExtraUnlocked() { return extra; }
};
struct DataStore {
  bool sub_rm_duplicates = false, sub_rm_invalid = true;
  bool sub_url_test = false, sub_rm_unavailable = false;
};
inline DataStore store;
inline auto dataStore = &store;
struct ProfileManager {
  std::map<int, Profile> loaded;
  std::shared_ptr<Group> current = std::make_shared<Group>();
  Profile GetProfile(int id) {
    auto it = loaded.find(id);
    return it == loaded.end() ? nullptr : it->second;
  }
  void FillProfileEnts(QList<Profile> &, const QList<int> &);
  auto CurrentGroup() { return current; }
  static auto NewGroup() { return std::make_shared<Group>(); }
  void AddGroup(std::shared_ptr<Group> group) { current = group; }
  void BatchDeleteProfiles(const QList<int> &ids) {
    for (int id : ids) {
      Fixture::deleted += id;
      loaded.erase(id);
      std::erase(current->profiles, id);
    }
  }
};
inline ProfileManager manager;
inline auto profileManager = &manager;
inline bool IsValid(Profile profile) {
  if (!profile) throw std::runtime_error("null loaded profile passed to validator");
  Fixture::validated += profile->id;
  return profile->valid; // No core, RPC, or network is opened.
}
struct ProfileFilter {
  static void Uniq(const QList<Profile> &input, QList<Profile> &output, bool, bool) {
    output = input; // No duplicates in the duplicate-stage fixture.
  }
  static void OnlyInSrc_ByPointer(const QList<Profile> &, const QList<Profile> &,
                                  QList<Profile> &) {}
};
}

namespace Subscription {
using PreFinish = std::function<void(std::shared_ptr<Configs::Group>)>;
using Info = std::function<QString(bool *, bool *, const QString &)>;
struct GroupUpdater {
  void AsyncUpdate(const PreFinish, const QString &, const Info &, int = -1,
                   const std::function<void()> & = nullptr,
                   const QMap<QString, QString> & = {}, const QByteArray & = {}, bool = false);
  void AsyncUpdateGroup(std::shared_ptr<Configs::Group> group, PreFinish callback,
                        const Info &info, std::function<void()> finish,
                        std::function<std::shared_ptr<const Configs::GroupExtra>(
                            std::shared_ptr<const Configs::GroupExtra>)>) {
    AsyncUpdate(callback, group->extra->url, info, group->id, finish);
  }
  void Update(PreFinish callback, const QString &, int, bool, bool,
              const QMap<QString, QString> &, const QByteArray &, bool) {
    // Import/network are outside this fixture. Enter the actual post-update job
    // where Update calls PreFinishJob(group), then return through real AsyncUpdate.
    Fixture::sawBackgroundCallback = std::this_thread::get_id() != Fixture::guiThread;
    Fixture::events.emplace_back("callback_enter");
    callback(Configs::manager.current);
    Fixture::events.emplace_back("callback_return");
    if (!Fixture::validationQueue.empty())
      throw std::runtime_error("callback returned before all validation workers ran");
  }
  void asyncUpdateCallback(int) {
    ++Fixture::signalCount;
    Fixture::events.emplace_back("completion_signal");
  }
};
inline GroupUpdater updater;
inline auto groupUpdater = &updater;
}

struct MainWindow {
  Subscription::PreFinish post_update_job;
  void installPostUpdateJob();
  void on_menu_update_subscription_triggered();
  void urltest_current_group(const QList<int> &, bool, std::function<void(const QList<int> &)>) {
    ++Fixture::urlCount;
    Fixture::events.emplace_back("url_test");
    // URL test initiation is observed only. It never makes a request.
  }
};
