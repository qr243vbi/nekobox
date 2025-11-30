#include "include/sys/Settings.h"

#include "include/dataStore/ResourceEntity.hpp"
#include <QApplication>
#include <QDir>
#include <QTemporaryFile>
#include <QFontDatabase>
#include <qcontainerfwd.h>
#include <qsettings.h>

QSettings getSettings() {
  return QSettings(CONFIG_INI_PATH, QSettings::IniFormat);
}

QString getResourcesDir() {
  QString str = Configs::resourceManager->resources_path;
  if (str == "") {
    str = getRootResource("public");
  }
  return str;
};

QString getResource(QString str) {
  {
    auto link = Configs::resourceManager->getLink(str);
   // qDebug() << "Link for " << str << " is " << link;
    if (!link.isEmpty()) {
      return link;
    }
  }
  QString dir = getResourcesDir();
  dir += "/";
  dir += str;
  QFile file(dir);
  if (file.exists()) {
    return dir;
  } else {
    return getRootResource(str);
  }
}

QString getRootResource(QString str) {
  QString dir = QApplication::applicationDirPath();
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
    return QApplication::applicationFilePath();
  }
}
#else
QString getApplicationPath() { return QApplication::applicationFilePath(); }
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

QString getLocale(){
  auto settings = getSettings();
  return settings.value("language", QLocale().name()).toString();
}

void updateEmojiFont(){
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
    int fontId = QFontDatabase::addApplicationFont(getResource("emoji.ttf"));

    if (fontId >= 0)
    {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        QFontDatabase::setApplicationEmojiFontFamilies(fontFamilies);
    } else
    {
        qDebug() << "could not load noto font!";
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

void MainWindowTableSettings::Save(QSettings &settings){
  settings.setValue("manually_column_width", this->manually_column_width);
  QStringList list ;
  list << QString::number(this->column_width[0]);
  list << QString::number(this->column_width[1]);
  list << QString::number(this->column_width[2]);
  list << QString::number(this->column_width[3]);
  list << QString::number(this->column_width[4]);
  settings.setValue("column_width", list);
};
void MainWindowTableSettings::Load(QSettings &settings){
  this->manually_column_width = settings.value(
    "manually_column_width", false).toBool();
  QStringList list = settings.value("column_width").toStringList();
  for (int i = list.size(); i < 5; i ++){
    list << "-1";
  }
  this->column_width[0] = list[0].toInt();
  this->column_width[1] = list[1].toInt();
  this->column_width[2] = list[2].toInt();
  this->column_width[3] = list[3].toInt();
  this->column_width[4] = list[4].toInt();
};

namespace Configs{
  MainWindowTableSettings tableSettings;
}