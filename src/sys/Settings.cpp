#include "include/sys/Settings.h"

#include "include/dataStore/ResourceEntity.hpp"
#include <QApplication>
#include <QDir>
#include <QTemporaryFile>

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

#ifdef Q_OS_LINUX
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
