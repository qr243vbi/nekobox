#ifndef NEKOBOX_SETTINGS
#define NEKOBOX_SETTINGS
#include <QList>
#include <QMap>
#include <QSettings>
#include <nekobox/dataStore/Utils.hpp>

#define CONFIG_INI_PATH QDir::current().absolutePath() + "/window.ini"

#define GLOBAL_INI_PATH getResource("global.ini")

#define SETTINGS_VALUE_LOAD(Bin)                                               \
  void Configs::Settings##Bin##Value::Load(QSettings &settings,                \
                                           SettingsStore *store)

#define SETTINGS_VALUE_SAVE(Bin)                                               \
  void Configs::Settings##Bin##Value::Save(QSettings &settings,                \
                                           SettingsStore *store)

#define SETTINGS_PUT(X, T)                                                     \
  void Configs::SettingsStore::_put(                                           \
      QList<std::shared_ptr<Configs::SettingsValue>> &list,                    \
      const QString &str, X *value) {                                          \
    std::shared_ptr<Configs::SettingsValue> val =                              \
        std::make_shared<Configs::Settings##T##Value>();                       \
    size_t ptr = (size_t)(void *)(value) - (size_t)(void *)(this);             \
    val->ptr = ptr;                                                            \
    val->name = str;                                                           \
    list << val;                                                               \
  }

#define ADD_LIST(X) _put(list, #X, &X);

#define INIT_LIST(X)                                                           \
  QList<std::shared_ptr<Configs::SettingsValue>> &Configs::X::_map() {         \
    static QList<std::shared_ptr<Configs::SettingsValue>> list;                \
    if (list.isEmpty()) {

#define END_LIST                                                               \
  }                                                                            \
  return list;                                                                 \
  }

#define SETTINGS_VALUE(Bin)                                                    \
  class Settings##Bin##Value : public SettingsValue {                          \
  public:                                                                      \
    void Load(QSettings &settings, SettingsStore *store) override;             \
    void Save(QSettings &settings, SettingsStore *store) override;             \
  };

namespace Configs {
class SettingsValue;
class SettingsStore {
public:
  virtual QList<std::shared_ptr<SettingsValue>> &_map() = 0;
  virtual QSettings settings() = 0;
  virtual void Load();
  virtual void Save();
  void _put(QList<std::shared_ptr<SettingsValue>> &list, const QString &str,
            int *ptr);
  void _put(QList<std::shared_ptr<SettingsValue>> &list, const QString &str,
            char *ptr);
  void _put(QList<std::shared_ptr<SettingsValue>> &list, const QString &str,
            bool *ptr);
  void _put(QList<std::shared_ptr<SettingsValue>> &list, const QString &str,
            QString *ptr);
  void _put(QList<std::shared_ptr<SettingsValue>> &list, const QString &str,
            QStringList *ptr);
};

class SettingsValue {
public:
  size_t ptr;
  QString name;
  virtual void Load(QSettings &settings, SettingsStore *store) = 0;
  virtual void Save(QSettings &settings, SettingsStore *store) = 0;
};

SETTINGS_VALUE(Bool)
SETTINGS_VALUE(Str)
SETTINGS_VALUE(Int)
SETTINGS_VALUE(StrList)
SETTINGS_VALUE(Chr)

#undef SETTINGS_VALUE

class WindowSettings : public SettingsStore {
public:
  QList<std::shared_ptr<SettingsValue>> &_map() override;
  QSettings settings() override;
  QString theme =
#ifdef Q_OS_WIN
      "System"
#else
      "Fusion"
#endif
      ;
  QString font_family =
#ifdef Q_OS_WIN
      ""
#else
      "Noto Sans"
#endif
      ;
  int font_size =
#ifdef Q_OS_WIN
      0
#else
      11
#endif
      ;
  bool logs_enabled = true;
  bool test_after_start = true;
  char startup_update = false;
  int max_log_line = 200;
  int width = 0;
  int height = 0;
  int X = 0;
  int Y = 0;
  bool maximized = false;
  QString splitter_state = "";
  bool auto_hide = false;
  bool save_geometry = true;
  bool save_position = true;
  bool text_under_buttons = true;
  bool manually_column_width = false;
  QStringList column_width;
  QString language = "";
};

}; // namespace Configs

void updateEmojiFont();

bool createSymlink(const QString &targetPath, const QString &linkPath);
// QSettings getSettings();
QSettings getGlobal();
QString getLocale();
QString getResourcesDir();
QString getResource(QString, QStringList dirs = {});
#ifndef SKIP_UPDATER_BUTTON
QString getUpdaterPath();
#endif
#ifdef Q_OS_UNIX
bool isAppImage();
#endif
QString getRootResource(QString str);
QString getCorePath();
QString getApplicationPath();
bool isFileAppendable(QString filePath);
bool isDirectoryWritable(QString filePath);

namespace Configs {
struct MainWindowTableSettings {
  bool manually_column_width = false;
  int column_width[5];
  void Save(std::shared_ptr<WindowSettings> settings);
  void Load(std::shared_ptr<WindowSettings> settings);
};

extern std::shared_ptr<WindowSettings> windowSettings;
extern MainWindowTableSettings tableSettings;
} // namespace Configs

struct LanguageValue{
      QString code;
      QString name;
      int index;
};

QList<std::shared_ptr<LanguageValue>> &languageCodes();

#endif
