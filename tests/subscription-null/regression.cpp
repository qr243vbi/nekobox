#include "support.hpp"
#include <nekobox/dataStore/ProfileFilter.hpp>
#include <iostream>

namespace Subscription {
inline QList<std::shared_ptr<Configs::ProxyEntity>> incoming;
inline int nullKeys = 0;
struct RawUpdater {
  int gid_add_to = -1;
  QMap<Configs::ProfileFilterKey, bool> ignore_map;
  QList<std::shared_ptr<Configs::ProxyEntity>> proxies;
  bool AddProxy(std::shared_ptr<Configs::ProxyEntity>);
  void update(const QString &) {
    // Substitute only protocol parsing; execute production AddProxy for each entity.
    for (const auto &[key, matched] : ignore_map) {
      (void)matched;
      if (key.key == nullptr) ++nullKeys;
    }
    for (const auto &profile : incoming) AddProxy(profile);
  }
};
struct GroupUpdater {
  void Update(const std::function<void(std::shared_ptr<Configs::Group>)>,
              const QString &, int, bool = false, bool = false,
              const QMap<QString, QString> & = {}, const QByteArray & = {}, bool = false);
};
}
#include "production.inc"

int checks = 0;
void check(bool condition, const char *message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
auto profile(int id, const char *identity, int gid = 1) {
  auto value = std::make_shared<Configs::ProxyEntity>();
  value->id = id; value->gid = gid; value->contents->identity = identity;
  return value;
}
auto group(int gid, QList<int> ids) {
  auto value = std::make_shared<Configs::Group>();
  value->id = gid; value->profiles = std::move(ids);
  Configs::manager.groups[gid] = value;
  value->Save();
  return value;
}
void run(const std::shared_ptr<Configs::Group> &value,
         QList<std::shared_ptr<Configs::ProxyEntity>> incoming = {}) {
  Subscription::incoming = std::move(incoming);
  int finishes = 0;
  Subscription::GroupUpdater updater;
  updater.Update([&](const auto &finished) {
    ++finishes; check(finished == value, "completion refers to original group");
  }, "https://example.invalid/synthetic", value->id);
  check(finishes == 1, "one pre-finish callback");
  check(Subscription::nullKeys == 0, "no null key reaches subscription matching");
  drainJobs();
  check(Configs::savedGroups.at(value->id) == value->profiles,
        "saved membership matches final group membership");
  check(value->extra->sub_last_update == 100, "subscription metadata update retained");
}
void preserved(const std::shared_ptr<Configs::Group> &value, const QList<int> &expected) {
  check(value->profiles == expected, "group membership and ordering retained");
  check(Configs::savedGroups.at(value->id) == expected, "retained IDs persist on save");
}
void noDeletion() {
  check(Configs::manager.uncached.empty(), "no profile invalidated");
  check(Configs::database.dropped.empty(), "no proxy or bean record deleted");
}

void missing(int count, bool same = false) {
  QList<int> ids;
  for (int i = 0; i < count; ++i) ids << (same ? 901 : 901 + i);
  auto value = group(1, ids);
  run(value);
  preserved(value, ids);
  noDeletion();
  check(Configs::manager.lookedUp == ids, "one lookup per unresolved old entry");
  check(Configs::savedProfiles.empty(), "no fabricated replacement records");
}
void mixed() {
  auto keep = profile(10, "keep");
  auto remove = profile(11, "remove");
  Configs::manager.records = {{10, keep}, {11, remove}};
  auto value = group(1, {901, 10, 902, 11});
  auto add = profile(-1, "add");
  run(value, {profile(-1, "keep"), add});
  preserved(value, {901, 10, 902, 10001});
  check(Configs::manager.records.at(10) == keep, "unchanged valid profile identity retained");
  check(Configs::manager.records.at(10001) == add, "new profile cached with assigned ID");
  check(Configs::manager.uncached == QList<int>{11}, "only removed valid profile invalidated");
  check(Configs::database.dropped == QList<std::pair<Configs::StoreType, int>>{
        {Configs::Proxies, 11}, {Configs::Beans, 11}}, "only removed valid profile records deleted");
  check(Configs::savedProfiles == QList<int>{10001}, "only new profile saved");
}
void duplicates() {
  auto first = profile(10, "duplicate");
  Configs::manager.records = {{10, first}, {11, profile(11, "duplicate")}};
  auto value = group(1, {901, 10, 11, 901});
  run(value, {profile(-1, "duplicate")});
  preserved(value, {901, 10, 901});
  check(Configs::manager.records.at(10) == first, "first equivalent valid profile retained");
  check(Configs::manager.uncached == QList<int>{11}, "existing duplicate removal contract retained");
  check(Configs::database.dropped.size() == 2, "only duplicate proxy and bean removed");
  check(Configs::savedProfiles.empty(), "equivalent incoming profile not re-added");
}
void unchanged() {
  auto keep = profile(10, "keep");
  Configs::manager.records[10] = keep;
  auto value = group(1, {10});
  run(value, {profile(-1, "keep")});
  preserved(value, {10}); noDeletion();
  check(Configs::manager.records.at(10) == keep, "same valid object kept");
  check(Configs::savedProfiles.empty(), "unchanged update writes no new entity");
}
void repeatedGroups() {
  for (int gid = 1; gid <= 12; ++gid) {
    auto old = profile(100 + gid, "keep", gid);
    Configs::manager.records[old->id] = old;
    group(gid, {100 + gid, 900 + gid});
  }
  for (int repeat = 0; repeat < 3; ++repeat) {
    for (int gid = 1; gid <= 12; ++gid) {
      auto value = Configs::manager.groups.at(gid);
      run(value, {profile(-1, "keep")});
      preserved(value, {100 + gid, 900 + gid});
    }
  }
  noDeletion();
  check(Configs::manager.records.size() == 12, "all valid records survive repeated groups");
  check(completions == 36, "all sequential group updates finish");
  check(Configs::savedProfiles.empty(), "repeated unchanged groups add no profiles");
}
void recoveredLookup() {
  auto value = group(1, {901});
  run(value);
  auto recovered = profile(901, "recovered");
  Configs::manager.records[901] = recovered;
  run(value, {profile(-1, "recovered")});
  preserved(value, {901}); noDeletion();
  check(Configs::manager.records.at(901) == recovered, "later successful load retains original profile");
  check(Configs::savedProfiles.empty(), "retry does not duplicate recovered record");
}
void clearBoundary(bool explicitClear) {
  Configs::store.sub_clear = explicitClear;
  QList<int> ids;
  for (int i = 0; i < (explicitClear ? 2 : 1001); ++i) ids << 900 + i;
  auto value = group(1, ids);
  run(value, {profile(-1, "new")});
  preserved(value, {10001});
  check(Configs::savedProfiles == QList<int>{10001}, "clear mode retains existing add behavior");
  noDeletion(); // Missing records do not cause database deletion, even in clear mode.
}
void startedProfile() {
  Configs::manager.records[10] = profile(10, "running");
  Configs::store.started_id = 10;
  auto value = group(1, {901, 10});
  run(value);
  preserved(value, {901, 10}); noDeletion();
}
void networkError() {
  auto value = group(1, {901, 902});
  NetworkRequestHelper::response.error = "synthetic failure";
  Subscription::GroupUpdater updater;
  updater.Update([](const auto &) { throw std::runtime_error("unexpected completion"); },
                 "https://example.invalid/synthetic", 1);
  preserved(value, {901, 902}); noDeletion();
  check(Configs::manager.lookedUp.empty(), "failed request never starts old-profile lookup");
  check(jobs.empty(), "failed request schedules no storage changes");
}
int main(int argc, char **argv) {
  try {
    if (argc != 2) throw std::runtime_error("Expected one case name");
    const std::string name = argv[1];
    if (name == "one_missing") missing(1);
    else if (name == "two_missing") missing(2);
    else if (name == "repeated_missing_id") missing(2, true);
    else if (name == "mixed") mixed();
    else if (name == "duplicates") duplicates();
    else if (name == "unchanged") unchanged();
    else if (name == "repeated_groups") repeatedGroups();
    else if (name == "recovered_lookup") recoveredLookup();
    else if (name == "incremental_boundary") missing(1000);
    else if (name == "automatic_clear") clearBoundary(false);
    else if (name == "explicit_clear") clearBoundary(true);
    else if (name == "started_profile") startedProfile();
    else if (name == "network_error") networkError();
    else throw std::runtime_error("Unknown case");
    std::cout << "PASS " << name << " checks=" << checks << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << argv[1] << ": " << error.what() << '\n';
    return 1;
  }
}
