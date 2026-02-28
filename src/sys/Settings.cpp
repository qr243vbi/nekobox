#include "nekobox/sys/Settings.h"

#include "nekobox/dataStore/ResourceEntity.hpp"
#include "nekobox/dataStore/Utils.hpp"
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

void Configs::SettingsStore::Save() {
  QSettings store = settings();
  for (auto val : _map()) {
    val->Save(store, this);
  }
  store.sync();
}

void Configs::SettingsStore::Load() {
  QSettings store = settings();
  for (auto val : _map()) {
    val->Load(store, this);
  }
}

SETTINGS_PUT(int, Int)
SETTINGS_PUT(bool, Bool)
SETTINGS_PUT(QString, Str)
SETTINGS_PUT(char, Chr)
SETTINGS_PUT(QStringList, StrList)

INIT_LIST(WindowSettings)
ADD_LIST(theme)
ADD_LIST(font_family)
ADD_LIST(font_size)
ADD_LIST(logs_enabled)
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
ADD_LIST(text_under_buttons)
ADD_LIST(language)
ADD_LIST(manually_column_width)
ADD_LIST(column_width)
END_LIST

SETTINGS_VALUE_LOAD(Bool) {
  *(bool *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(name, *(bool *)(void *)(ptr + (size_t)(void *)store))
          .toBool();
}
SETTINGS_VALUE_LOAD(Str) {
  *(QString *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(name, *(QString *)(void *)(ptr + (size_t)(void *)store))
          .toString();
}
SETTINGS_VALUE_LOAD(StrList) {
  *(QStringList *)(void *)(ptr + (size_t)(void *)store) =
      settings
          .value(name, *(QStringList *)(void *)(ptr + (size_t)(void *)store))
          .toStringList();
}
SETTINGS_VALUE_LOAD(Int) {
  *(int *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(name, *(int *)(void *)(ptr + (size_t)(void *)store))
          .toInt();
}
SETTINGS_VALUE_LOAD(Chr) {
  *(char *)(void *)(ptr + (size_t)(void *)store) =
      settings.value(name, (bool)*(char *)(void *)(ptr + (size_t)(void *)store))
          .toBool();
}

SETTINGS_VALUE_SAVE(Bool) {
  settings.setValue(name, *(bool *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(Str) {
  settings.setValue(name, *(QString *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(StrList) {
  settings.setValue(name,
                    *(QStringList *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(Int) {
  settings.setValue(name, *(int *)(void *)(ptr + (size_t)(void *)store));
}
SETTINGS_VALUE_SAVE(Chr) {
  char ch = *(char *)(void *)(ptr + (size_t)(void *)store);
  if (ch == true || ch == false) {
    settings.setValue(name, (bool)ch);
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
  QString core_path = Configs::resourceManager->core_path;
  //   qDebug() << "Got Core Path From Settings: " << core_path;
  if (core_path == "") {
    core_path = getRootResource(
#ifdef Q_OS_WIN
        "nekobox_core.exe"
#else
        "nekobox_core"
#endif
    );
    //       qDebug() << "Default Instead: " << core_path;
  }
  return core_path;
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

bool createSymlink(const QString &targetPath, const QString &linkPath) {
  if (QFile::link(targetPath, linkPath)) {
    return true;
  } else {
    return false;
  }
}

QString getLocale() {
  return defStr(Configs::windowSettings->language, QLocale().name());
}

void updateEmojiFont() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  QString font = getResource("emoji.ttf");
  static int latestFont = -1;

  qDebug() << "Font path is : " << font;
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

/*
    "",
    "C",
    "he_IL",
    "zh_CN",
    "fa_IR",
    "ru_RU"
*/

#define ADD(X, Y) if (getLangResource(X) == ""){ \
  std::shared_ptr<LanguageValue> val = std::make_shared<LanguageValue>();  \
  val->code = X;      \
  val->name = Y;      \
  val->index = index; \
  index++;            \
  langs.append(val);  \
}

QList<std::shared_ptr<LanguageValue>> &languageCodes() {
  static QList<std::shared_ptr<LanguageValue>> langs;
  static bool readed = false;
  if (!readed) {
    readed = true;
    int index = 0;
    ADD("", QObject::tr("System"))
    QString path = getResource("languages.txt");
    if (path == "") {
      ADD("C", "English")
      ADD("zh_CN", "简体中文")
      ADD("he_IL", "עברית")
      ADD("fa_IR", "فارسی")
      ADD("ru_RU", "Русский")
    } else {
      QString text = ReadFileText(path);
      QStringList list = text.split("\n");
      for (QString str : list) {
        int ind = str.indexOf(':');
        if (ind > 0) {
          QString key = str;
          key.slice(0, ind);
          QString value = str;
          value.slice(ind + 1);
          if (!key.isEmpty() && !value.isEmpty()){
            ADD(key, value)
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

