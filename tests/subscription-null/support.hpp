#pragma once
// Offline boundaries for source-extracted subscription update tests.
// No Qt event loop, filesystem database, network, or proxy core is used.
#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

struct QString : std::string {
  using std::string::string;
  QString(const std::string &text) : std::string(text) {}
  bool isEmpty() const { return empty(); }
  bool startsWith(const char *prefix) const { return starts_with(prefix); }
  QString trimmed() const { return *this; } // All fixture inputs are ASCII, trimmed.
  QString mid(std::size_t offset) const { return substr(offset); }
  QString toUtf8() const { return *this; }
  static QString fromUtf8(const QString &value) { return value; }
  QString arg(const QString &value) const {
    auto text = *this;
    if (auto position = text.find("%1"); position != npos) text.replace(position, 2, value);
    return text;
  }
  QString arg(int value) const { return arg(std::to_string(value)); }
};
struct QByteArray : QString {
  using QString::QString;
  enum { OmitTrailingEquals };
  static QString fromBase64(const QString &, int) {
    throw std::runtime_error("Base64 decoding is outside this offline fixture");
  }
};
template <class T> struct QList : std::vector<T> {
  using std::vector<T>::vector;
  QList &operator<<(const T &value) { this->push_back(value); return *this; }
  QList &operator+=(const T &value) { return *this << value; }
  void append(const T &value) { this->push_back(value); }
  int count() const { return static_cast<int>(this->size()); }
  bool contains(const T &value) const {
    return std::find(this->begin(), this->end(), value) != this->end();
  }
  void removeAll(const T &value) {
    this->erase(std::remove(this->begin(), this->end(), value), this->end());
  }
};
template <class K, class V> struct QMap : std::map<K, V> {
  int count() const { return static_cast<int>(this->size()); }
};
template <class T> using QSet = std::set<T>;
template <class T> T &asKeyValueRange(T &map) { return map; }
struct QObject { static QString tr(const char *text) { return text; } };
struct QDateTime { static long long currentMSecsSinceEpoch() { return 100000; } };
struct QUrl {
  explicit QUrl(const QString &) {}
  QString host() const { return "example.invalid"; }
};
struct DebugStream { template <class T> DebugStream &operator<<(const T &) { return *this; } };
inline DebugStream qDebug() { return {}; }
inline std::deque<std::function<void()>> jobs;
inline void runOnNewThread(std::function<void()> job) { jobs.push_back(std::move(job)); }
inline void drainJobs() {
  while (!jobs.empty()) {
    auto job = std::move(jobs.front()); jobs.pop_front(); job();
  }
}
inline void MW_show_log(const QString &) {}
inline int completions = 0;
inline void MW_dialog_message(const char *, const char *) { ++completions; }
namespace HappDecrypt {
inline QString decryptLink(const QString &) {
  throw std::runtime_error("Link decryption is outside this offline fixture");
}
}
namespace NetworkRequestHelper {
struct Response { QString error, data = "synthetic subscription"; QMap<QString, QString> header; };
inline Response response;
inline int requests = 0;
inline Response HttpGet(const QString &, bool, const QMap<QString, QString> &, const QByteArray &) {
  ++requests;
  return response; // Deliberately never opens a socket.
}
inline QString GetHeader(const QMap<QString, QString> &headers, const QString &name) {
  auto found = headers.find(name);
  return found == headers.end() ? QString{} : found->second;
}
}
namespace Configs {
struct SyntheticBean {
  QString identity;
  int compare(const SyntheticBean *other, const QList<QString> & = {}) const {
    return identity.compare(other->identity);
  }
};
inline std::map<int, QList<int>> savedGroups;
inline QList<int> savedProfiles;
struct ProxyEntity {
  int id = -1, gid = -1;
  QString type = "synthetic", serverAddress = "example.invalid", name = "synthetic";
  int serverPort = 1;
  std::shared_ptr<SyntheticBean> contents = std::make_shared<SyntheticBean>();
  auto bean() const { return contents; }
  virtual int Id() const { return id; }
  QString DisplayTypeAndName() const { return name; }
  void Save() { savedProfiles << id; }
};
struct GroupExtra { long long sub_last_update = 0; QString info; };
struct Group {
  int id = 1;
  bool archive = false;
  QString name = "Synthetic group";
  QList<int> profiles;
  std::shared_ptr<GroupExtra> extra = std::make_shared<GroupExtra>();
  void Save() { savedGroups[id] = profiles; }
  auto getExtraUnlocked() { return extra; }
  bool RemoveProfile(int);
  bool HasProfile(int) const;
};
struct DataStore { bool sub_clear = false, sub_send_hwid = false; int started_id = -1, current_group = 1, imported_count = 0; };
inline DataStore store;
inline DataStore *dataStore = &store;
enum StoreType { Proxies, Beans };
struct DatabaseManager {
  QList<std::pair<StoreType, int>> dropped;
  void Drop(StoreType type, int id) { dropped << std::pair{type, id}; }
};
inline DatabaseManager database;
inline DatabaseManager *databaseManager = &database;
struct ProfileManager {
  std::map<int, std::shared_ptr<ProxyEntity>> records;
  std::map<int, std::shared_ptr<Group>> groups;
  QList<int> lookedUp, uncached;
  int max_profile_id = 10000;
  auto GetGroup(int id) {
    auto found = groups.find(id);
    return found == groups.end() ? std::shared_ptr<Group>{} : found->second;
  }
  auto GetProfile(int id) {
    lookedUp << id;
    auto found = records.find(id);
    return found == records.end() ? std::shared_ptr<ProxyEntity>{} : found->second;
  }
  void CacheProfile(std::shared_ptr<ProxyEntity> profile) { records[profile->id] = profile; }
  void UncacheProfile(int id, bool) { uncached << id; records.erase(id); }
  void lock() {}
  void unlock() {}
  bool AddProfileBatch(const QList<std::shared_ptr<ProxyEntity>> &, int = -1);
  void BatchDeleteProfiles(const QList<int> &, int = -1);
};
inline ProfileManager manager;
inline ProfileManager *profileManager = &manager;
}
