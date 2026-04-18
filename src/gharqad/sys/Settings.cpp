#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/sys/Settings.h>
#include <nekobox/dataStore/ResourceEntity.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QTemporaryFile>
#include <QVariantMap>
#include <qcontainerfwd.h>
#include <qsettings.h>

QSettings Configs::WindowSettings::settings() {
  return QSettings(CONFIG_INI_PATH, QSettings::IniFormat);
}

void Configs::SettingsStore::Save(const QString & parent) {
  QSettings store = settings();
  for (auto val : _map()) {
    val->Save(store, this, parent);
  }
  store.sync();
}

void Configs::SettingsStore::Load(const QString & parent) {
  QSettings store = settings();
  for (auto val : _map()) {
    val->Load(store, this, parent);
  }
}

SETTINGS_PUT(int, Int)
SETTINGS_PUT(bool, Bool)
SETTINGS_PUT(QString, Str)
SETTINGS_PUT(char, Chr)
SETTINGS_PUT(QStringList, StrList)

INIT_LIST(WindowSettings)
ADD_LIST(theme)
ADD_LIST(no_symlinks)
ADD_LIST(font_family)
ADD_LIST(font_size)
ADD_LIST(logs_enabled)
ADD_LIST(ask_delete)
ADD_LIST(test_after_start)
ADD_LIST(startup_update)
ADD_LIST(max_log_line)
ADD_LIST(width)
ADD_LIST(height)
ADD_LIST(X)
ADD_LIST(Y)
ADD_LIST(maximized)
ADD_LIST(splitter_state)
ADD_LIST(auto_hide)
ADD_LIST(save_position)
ADD_LIST(save_geometry)
ADD_LIST(program_name)
ADD_LIST(auto_scroll_log)
ADD_LIST(text_under_buttons)
ADD_LIST(language)
ADD_LIST(first_start)
ADD_LIST(manually_column_width)
ADD_LIST(column_width)
END_LIST

#define KEY parent + name

SETTINGS_VALUE_LOAD(Bool) {
  *(bool *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(KEY, *(bool *)(void *)(ptr + (size_t)(void *)store))
          .toBool();
}
SETTINGS_VALUE_LOAD(Str) {
  *(QString *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(KEY, *(QString *)(void *)(ptr + (size_t)(void *)store))
          .toString();
}
SETTINGS_VALUE_LOAD(StrList) {
  *(QStringList *)(void *)(ptr + (size_t)(void *)store) =
      settings
          .value(KEY, *(QStringList *)(void *)(ptr + (size_t)(void *)store))
          .toStringList();
}
SETTINGS_VALUE_LOAD(Int) {
  *(int *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(KEY, *(int *)(void *)(ptr + (size_t)(void *)store))
          .toInt();
}
SETTINGS_VALUE_LOAD(Chr) {
  *(char *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(KEY, (bool)*(char *)(void *)(ptr + (size_t)(void *)store))
          .toBool();
}

SETTINGS_VALUE_SAVE(Bool) {
  settings.setValue(KEY, *(bool *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(Str) {
  settings.setValue(KEY, *(QString *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(StrList) {
  settings.setValue(KEY,
                    *(QStringList *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(Int) {
  settings.setValue(KEY, *(int *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(Chr) {
  char ch = *(char *)(void *)(ptr + (size_t)(void *)store);
  if (static_cast<bool>(ch) == true || static_cast<bool>(ch) == false) {
    settings.setValue(KEY, (bool)ch);
  }
}

QSettings getGlobal() {
  return QSettings(GLOBAL_INI_PATH, QSettings::IniFormat);
}

QString getResourcesDir() {
  QString str = Configs::resourceManager->resources_path;

  if (str == "") {
#ifdef NKR_DEFAULT_RESOURCE_DIRECTORY
    str = NKR_DEFAULT_RESOURCE_DIRECTORY;
#else
    str = getRootResource("public");
#endif
  }
  return str;
};

QString getResource(QString str, QStringList dirs) {
  {
    auto link = Configs::resourceManager->getLink(str);
    // qDebug() << "Link for " << str << " is " << link;
    if (!link.isEmpty()) {
      return link;
    }
  }
  for (QString dir : dirs){
    QString path = dir + "/" + str;
    QFile file(path);
    if (file.exists()){
      return path;
    }
  }
  QString dir = getResourcesDir();
  dir += "/";
  dir += str;
  QFile file(dir);
  if (file.exists()) {
    return dir;
  } else {
    str = getRootResource(str);
    QFile file2(str);
    if (file2.exists()) {
      return str;
    } else {
      return "";
    }
  }
}

QString getLangResource(const QString &locale){
  return getResource(locale + ".qm");
}

QString getRootResource(QString str) {
  QString dir = root_directory;
  dir += "/";
  dir += str;
  return dir;
}
#ifndef SKIP_UPDATER_BUTTON
QString getUpdaterPath() {
  return getResource(
#ifdef Q_OS_WIN
      "updater.exe"
#else
      "updater"
#endif
  );
}
#endif

#ifdef Q_OS_UNIX
#include <QProcessEnvironment>
QString getAppImage() {
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  if (env.contains("APPIMAGE") &&
      env.contains("NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE")) {
    QString appimage = env.value("APPIMAGE");
    QString appimageproof = env.value("NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE");
    QFile imageproof(getRootResource(appimageproof));
    if (imageproof.exists()) {
      QFile image(appimage);
      if (image.exists()) {
        return appimage;
      }
    }
  }
  return "";
}

bool isAppImage() { return getAppImage() != ""; }
QString getApplicationPath() {
  QString appimage = getAppImage();
  if (appimage != "") {
    return appimage;
  } else {
    return software_path;
  }
}
#else
QString getApplicationPath() { return software_path; }
#endif

QString getCorePath() {
  return getResource(  NKR_CORE_NAME );
    //       qDebug() << "Default Instead: " << core_path;
}

bool isFileAppendable(QString filePath) {
  QFile file(filePath);
  // Check if the file exists and is writable in append mode
  if (file.exists()) {
    if (file.open(QIODevice::Append)) {
      file.close();
      return true;
    }
  } else {
    if (file.open(QIODevice::WriteOnly)) {
      file.close();
      return true;
    }
  }
  return false;
}


QString getLocale() {
    QString locale;
    auto text = ReadFileText(getResource("locale.txt")).trimmed();
    #ifdef DEBUG_MODE
        qDebug() << "Locale.txt text is" << text;
    #endif
    int ind = text.indexOf(':');
    bool forced = false;
    if (ind < 0) {
        if (locale == ""){
            locale = text;
        }
    } else {
        auto last = text.sliced(ind+1);
        forced = (last.compare("force", Qt::CaseInsensitive) == 0);
        if (forced || (locale == "")){
            locale = text.sliced(0, ind);
        }
    }
    if (locale == ""){
        locale = QLocale().name();
    }
    #ifdef DEBUG_MODE
        qDebug() << "Locale is" << locale;
    #endif
  return forced ? (Configs::windowSettings->language = locale) : defStr(Configs::windowSettings->language, locale);
}

void updateEmojiFont() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  QString font = getResource("emoji.ttf");
  static int latestFont = -1;
#ifdef DEBUG_MODE
  qDebug() << "Font path is : " << font;
#endif
  int fontId = QFontDatabase::addApplicationFont(font);
  if (fontId >= 0) {
    if (latestFont >= 0) {
      QFontDatabase::removeApplicationFont(latestFont);
    }
    latestFont = fontId;
    QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    QFontDatabase::setApplicationEmojiFontFamilies(fontFamilies);
    QApplication::processEvents();

    QString font_family = Configs::windowSettings->font_family;
    int font_size = Configs::windowSettings->font_size;

    if (!font_family.isEmpty()) {
      auto font = qApp->font();
      font.setFamily(font_family);
      qApp->setFont(font);
    }
    if (font_size != 0) {
      auto font = qApp->font();
      font.setPointSize(font_size);
      qApp->setFont(font);
    }
  } else {
  }
#endif
}

bool isDirectoryWritable(QString dirPath) {
  // Ensure the directory exists first
  QDir dir(dirPath);
  if (!dir.exists()) {
    return false;
  }

  // Create a temporary file in that directory
  QTemporaryFile tempFile(dir.filePath("temp_XXXXXX.tmp"));
  if (tempFile.open()) {
    tempFile.close();
    // The file was created, so the directory is writable. Clean up.
    tempFile.remove();
    return true;
  } else {
    // Failed to create/open the file, directory is likely not writable
    return false;
  }
}

void Configs::MainWindowTableSettings::Save(
    std::shared_ptr<WindowSettings> settings) {
  settings->manually_column_width = this->manually_column_width;
  QStringList &list = settings->column_width;
  list.clear();
  list << QString::number(this->column_width[0]);
  list << QString::number(this->column_width[1]);
  list << QString::number(this->column_width[2]);
  list << QString::number(this->column_width[3]);
  list << QString::number(this->column_width[4]);
};
void Configs::MainWindowTableSettings::Load(
    std::shared_ptr<WindowSettings> settings) {
  this->manually_column_width = settings->manually_column_width;
  QStringList &list = settings->column_width;
  for (int i = list.size(); i < 5; i++) {
    list << "-1";
  }
  this->column_width[0] = list[0].toInt();
  this->column_width[1] = list[1].toInt();
  this->column_width[2] = list[2].toInt();
  this->column_width[3] = list[3].toInt();
  this->column_width[4] = list[4].toInt();
};

#define ADD(X, Y) { \
  std::shared_ptr<LanguageValue> val = std::make_shared<LanguageValue>();  \
  val->code = X;      \
  val->name = Y;      \
  val->index = index; \
  index++;            \
  langs.append(val);  \
}

#define ADDIF(X, Y) if (getLangResource(X) != "") ADD(X, Y)

QList<std::shared_ptr<LanguageValue>> &languageCodes() {
  static QList<std::shared_ptr<LanguageValue>> langs;
  static bool readed = false;
  if (!readed) {
    readed = true;
    int index = 0;
    ADD("", QObject::tr("System"))
    ADD("C", "English")
    QString path = getResource("languages.txt");
    if (path == "") {
      #ifdef DEBUG_MODE
      qDebug() << "languages.txt path not found";
      #endif
    } else {
      QString text = ReadFileText(path);
      QStringList list = text.split("\n");
      for (QString str : list) {
        str = str.trimmed();
        int ind = -1;
        if (!str.startsWith('#')){
          ind = str.indexOf(':');
        }
        if (ind > 0) {
          QString key = str;
          key = key.sliced(0, ind);
          QString value = str;
          value = value.sliced(ind + 1);
          #ifdef DEBUG_MODE
          qDebug() << "Language: " << key << value ;
          #endif
          if (!key.isEmpty() && !value.isEmpty()){
            ADDIF(key, value)
          }
        }
      }
    }
  }
  return langs;
}

namespace Configs {

std::shared_ptr<WindowSettings> windowSettings =
    std::make_shared<WindowSettings>();
MainWindowTableSettings tableSettings;
} // namespace Configs

