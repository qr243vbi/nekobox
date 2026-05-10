#include <memory>
#ifndef SKIP_ROCKSDB
#include <rocksdb/write_batch.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ConfigItem.hpp>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <string_view>

#include <nekobox/dataStore/Database.hpp>
#include <nekobox/dataStore/DatabaseRocksDB.hpp>

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
#ifndef SKIP_ROCKSDB
  if (Configs::config_type == Configs::DatabaseType::rocksdb_type) {
    return Configs::write_rocksdb(this->database, store);
  }
#endif
  auto ret = SaveToFile(store);
#ifndef SKIP_ROCKSDB
  if (ret) {
    Configs::clear_rocksdb(this->database, store);
  }
#endif
  return ret;
}
bool FileDatabaseManager::Load(JsonStore *store) {
#ifndef SKIP_ROCKSDB
  auto [ok, readed] = Configs::read_rocksdb(this->database, store);
  if (!ok) {
    return false;
  } else if (readed) {
    if (Configs::config_type != Configs::DatabaseType::rocksdb_type) {
      if (SaveToFile(store)) {
        Configs::clear_rocksdb(this->database, store);
      };
    }
    return true;
  }
#else
  bool readed;
#endif
  readed = LoadFromFile(store);
#ifndef SKIP_ROCKSDB
  if (readed) {
    if (Configs::config_type == Configs::DatabaseType::rocksdb_type) {
      Configs::write_rocksdb(this->database, store);
      DropFromDirectory(store->StoreType(), store->Id());
    }
  }
#endif
  return readed;
}
bool FileDatabaseManager::Drop(char chr, int id) {
  bool ret = DropFromDirectory(chr, id);
#ifndef SKIP_ROCKSDB
  ret = Configs::drop_rocksdb(this->database, chr, id);
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
{
#ifndef SKIP_ROCKSDB
  initialize_rocksdb(this->database);
#endif
};

FileDatabaseManager::~FileDatabaseManager() {
#ifndef SKIP_ROCKSDB
//  this->database->CompactRange(rocksdb::CompactRangeOptions(), nullptr, nullptr);
  this->database->Close();
#endif
};

bool FileDatabaseManager::SaveToFile(JsonStore *store) {
  auto type = store->StoreType();
  auto id = store->Id();
  auto path = getJsonStoreFileName(type, id);
  if (path == "") {
    return false;
  }
  if (Configs::config_type == Configs::DatabaseType::ini_type) {
    store->SaveINI(QFileInfo(path), "");
    return true;
  }
  return store->SaveToFile(path, Configs::config_type ==
                                     Configs::DatabaseType::json_type);
}
#ifndef SKIP_ROCKSDB
static rocksdb::ReadOptions ReadOptions(){
  rocksdb::ReadOptions read_options;
  // Ensure you see only committed data
  read_options.verify_checksums = true;
  // Prevent returning data that is being compacted/obsolete
  read_options.fill_cache = true;
  // Stronger consistency during iteration 
  read_options.snapshot = nullptr;
  return read_options;
} 
static rocksdb::WriteOptions WriteOptions() {
  rocksdb::WriteOptions write_options;
  // Force durability before returning success
  write_options.sync = true;
  // Ensure WAL is always used (never bypass durability log)
  write_options.disableWAL = false;
  // Prevent write reordering issues in complex workloads
  write_options.no_slowdown = false;
  write_options.low_pri = false;
  return write_options;
}

static rocksdb::Options Options() {
  rocksdb::Options options;
  options.create_if_missing = true;
  // Strong consistency checks
  options.paranoid_checks = true;
  // never auto-delete WAL
  options.WAL_ttl_seconds = 0;   
  // no size-based trimming   
  options.WAL_size_limit_MB = 0;   
  // flush in shutdown
  options.avoid_flush_during_shutdown = false;
  // auto compaction
  options.disable_auto_compactions = false; 
  return options;
}
#endif

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
  QString fn = getJsonStoreFileName(chr, id);
  if (fn != "") {
    QFile file(fn);
    return file.remove();
  }
  return false;
}

QList<int> FileDatabaseManager::Query(char type) {
#ifdef SKIP_ROCKSDB
  return FileDatabaseManager::QueryFromDirectory(type);
#else
  return Configs::query_rocksdb(this->database, type);
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
#ifndef SKIP_ROCKSDB
QList<int> Configs::query_rocksdb(std::unique_ptr<rocksdb::DB>& db, char c) {
  QList<int> result;
  QSet<uint32_t> result_set;

  std::unique_ptr<rocksdb::Iterator> it(
      db->NewIterator(ReadOptions()));

  std::string prefix = "";
  prefix += c;
  
  it->Seek(prefix);
  #ifdef DEBUG_MODE
    qDebug() << "VALID? " << it->Valid();
  #endif

  while(it->Valid()) {
  #ifdef DEBUG_MODE
    qDebug() << "Slicing? " << it->Valid();
  #endif
    rocksdb::Slice key = it->key();

    if (key.size() != 5) {
#ifdef DEBUG_MODE
      qDebug() << "ERROR: Corrupted database";
#endif
      throw std::runtime_error("Invalid key size");
    }

    const char* k = key.data();

    // prefix changed -> stop
    if (k[0] != c) {
#ifdef DEBUG_MODE
      qDebug() << "Found"
               << result.count()
               << "elements for type"
               << int(c);
#endif
      break;
    }

    // decode little-endian uint32
    const unsigned char* b =
        reinterpret_cast<const unsigned char*>(k);

    uint32_t u =
        (uint32_t(b[1]) << 0)  |
        (uint32_t(b[2]) << 8)  |
        (uint32_t(b[3]) << 16) |
        (uint32_t(b[4]) << 24);

    if (!result_set.contains(u)) {

      int i = static_cast<int32_t>(u);

      if (i < 0) {
        goto continue_please;
      }

      result.append(i);
      result_set.insert(u);
    }

    continue_please:
    it->Next();
  };

  if (!it->status().ok()) {
    throw std::runtime_error(
        it->status().ToString());
  }

  return result;
}

std::string Configs::pack_char_int(char type, int32_t id) {
  uint32_t u = static_cast<uint32_t>(id); // preserve bit pattern
  std::string s(5, '\0');
  s[0] = type;
  s[1] = static_cast<char>(u & 0xFF);
  s[2] = static_cast<char>((u >> 8) & 0xFF);
  s[3] = static_cast<char>((u >> 16) & 0xFF);
  s[4] = static_cast<char>((u >> 24) & 0xFF);
  return s;
}

std::tuple<char, int32_t>
Configs::unpack_char_int(const std::string &slice) {
  // Expect key size == 5: [0]=char, [1..4]=int32 bytes (little-endian)
  // Caller can decide whether to validate; here we do minimal validation.
  if (slice.size() != 5) {
    throw std::runtime_error("Invalid key size for unpack_char_int_portable");
  }

  const char * key = slice.data();
  char c = key[0];

  uint32_t u = 0;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[1])) << 0;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[2])) << 8;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[3])) << 16;
  u |= static_cast<uint32_t>(static_cast<unsigned char>(key[4])) << 24;

  int32_t x =
      static_cast<int32_t>(u); // restores original bit-pattern to signed int32
  return std::tie(c, x);
}

bool Configs::clear_rocksdb(std::unique_ptr<rocksdb::DB>&env, Configs_ConfigItem::JsonStore *store) {
  return clear_rocksdb(env, store->StoreType(), store->Id());
}

bool Configs::clear_rocksdb(std::unique_ptr<rocksdb::DB>&env, char c, int32_t x) {
#ifdef DEBUG_MODE
  qDebug() << "Clearing RocksDB ";
#endif
  return Configs::write_rocksdb(env, c, x, "");
}

bool Configs::drop_rocksdb(std::unique_ptr<rocksdb::DB>&env, char c, int32_t x) {
// std::lock_guard<std::mutex> lock(env.env_mutex);
#ifdef DEBUG_MODE
  qDebug() << "Drop RocksDB ";
#endif
  auto key = pack_char_int(c, x);
  auto status = env->Delete(
        WriteOptions(),
        key
  );
  return status.ok();
}

bool Configs::write_rocksdb(std::unique_ptr<rocksdb::DB>&env, Configs_ConfigItem::JsonStore *store) {
  auto data = store->ToBytes().toStdString();
#ifdef DEBUG_MODE
  qDebug() << "Writing data" << data.size();
#endif
  return write_rocksdb(env, store->StoreType(), store->Id(), data);
}

bool Configs::write_rocksdb(std::unique_ptr<rocksdb::DB>&env, char c, int32_t x,
                         const std::string &view) {
      auto key = pack_char_int(c, x);
      rocksdb::WriteBatch batch;
      batch.Put(key, view);
      auto status =
      env->Write(
        WriteOptions(),
        &batch
      );
      #ifdef DEBUG_MODE
        qDebug() << "Status is: " << status.ToString() << " Size is: " << view.size();
      #endif
      return status.ok();
}

std::tuple<bool, bool>
Configs::read_rocksdb(std::unique_ptr<rocksdb::DB>&env, Configs_ConfigItem::JsonStore *store) {

#ifdef DEBUG_MODE
  qDebug() << "READING RocksDB FILE";
#endif
  auto bytes = store->ToBytes();
  std::string view;
  bool isok = read_rocksdb(env, store->StoreType(), store->Id(), view);
  bool readed = false;
  if (isok) {
    if (view.size() > 0) {
      readed = true;
#ifdef DEBUG_MODE
      qDebug() << "READED DATA" << "ID" << store->Id() << "TYPE"
               << (int)store->StoreType() << "LEN" << view.size();
#endif
      QByteArray ba =
          QByteArray::fromRawData(view.data(), static_cast<int>(view.size()));
      store->FromBytes(ba);
    }
  }
#ifdef DEBUG_MODE
  else {
    qDebug() << "RocksDB is not readed";
  }
#endif
  return std::make_tuple(isok, readed);
}

bool Configs::read_rocksdb(std::unique_ptr<rocksdb::DB>&db, char c, int32_t x,
                        std::string &view) {
  auto key_data = pack_char_int(c, x);
  rocksdb::Slice slice(key_data.data(), 5);
  rocksdb::Status status =
    db->Get(
        ReadOptions(),
        key_data,
        &view
    );

  return status.ok();
}

#define DATABASE_NAME "./iblis.rocksdb"
#define OLD_DATABASE_NAME "./iblis.lmdb"
#include <filesystem>


#ifdef MIGRATE_LMDB
#include <lmdb.h>
#include <functional>
#include <string>
#include <stdexcept>
bool export_lmdb(
    const std::string& path,
    std::function<void(
        const std::string&,
        const std::string&)> callback)
{
    MDB_env* env = nullptr;
    MDB_txn* txn = nullptr;
    MDB_cursor* cursor = nullptr;
    MDB_dbi dbi;

    int rc = mdb_env_create(&env);
    if (rc != MDB_SUCCESS)
        return false;

    // Don't create if missing
    rc = mdb_env_open(
        env,
        path.c_str(),
        MDB_RDONLY,
        0);

    if (rc != MDB_SUCCESS) {
        mdb_env_close(env);

        // ENOENT => database doesn't exist
        if (rc == ENOENT)
            return false;

        throw std::runtime_error(
            mdb_strerror(rc));
    }

    rc = mdb_txn_begin(
        env,
        nullptr,
        MDB_RDONLY,
        &txn);

    if (rc != MDB_SUCCESS)
        goto cleanup;

    // Open main database
    rc = mdb_dbi_open(
        txn,
        nullptr,
        0,
        &dbi);

    if (rc != MDB_SUCCESS)
        goto cleanup;

    rc = mdb_cursor_open(
        txn,
        dbi,
        &cursor);

    if (rc != MDB_SUCCESS)
        goto cleanup;

    MDB_val key, value;

    while ((rc = mdb_cursor_get(
                cursor,
                &key,
                &value,
                MDB_NEXT)) == MDB_SUCCESS)
    {
        callback(
            std::string(
                static_cast<char*>(key.mv_data),
                key.mv_size),
            std::string(
                static_cast<char*>(value.mv_data),
                value.mv_size));
    }

cleanup:

    if (cursor)
        mdb_cursor_close(cursor);

    if (txn)
        mdb_txn_abort(txn);

    if (env)
        mdb_env_close(env);

    // MDB_NOTFOUND after iteration is normal
    return rc == MDB_NOTFOUND ||
           rc == MDB_SUCCESS;
}
#endif

void Configs::initialize_rocksdb(std::unique_ptr<rocksdb::DB>& db) {

  rocksdb::Status status =
    rocksdb::DB::Open(
        Options(),
        DATABASE_NAME,
        &db
  );

  if (!status.ok()) {
    std::cerr << status.ToString() << std::endl;
    return;
  }
  rocksdb::WriteBatch dbi; 
#ifdef MIGRATE_LMDB
  if (export_lmdb(OLD_DATABASE_NAME, [&dbi](std::string key, std::string value)->void{
    dbi.Put(key, value);
  })){
    db->Write(WriteOptions(), &dbi);
  } else {
    dbi.Clear();
  };
#endif


  {
    std::vector<char> types = {Proxies, Beans, Routes, Groups};
    for (char c : types) {
      auto ids = FileDatabaseManager::QueryFromDirectory(c);
      for (int x : ids) {
        dbi.Put(pack_char_int(c, x), "");
      }
    }
    std::vector<char> common_types = {
        Shortcuts,    ResourceManager, ProxyManager,  NekoBox,
        DefaultRoute, TrafficLooper,   DatabaseLogger};
    for (char c : common_types) {
      dbi.Put(pack_char_int(c, 0), "");
    }
    auto status =
    db->Write(
        WriteOptions(),
        &dbi
    );

    if (!status.ok()) {
      std::cerr << status.ToString() << std::endl;
      return;
    }
  }
  return;
}
#undef DATABASE_NAME

#endif

namespace Configs {
void ProfileManager::FillProfileEnts(QList<std::shared_ptr<Configs::ProxyEntity>> &list, const QList<int> & l){
  for (int i : l){
    auto profile =  Configs::profileManager->GetProfile(i);
    if (profile != nullptr){
      list << profile;
    }
  }
};

int ProfileManager::GetProfileLatency(int id){
  auto profile = this->GetProfile(id);
  if (profile == nullptr){
    return -1;
  }
  return profile->latencyInt;
}

ProfileManager *profileManager = new ProfileManager();

int ProfileManager::getGroupCount(){
  int proxies = 0;
  for (const auto & group: Configs::profileManager->groups){
    proxies += group.second->profiles.size();
  }
  return proxies;
}

ProfileManager::ProfileManager() : JsonStore() {}

DECL_MAP(ProfileManager)
ADD_MAP("groups", groupsTabOrder, integerList);
STOP_MAP

void ProfileManager::LoadManager() {
  JsonStore::Load();
  //
//  weak_profiles;
  groups = {};
  routes = {};
  auto profilesIdOrder =
      Configs::databaseManager->Query(Configs::JsonStoreType::Proxies);
  auto groupsIdOrder =
      Configs::databaseManager->Query(Configs::JsonStoreType::Groups);
  auto routesIdOrder =
      Configs::databaseManager->Query(Configs::JsonStoreType::Routes);
  // Load Proxys
  int max = 0;
#ifdef DEBUG_MODE
  qDebug() << "Profiles ID ORDER" << profilesIdOrder;
  qDebug() << "Groups ID ORDER" << groupsIdOrder;
  qDebug() << "Routes ID ORDER" << routesIdOrder;
#endif

  for (auto id : profilesIdOrder) {
#ifdef DEBUG_MODE
    qDebug() << "Load Profile With ID" << id;
#endif
//    auto ent = LoadProxyEntity(id);
    // Corrupted profile?
//    if (ent == nullptr || !ent->isValid() || ent->id != id) {
//      delProfile << id;
//      continue;
 //   }
    if (id > max) {
      max = id;
    }
 //   profiles[id] = ent;
 //   if (ent->type == "extracore")
 //     extraCorePaths.insert(ent->ExtraCoreBean()->extraCorePath);
  }
  // Clear Corrupted profile
  this->max_profile_id = max;
  max = 0;
  // Load Groups
  auto loadedOrder = groupsTabOrder;
  groupsTabOrder = {};
  for (auto id : groupsIdOrder) {
    auto ent = LoadGroup(id);
    // Corrupted group?
    if (ent->id != id) {
      continue;
    }
    if (id > max) {
      max = id;
    }
    // Ensure order contains every group
    if (!loadedOrder.contains(id)) {
      loadedOrder << id;
    }
    groups[id] = ent;
  }
  this->max_group_id = max;
  max = 0;
  /*
  QList<int> orphanProfiles;
  for (const auto [id, proxy] : (profiles)) {
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
    */
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
      if (MW_show_log != nullptr)
      MW_show_log(
          QString("Route Profile with id %d is corrupted, consider delete it")
              .arg(id));
      continue;
    }
    if (id > max) {
      max = id;
    }
    routes[id] = route;
  }
  this->max_route_chain_id = max;

  if (groups.size() == 0){
    auto defaultGroup = Configs::profileManager->NewGroup();
    defaultGroup->name = QObject::tr("Default");
    Configs::profileManager->AddGroup(defaultGroup);
  }
  if (routes.size() == 0){
    auto defaultRoute = Configs::RoutingChain::GetDefaultChain();
    Configs::profileManager->AddRouteChain(defaultRoute);
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
  }
  /*
  if (!ent->isValid()) {
#ifdef DEBUG_MODE
    qDebug() << "UNKNOWN TYPE:" << ent->type;
#endif
    return ent;
  }
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
  */
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
  if (type.isEmpty()) {
    return type;
  }
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

bool ProfileManager::AddProfile(const std::shared_ptr<ProxyEntity> &ent,
                                int gid) {
  QList<std::shared_ptr<ProxyEntity>> list;
  list << ent;
  return ProfileManager::AddProfileBatch(list, gid);
}

void ProfileManager::CacheProfile(std::shared_ptr<ProxyEntity> ent){
  cached_profiles.insert({ent->Id(), ent});
}

void ProfileManager::UncacheProfile(int id, bool force){
  cached_profiles.erase(id);
  if (force){
    weak_profiles_mutex.lock();
    weak_profiles.remove(id);
    weak_profiles_mutex.unlock();
  }
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
    int id = this->max_profile_id = (this->max_profile_id + 1);
#ifdef DEBUG_MODE
    qDebug() << "Profile Id Is: " << id;
#endif
    ent->gid = gid;
    ent->id = id;
    group->profiles.append(id);
    this->CacheProfile(ent);
  //  profiles[id] = ent;
  }

  runOnNewThread([=, this] {
    lock();
    group->Save();
    for (const auto &ent : ents) {
#ifdef DEBUG_MODE
      qDebug() << "Profile Id Is: Before Save: " << ent->id;
#endif
      ent->Save();
    }
    unlock();
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
  Configs::profileManager->FillProfileEnts(entsp, ents);
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
      lock();
      for (const auto &grp : grps)
        grp->Save();
      for (const auto &ent : ments)
        ent->Save();
      unlock();
    });
  }
  return true;
}

void ProfileManager::DeleteProfile(int id) {
  deleteProfile(id);
}

void ProfileManager::BatchDeleteProfiles(const QList<int> &ids, int groupid) {
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
    if (groupid != -999){
      if (groupid < 0) {
        if (auto group = GetGroup(ent->gid); group != nullptr) {
          group->RemoveProfile(id);
          changed_groups.insert(group);
        }
      } else {
        if (groupid != ent->gid ) {
          continue;
        }
      }
    }
    UncacheProfile(id, true);
    deleted_ids.insert(id);
  }

  runOnNewThread([=, this] {
    lock();
    for (const auto &group : changed_groups)
      group->Save();
    for (int id : deleted_ids) {
      Configs::databaseManager->Drop(Proxies, id);
      Configs::databaseManager->Drop(Beans, id);
    }
    unlock();
  });
}

void ProfileManager::unlock() { this->mutex.unlock(); }

void ProfileManager::lock() { this->mutex.lock(); }

void ProfileManager::deleteProfile(int id) {
  QList<int> ids = {id};
  BatchDeleteProfiles(ids);
}

std::shared_ptr<ProxyEntity> ProfileManager::GetProfile(int id) {
  if (cached_profiles.size() > 0){
    auto iter = cached_profiles.find(id);
    if (iter != cached_profiles.end()){
      return iter->second;
    }
  }
  std::shared_ptr<ProxyEntity> profile;
  weak_profiles_mutex.lock();
  auto iter = weak_profiles.tryGet(id, profile);
  weak_profiles_mutex.unlock();
  if (!iter){
    get_weak_profile:
    auto profile = this->LoadProxyEntity(id);
    if (profile->isValid()){
 //     std::weak_ptr<ProxyEntity> weak = profile;
      weak_profiles_mutex.lock();
      weak_profiles.insert(id, profile);
      weak_profiles_mutex.unlock();
      return profile;
    }
      #ifdef DEBUG_MODE
        qDebug() << "ATTENTION!!! Invalid profile id";
      #endif
    return nullptr;
  } else {
    if (profile == nullptr){
      goto get_weak_profile;
    }
    return profile;
  }
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

bool ProfileManager::AddGroup(const std::shared_ptr<Group> &ent){
  return AddGroupBatch({ent});
}

bool ProfileManager::AddGroupBatch(const QList<std::shared_ptr<Group>> &ents) {
  QList<std::shared_ptr<Group>> to_save;
  for (auto ent : ents){
    if (ent->id >= 0) {
      continue;
    }
    int id = this->max_group_id = (this->max_group_id + 1);
    ent->id = id;
    groups[ent->id] = ent;
    groupsTabOrder.push_back(id);

    to_save << ent;
  }
  bool ret = to_save.size() > 0;
  if (ret){
   runOnNewThread([=, this] {
      lock();
      for (auto gid : to_save){
       gid->Save();
      }
      unlock();
    });
  }
  return ret;
}

void ProfileManager::DeleteGroup(int gid) {
  BatchDeleteGroups({gid});
}


void ProfileManager::BatchDeleteGroups(const QList<int> & ids) {
  QSet<int> to_drop;
  for (int gid : ids){
    if (groups.size() <= 1)
      continue;
    auto group = GetGroup(gid);
    if (group == nullptr)
      continue;
    groupsTabOrder.removeAll(gid);
    to_drop << gid;
  }
  if (to_drop.size() > 0){
   runOnNewThread([=, this] {
      for (int gid : to_drop){
       auto group = GetGroup(gid);
       BatchDeleteProfiles(group->profiles, gid);
      }
      lock();
      for (int gid : to_drop){
       Configs::databaseManager->Drop(Groups, gid);
      }
      unlock();
    });
  }
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

bool ProfileManager::AddRouteChain(const std::shared_ptr<RoutingChain> &chain) {
  if (chain->id >= 0) {
    return false;
  }
  int id = this->max_route_chain_id = (this->max_route_chain_id + 1);
  chain->id = id;
  routes[chain->id] = chain;
  chain->Save();

  return true;
}

std::shared_ptr<RoutingChain> ProfileManager::GetRouteChain(int id) {
  return routes.count(id) > 0 ? routes[id] : nullptr;
}

void ProfileManager::UpdateRouteChains(
    const QList<std::shared_ptr<RoutingChain>> &newChain) {
  routes.clear();

  QList<std::shared_ptr<Configs::RoutingChain> > to_save;

  for (const auto &item : newChain) {
    if (!AddRouteChain(item)) {
      routes[item->id] = item;
      to_save << item;
    }
  }


  runOnNewThread([=, this] {
    lock();
    for (const auto &item: to_save){
      item->Save();
    }

    auto currFiles =
      Configs::databaseManager->Query(Configs::JsonStoreType::Routes);
    for (const auto &item : currFiles) { // clean up removed route profiles
      if (!routes.count(item)) {
        Configs::databaseManager->Drop(Routes, item);
      }
    }
    unlock();
  });

}
} // namespace Configs
