#include "include/dataStore/ResourceEntity.hpp"
#include "include/sys/Settings.h"
#include <QString>
#include <qcoreapplication.h>

static inline QString _ent(QString name) {
  name = name.replace("/", "_P");
#ifdef Q_OS_WIN
  name = name.replace("\\", "_p");
#endif
  name = name.replace("_", "__");
  return name;
}

namespace Configs {


ResourceManager::ResourceManager() : JsonStore("resources/manager.json") {
  _add(new configItem("core_path", &core_path, itemType::string));
  _add(new configItem("resources_path", &resources_path, itemType::string));
  symlinks_supported = true;
  load_control_must = true;
  this->Load();
}

bool ResourceManager::saveLink(QString str, QString path){
  str = _ent(str);
  if (symlinks_supported) {
    QString file("resources/" + str + ".ent.lnk");
    QFile::remove(file);
    return createSymlink(path, file);
  } else {
    QFile file("resources/" + str + ".ent.txt");
    file.remove();
    return WriteFileText(file, path);
  }
}

QString ResourceManager::getLink(QString str) {
  QString filepath = "";
  str = _ent(str);
  if (symlinks_supported) {
    QFile file("resources/" + str + ".ent.lnk");
    if (file.exists()) {
      filepath = file.symLinkTarget();
    }
  } else {
    QFile file("resources/" + str + ".ent.txt");
    if (file.exists()) {
      filepath = ReadFileText(file);
    }
  }
  QFileInfo info(filepath);
  if (info.exists()) {
    return info.absoluteFilePath();
  } else {
    return "";
  }

};

  ResourceManager  * resourceManager = new Configs::ResourceManager();
} // namespace Configs
