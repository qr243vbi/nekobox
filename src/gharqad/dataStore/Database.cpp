#include "nekobox/dataStore/ConfigItem.hpp"
#include "nekobox/dataStore/Utils.hpp"
#include <string_view>
#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/Database.hpp>
#include <nekobox/dataStore/DatabaseLMDB.hpp>

#include <QDir>
#include <QMutex>
#include <chrono>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/configs/proxy/includes.h>

#ifdef NKR_SOFTWARE_KEYS
#include <nekobox/ui/security_addon.h>
int Stats::DatabaseLogger::get_failed_auth_count(bool pre_save) {
  auto ret = this->failed_auth_count + getFailedCount(pre_save);
  if (pre_save) {
    this->failed_auth_count = ret;
  }
  return ret;
}
#endif

[[nodiscard]] QString Stats::TrafficData::DisplaySpeed() const {
  return UNICODE_LRO + QString("%1↑ %2↓").arg(ReadableSize(uplink_rate),
                                              ReadableSize(downlink_rate));
}

[[nodiscard]] QString Stats::TrafficData::DisplayTraffic() const {
  if (downlink + uplink == 0)
    return "";
  return UNICODE_LRO +
         QString("%1↑ %2↓").arg(ReadableSize(uplink), ReadableSize(downlink));
}

long long Stats::DatabaseLogger::get_usage_time() {
  auto ret = this->usage_time;
  ret += GetTime() - this->usage_time_update;
  return ret;
}

void Stats::DatabaseLogger::initialize() {
  Stats::databaseLogger->start_count++;
  auto time = GetTime();
  if (this->first_launch_time == 0) {
    this->first_launch_time = time;
  }
  this->last_launch_time = time;
  this->usage_time_update = time;
}

bool Stats::DatabaseLogger::Save() {
#ifdef NKR_SOFTWARE_KEYS
  get_failed_auth_count(true);
#endif
  auto ret = get_usage_time();
  this->usage_time_update = 0;
  this->usage_time = ret;
  return JsonStore::Save();
}

long Stats::DatabaseLogger::GetTime() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  return millis;
}

namespace Configs {
QString getJsonStoreFileName(short type, long id) {
  switch (type) {
  case Routes:
  case Proxies:
  case Groups:
  case Beans:
    break;
  case Shortcuts:
    return "shortcuts.cfg";
  case ResourceManager:
    return "resource_manager.cfg";
  case ProxyManager:
    return "profiles.cfg";
  case NekoBox:
    return "nekobox.cfg";
  case DefaultRoute:
    return "default_route_profile.cfg";
  case TrafficLooper:
    return "stats/traffic.cfg";
  case DatabaseLogger:
    return "stats/usage.cfg";
  default:
    return "";
  }
  {
    QString str;
    if (id < 0)
      return "";
    switch (type) {
    case Routes:
      str = "route_profiles/%1.cfg";
      break;
    case Proxies:
      str = "profiles/%1.cfg";
      break;
    case Groups:
      str = "groups/%1.cfg";
      break;
    case Beans:
      str = "beans/%1.cfg";
      break;
    }
    return str.arg(id);
  }
};

QString getJsonStorePathName(char type) {
  switch (type) {
  case Routes:
    return "route_profiles";
  case Proxies:
    return "profiles";
  case Groups:
    return "groups";
  case Beans:
    return "beans";
  default:
    return "";
  }
}

QMutex storeMutex;
#define LOG_CREATE(CASE, VAR)                                                  \
  case CASE:                                                                   \
    if (!store->storage_exists()) {                                            \
      storeMutex.lock();                                                       \
      Stats::databaseLogger->VAR->created += 1;                                \
      store->storage_exists(true);                                             \
      storeMutex.unlock();                                                     \
    };                                                                         \
    break;

#define LOG_DELETE(CASE, VAR)                                                  \
  case CASE: {                                                                 \
    storeMutex.lock();                                                         \
    Stats::databaseLogger->VAR->deleted += 1;                                  \
    storeMutex.unlock();                                                       \
  }; break;

bool FileDatabaseManager::Save(JsonStore *store) {
  switch (store->StoreType()) {
    LOG_CREATE(Routes, routes)
    LOG_CREATE(Proxies, profiles)
    LOG_CREATE(Groups, groups)
  }
  #ifndef SKIP_LMDB
  if (Configs::config_type == Configs::DatabaseType::lmdb_type){
    return Configs::write_lmdb(this->database, store);
  }
  #endif
  auto ret = SaveToFile(store);
  #ifndef SKIP_LMDB
  if (ret){
    Configs::clear_lmdb(this->database, store);
  }
  #endif
  return ret;
}
bool FileDatabaseManager::Load(JsonStore *store) { 
  #ifndef SKIP_LMDB
  auto [ok, readed] = Configs::read_lmdb(this->database, store);
  if (!ok){
    return false;
  } else if (readed){
    return true;
  }
  #endif
  return LoadFromFile(store); 
}
bool FileDatabaseManager::Drop(char chr, int id) {
  bool ret = DropFromDirectory(chr, id);
  #ifndef SKIP_LMDB
  #endif
  if (ret) {
    switch (chr) {
      LOG_DELETE(Routes, routes);
      LOG_DELETE(Proxies, profiles);
      LOG_DELETE(Groups, groups);
    }
  }
  return ret;
}


FileDatabaseManager::FileDatabaseManager()
    #ifndef SKIP_LMDB
    : database(initialize_lmdb())
    #endif
{
};


FileDatabaseManager::~FileDatabaseManager(){
  #ifndef SKIP_LMDB
  this->database.close();
  #endif
};

bool FileDatabaseManager::SaveToFile(JsonStore *store) {
  auto type = store->StoreType();
  auto id = store->Id();
  auto path = getJsonStoreFileName(type, id);
  if (path == "") {
    return false;
  }
  if (Configs::config_type == Configs::DatabaseType::ini_type){
    store->SaveINI(QFileInfo(path), "");
    return true;
  }
  return store->SaveToFile(path, Configs::config_type == Configs::DatabaseType::json_type);
}
bool FileDatabaseManager::LoadFromFile(JsonStore *store) {
  auto type = store->StoreType();
  auto id = store->Id();
  auto path = getJsonStoreFileName(type, id);
  if (path == "") {
    return false;
  }
  auto ret = store->LoadFromFile(path);
  return true;
}

bool FileDatabaseManager::DropFromDirectory(char chr, int id) {
  QString fn = getJsonStorePathName(chr);
  if (fn != "") {
    QFile file(fn + QDir::separator() + QString::number(id) + ".cfg");
    return file.remove();
  }
  return false;
}

QList<int> FileDatabaseManager::Query(char type) {
  #ifdef SKIP_LMDB
  return FileDatabaseManager::QueryFromDirectory(type);
  #else
  return Configs::query_lmdb(type);
  #endif
}

QList<int> FileDatabaseManager::QueryFromDirectory(char type) {
  QList<int> result;
  QString fn = getJsonStorePathName(type);
  if (fn == "") {
    return result;
  }
  QDir dr(fn);
  auto entryList = dr.entryList(QDir::Files);
  for (auto e : entryList) {
    e = e.toLower();
    if (!e.endsWith(".cfg", Qt::CaseInsensitive))
      continue;
    e = e.remove(".cfg", Qt::CaseInsensitive);
    bool ok;
    auto id = e.toInt(&ok);
    if (ok) {
      result << id;
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}
} // namespace Configs

// Query
#ifndef SKIP_LMDB
std::string Configs::pack_char_int(char c, int32_t x) {
  uint32_t u = static_cast<uint32_t>(x); // preserve bit pattern
  std::string s(5, '\0');
  s[0] = c;
  s[1] = static_cast<char>( u         & 0xFF);
  s[2] = static_cast<char>((u >> 8)  & 0xFF);
  s[3] = static_cast<char>((u >> 16) & 0xFF);
  s[4] = static_cast<char>((u >> 24) & 0xFF);
  return s;
}

std::tuple<char, int32_t> Configs::unpack_char_int(const std::string_view& key) {
  // Expect key size == 5: [0]=char, [1..4]=int32 bytes (little-endian)
  // Caller can decide whether to validate; here we do minimal validation.
  if (key.size() != 5) {
    throw std::runtime_error("Invalid key size for unpack_char_int_portable");
  }

  char c = key[0];

  uint32_t u = 0;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[1])) << 0;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[2])) << 8;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[3])) << 16;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[4])) << 24;

  int32_t x = static_cast<int32_t>(u); // restores original bit-pattern to signed int32
  return std::tie(c, x);
}

bool Configs::clear_lmdb(lmdb::env& env, Configs_ConfigItem::JsonStore * store){
  return clear_lmdb(env, store->StoreType(), store->Id());
}

bool Configs::clear_lmdb(lmdb::env& env, char c, int32_t x){
  return Configs::write_lmdb(env, c, x, "");
}

bool Configs::write_lmdb(lmdb::env& env, Configs_ConfigItem::JsonStore * store){
  auto bytes = store->ToBytes();
  return write_lmdb(env, store->StoreType(), store->Id(), bytes.data());
}

bool Configs::write_lmdb(lmdb::env& env, char c, int32_t x, const std::string_view &view){
  auto key = pack_char_int(c, x);
  lmdb::dbi dbi;
  // Get the dbi handle, and insert some key/value pairs in a write transaction:
  auto wtxn = lmdb::txn::begin(env);
  dbi = lmdb::dbi::open(wtxn, nullptr);
  bool ret = dbi.put(wtxn, key, view);
  wtxn.commit();
  return ret;
}

std::tuple<bool, bool> Configs::read_lmdb(lmdb::env& env, Configs_ConfigItem::JsonStore * store){
  auto bytes = store->ToBytes();
  auto [view, isok] = read_lmdb(env, store->StoreType(), store->Id());
  bool readed = false;
  if (isok) {
    if (view.size() > 0){
      readed = true;
      store->FromBytes(view.data());
    }
  }
  return std::make_tuple(isok, readed);
}

std::tuple<std::string_view, bool> Configs::read_lmdb(lmdb::env& env, char c, int32_t x) {
  auto key_data = pack_char_int(c, x);

  lmdb::txn txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
  lmdb::dbi dbi = lmdb::dbi::open(txn, nullptr);
  std::string_view ret;

  bool found = dbi.get(txn, key_data, ret);

  txn.abort(); // read-only safety (or txn.commit(); also fine)

  if (!found) {
    return std::make_tuple("", false); // or throw / optional handling
  }

  return std::make_tuple(ret, true);
}


#define DATABASE_NAME "db"
lmdb::env Configs::initialize_lmdb(){
  QDir dir(".");
  bool init_db = false;
  if (!dir.exists(DATABASE_NAME)){
    dir.mkdir(DATABASE_NAME);
    init_db = true;
  }
  auto env = lmdb::env::create();
  env.open(DATABASE_NAME, 0, 0664);
  if (init_db){
    auto wtxn = lmdb::txn::begin(env);
    lmdb::dbi dbi = lmdb::dbi::open(wtxn, nullptr);
    std::vector<char> types = {Proxies, Beans, Routes, Groups};
    for (char c : types){
      auto ids = FileDatabaseManager::QueryFromDirectory(c);
      for (int x : ids){
        dbi.put(wtxn, pack_char_int(c, x), "");
      }
    }
    std::vector<char> common_types = {Shortcuts, ResourceManager, ProxyManager, NekoBox, DefaultRoute, TrafficLooper, DatabaseLogger};
    for (char c : common_types){
      dbi.put(wtxn, pack_char_int(c, 0), "");
    }
    wtxn.commit();
  }
  return env;
}
#undef DATABASE_NAME

#endif

namespace Configs {
ProfileManager *profileManager = new ProfileManager();

ProfileManager::ProfileManager() : JsonStore() {}

DECL_MAP(ProfileManager)
ADD_MAP("groups", groupsTabOrder, integerList);
STOP_MAP

void ProfileManager::LoadManager() {
  JsonStore::Load();
  //
  profiles = {};
  groups = {};
  routes = {};
  profilesIdOrder =
      Configs::databaseManager->Query(Configs::JsonStoreType::Proxies);
  groupsIdOrder =
      Configs::databaseManager->Query(Configs::JsonStoreType::Groups);
  routesIdOrder =
      Configs::databaseManager->Query(Configs::JsonStoreType::Routes);
  // Load Proxys
  QList<int> delProfile;
  for (auto id : profilesIdOrder) {
#ifdef DEBUG_MODE
    qDebug() << "Load Profile With ID" << id;
#endif
    auto ent = LoadProxyEntity(id);
    // Corrupted profile?
    if (ent == nullptr || !ent->isValid()) {
      delProfile << id;
      continue;
    }
    profiles[id] = ent;
    if (ent->type == "extracore")
      extraCorePaths.insert(ent->ExtraCoreBean()->extraCorePath);
  }
  // Clear Corrupted profile
  for (auto id : delProfile) {
    deleteProfile(id);
  }
  // Load Groups
  auto loadedOrder = groupsTabOrder;
  groupsTabOrder = {};
  auto needToCheckGroups = QSet<int>();
  for (auto id : groupsIdOrder) {
    auto ent = LoadGroup(id);
    // Corrupted group?
    if (ent->id != id) {
      continue;
    }
    // Ensure order contains every group
    if (!loadedOrder.contains(id)) {
      loadedOrder << id;
    }
    groups[id] = ent;
    if (ent->profiles.isEmpty())
      needToCheckGroups << id;
  }
  QList<int> orphanProfiles;
  for (const auto &[id, proxy] : profiles) {
    // corrupted data
    if (!groups.contains(proxy->gid) ||
        (!needToCheckGroups.contains(proxy->gid) &&
         !groups[proxy->gid]->HasProfile(id))) {
      orphanProfiles << id;
      continue;
    }
    if (needToCheckGroups.contains(proxy->gid))
      groups[proxy->gid]->AddProfile(id);
  }
  for (int id : orphanProfiles) {
    deleteProfile(id);
  }
  for (const auto groupID : needToCheckGroups) {
    groups[groupID]->Save();
  }
  // Ensure groups contains order
  for (auto id : loadedOrder) {
    if (groups.count(id)) {
      groupsTabOrder << id;
    }
  }
  // Load Routing profiles
  for (auto id : routesIdOrder) {
    auto route = LoadRouteChain(id);
    if (route == nullptr) {
      MW_show_log(
          QString("File route_profiles/%1.cfg is corrupted, consider delete it")
              .arg(id));
      continue;
    }

    routes[id] = route;
  }

  // First setup
  if (groups.empty()) {
    auto defaultGroup = NewGroup();
    defaultGroup->name = QObject::tr("Default");
    profileManager->AddGroup(defaultGroup);
  }

  // First setup
  if (routes.empty()) {
    auto defaultRoute = RoutingChain::GetDefaultChain();
    profileManager->AddRouteChain(defaultRoute);
  }
}

void ProfileManager::SaveManager() { JsonStore::Save(); }

std::shared_ptr<ProxyEntity> ProfileManager::LoadProxyEntity(
    //    const QString &jsonPath,
    //    const QString &beanPath
    int id) {
  // Load type
  auto ent = NewProxyEntity("");
  ent->id = id;
  //   ent->fn = jsonPath;
  bool validType = ent->Load();

  if (!validType) {
#ifdef DEBUG_MODE
    qDebug() << "LOADING FAILED:" << "profiles/" << id << ".cfg";
#endif
    ent->type = "";
    return ent;
  }
  if (!ent->isValid()) {
#ifdef DEBUG_MODE
    qDebug() << "UNKNOWN TYPE:" << ent->type;
#endif
    return ent;
  }
#ifdef DEBUG_MODE
  qDebug() << "Migration Mode" << ent->same_path_for_bean();
#endif
  std::shared_ptr<AbstractBean> bean;
  if (ent->same_path_for_bean()) {
    bean = ent->unlock(ent->bean());
    bean->Load();
    ent->same_path_for_bean(false);
    bean->Load();
    bean->Save();
    ent->Save();
  }
  ent->ResetBeans();
#ifdef DEBUG_MODE
  qDebug() << "Profiles loaded, beans resetted";
#endif
  return ent;
}

std::shared_ptr<RoutingChain> ProfileManager::LoadRouteChain(int id) {
  auto routingChain = std::make_shared<RoutingChain>();
  routingChain->id = id;
  ;
  if (!routingChain->Load()) {
    return nullptr;
  }

  return routingChain;
}

QString ProfileManager::GetDisplayType(const QString &type) {
  return Preset::SingBox::OutboundTypes.value(type, type);
}

std::shared_ptr<ProxyEntity>
ProfileManager::NewProxyEntity(const QString &type) {
  //    Configs::AbstractBean *bean = nullptr;
  auto ent = std::make_shared<ProxyEntity>(type);
  return ent;
}

std::shared_ptr<Group> ProfileManager::NewGroup() {
  auto ent = std::make_shared<Group>();
  return ent;
}

// Profile

int ProfileManager::NewProfileID() const {
  if (profiles.empty()) {
    return 0;
  } else {
    return profilesIdOrder.last() + 1;
  }
}

bool ProfileManager::AddProfile(const std::shared_ptr<ProxyEntity> &ent,
                                int gid) {
  QList<std::shared_ptr<ProxyEntity>> list;
  list << ent;
  return ProfileManager::AddProfileBatch(list, gid);
}

bool ProfileManager::AddProfileBatch(
    const QList<std::shared_ptr<ProxyEntity>> &ents, int gid) {
  gid = gid < 0 ? dataStore->current_group : gid;
  auto group = GetGroup(gid);
  if (group == nullptr)
    return false;
  for (const auto &ent : ents) {
    if (ent->id >= 0)
      continue;
    int id = ent->id = NewProfileID();
    ent->gid = gid;
    group->AddProfile(id);
    profiles[id] = ent;
    profilesIdOrder.push_back(id);
  }
  group->Save();
  runOnNewThread([=, this] {
    for (const auto &ent : ents)
      ent->Save();
  });
  return true;
}

bool ProfileManager::MoveProfile(int id, int gid) {
  QList<int> list;
  list << id;
  return ProfileManager::MoveProfileBatch(list, gid);
}

bool ProfileManager::MoveProfileBatch(const QList<int> &ents, int gid) {
  QList<std::shared_ptr<ProxyEntity>> entsp;
  for (int i : ents) {
    entsp << this->GetProfile(i);
  }
  return this->MoveProfileBatch(entsp, gid);
}

bool ProfileManager::MoveProfileBatch(
    const QList<std::shared_ptr<ProxyEntity>> &ents, int mgid) {
  if (this->groups.count(mgid) == 0) {
    return false;
  }
  bool added = false;
  auto group = this->groups[mgid];
  QSet<std::shared_ptr<Group>> ments;
  QSet<std::shared_ptr<Group>> grps;
  for (auto ent : ents) {
    int id = ent->id;
    int gid = ent->gid;
    if (gid == mgid) {
      continue;
    }
    if (this->groups.count(gid) == 0) {
      continue;
    }
    auto grp = this->groups[gid];
    grp->RemoveProfile(id);
    grps.insert(grp);
    group->AddProfile(id);
    ent->gid = mgid;
    added = true;
  }
  if (added) {
    grps.insert(group);
    runOnNewThread([=, this] {
      for (const auto &grp : grps)
        grp->Save();
      for (const auto &ent : ments)
        ent->Save();
    });
  }
  return true;
}

void ProfileManager::DeleteProfile(int id) {
  if (id < 0)
    return;
  if (dataStore->started_id == id)
    return;
  auto ent = GetProfile(id);
  if (ent == nullptr)
    return;
  if (auto group = GetGroup(ent->gid); group != nullptr) {
    group->RemoveProfile(id);
    group->Save();
  }
  deleteProfile(id);
}

void ProfileManager::BatchDeleteProfiles(const QList<int> &ids) {
  QSet<std::shared_ptr<Group>> changed_groups;
  QSet<int> deleted_ids;
  for (auto id : ids) {
    if (id < 0)
      continue;
    if (dataStore->started_id == id)
      continue;
    auto ent = GetProfile(id);
    if (ent == nullptr)
      continue;
    if (auto group = GetGroup(ent->gid); group != nullptr) {
      group->RemoveProfile(id);
      changed_groups.insert(group);
    }
    profiles.erase(id);
    deleted_ids.insert(id);
  }
  for (const auto &group : changed_groups)
    group->Save();
  QList<int> newOrder;
  for (auto id : profilesIdOrder) {
    if (deleted_ids.contains(id))
      continue;
    newOrder.append(id);
  }
  profilesIdOrder = newOrder;

  runOnNewThread([=, this] {
    for (int id : deleted_ids) {
      Configs::databaseManager->Drop(Proxies, id);
      Configs::databaseManager->Drop(Beans, id);
    }
  });
}

void ProfileManager::deleteProfile(int id) {
  QList<int> ids;
  ids << id;
  BatchDeleteProfiles(ids);
}

std::shared_ptr<ProxyEntity> ProfileManager::GetProfile(int id) {
  return profiles.contains(id) ? profiles[id] : nullptr;
}

QStringList ProfileManager::GetExtraCorePaths() const {
  return extraCorePaths.values();
}

bool ProfileManager::AddExtraCorePath(const QString &path) {
  if (extraCorePaths.contains(path)) {
    return false;
  }
  extraCorePaths.insert(path);
  return true;
}
// Group

std::shared_ptr<Group> ProfileManager::LoadGroup(int jsonPath) {
  auto ent = std::make_shared<Group>();
#ifdef DEBUG_MODE
  qDebug() << "Group Is: " << jsonPath;
#endif
  ent->id = jsonPath;
  ent->Load();
  return ent;
}

int ProfileManager::NewGroupID() const {
  if (groups.empty()) {
    return 0;
  } else {
    return groupsIdOrder.last() + 1;
  }
}

bool ProfileManager::AddGroup(const std::shared_ptr<Group> &ent) {
  if (ent->id >= 0) {
    return false;
  }

  ent->id = NewGroupID();
  groups[ent->id] = ent;
  groupsIdOrder.push_back(ent->id);
  groupsTabOrder.push_back(ent->id);

  ent->Save();
  return true;
}

void ProfileManager::DeleteGroup(int gid) {
  if (groups.size() <= 1)
    return;
  auto group = GetGroup(gid);
  if (group == nullptr)
    return;
  for (const auto id : group->Profiles()) {
    deleteProfile(id);
  }
  groups.erase(gid);
  groupsIdOrder.removeAll(gid);
  groupsTabOrder.removeAll(gid);
  Configs::databaseManager->Drop(Groups, gid);
}

std::shared_ptr<Group> ProfileManager::GetGroup(int id) {
  return groups.count(id) ? groups[id] : nullptr;
}

std::shared_ptr<Group> ProfileManager::CurrentGroup() {
  return GetGroup(dataStore->current_group);
}

std::shared_ptr<RoutingChain> ProfileManager::NewRouteChain() {
  auto route = std::make_shared<RoutingChain>();
  return route;
}

int ProfileManager::NewRouteChainID() const {
  if (routes.empty()) {
    return 0;
  }
  return routesIdOrder.last() + 1;
}

bool ProfileManager::AddRouteChain(const std::shared_ptr<RoutingChain> &chain) {
  if (chain->id >= 0) {
    return false;
  }

  chain->id = NewRouteChainID();
  routes[chain->id] = chain;
  routesIdOrder.push_back(chain->id);
  chain->Save();

  return true;
}

std::shared_ptr<RoutingChain> ProfileManager::GetRouteChain(int id) {
  return routes.count(id) > 0 ? routes[id] : nullptr;
}

void ProfileManager::UpdateRouteChains(
    const QList<std::shared_ptr<RoutingChain>> &newChain) {
  routes.clear();
  routesIdOrder.clear();

  for (const auto &item : newChain) {
    if (!AddRouteChain(item)) {
      routes[item->id] = item;
      routesIdOrder << item->id;
      item->Save();
    }
  }
  auto currFiles =
      Configs::databaseManager->Query(Configs::JsonStoreType::Routes);
  for (const auto &item : currFiles) { // clean up removed route profiles
    if (!routes.count(item)) {
      Configs::databaseManager->Drop(Routes, item);
    }
  }
}
} // namespace Configs
