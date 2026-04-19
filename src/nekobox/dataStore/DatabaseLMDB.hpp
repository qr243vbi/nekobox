
#ifndef DATABASE_LMDB
#define DATABASE_LMDB
#include <QList>

namespace Configs_ConfigItem {
    class JsonStore;
}

using namespace Configs_ConfigItem;

#ifndef SKIP_LMDB
#include <3rdparty/lmdbxx/include/lmdbxx/lmdb++.h>

namespace Configs {
  std::string pack_char_int(char c, int32_t x);
  lmdb::env initialize_lmdb();
  bool drop_lmdb(lmdb::env& env, char c, int32_t x);
  bool clear_lmdb(lmdb::env& env, char c, int32_t x);
  bool clear_lmdb(lmdb::env& env, JsonStore * store);
  bool write_lmdb(lmdb::env& env, char c, int32_t x, const std::string_view &view);
  bool write_lmdb(lmdb::env& env, JsonStore * store);
  QList<int> query_lmdb(lmdb::env &env, char c);
  bool read_lmdb(lmdb::env& env, char c, int32_t x, std::string_view & view);
  std::tuple<bool, bool> read_lmdb(lmdb::env& env, JsonStore * store);
  std::tuple<char, int32_t> unpack_char_int(const std::string_view& key);
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
#ifndef SKIP_LMDB
  lmdb::env database;
#endif
};

  extern std::shared_ptr<DatabaseManager> databaseManager;

} // namespace Configs
#endif