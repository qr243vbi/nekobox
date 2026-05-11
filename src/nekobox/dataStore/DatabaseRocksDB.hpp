
#ifndef DATABASE_LMDB
#define DATABASE_LMDB
#include <QList>

namespace Configs_ConfigItem {
    class JsonStore;
}

using namespace Configs_ConfigItem;

#ifndef SKIP_LEVELDB
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
namespace rocksdb = leveldb;

namespace Configs {
  std::string pack_char_int(char c, int32_t x);
  void initialize_rocksdb               (std::unique_ptr<rocksdb::DB>&db);
  bool drop_rocksdb                     (std::unique_ptr<rocksdb::DB>& env, char c, int32_t x);
  bool clear_rocksdb                    (std::unique_ptr<rocksdb::DB>& env, char c, int32_t x);
  bool clear_rocksdb                    (std::unique_ptr<rocksdb::DB>& env, JsonStore * store);
  bool write_rocksdb                    (std::unique_ptr<rocksdb::DB>& env, char c, int32_t x, const std::string &view);
  bool write_rocksdb                    (std::unique_ptr<rocksdb::DB>& env, JsonStore * store);
  QList<int> query_rocksdb              (std::unique_ptr<rocksdb::DB>& env, char c);
  bool read_rocksdb                     (std::unique_ptr<rocksdb::DB>& env, char c, int32_t x, std::string & view);
  std::tuple<bool, bool> read_rocksdb   (std::unique_ptr<rocksdb::DB>& env, JsonStore * store);
  std::tuple<char, int32_t> unpack_char_int(const std::string& key);
}
#endif



namespace Configs {
class DatabaseManager {
public:
  virtual bool Save(JsonStore *store) = 0;
  virtual bool Load(JsonStore *store) = 0;
  virtual bool Drop(char type, int id) = 0;
  virtual QList<int> Query(char type) = 0;
};



class FileDatabaseManager : public DatabaseManager {
public:
  FileDatabaseManager();
  ~FileDatabaseManager();
  virtual bool Save(JsonStore *) override;
  virtual bool Load(JsonStore *) override;
  virtual bool Drop(char, int) override;
  virtual QList<int> Query(char type) override;

  static bool SaveToFile(JsonStore *);
  static bool LoadFromFile(JsonStore *);
  static bool DropFromDirectory(char, int);
  static QList<int> QueryFromDirectory(char type);
private:
#ifndef SKIP_LEVELDB
  std::unique_ptr<rocksdb::DB> database;
#endif
};

  extern std::shared_ptr<DatabaseManager> databaseManager;

} // namespace Configs
#endif
