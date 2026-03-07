#include "nekobox/dataStore/ResourceEntity.hpp"
#include "nekobox/sys/Settings.h"
#include <QString>
#include <qcoreapplication.h>
#include "nekobox/configs/proxy/AbstractBean.hpp"

static inline QString _ent(QString name) {
  name = name.replace("/", "_P");
#ifdef Q_OS_WIN
  name = name.replace("\\", "_p");
#endif
  name = name.replace("_", "__");
  return name;
}

namespace Configs {

DECL_MAP(ResourceManager)
    ADD_MAP( "core_path", core_path, string);
    ADD_MAP( "resources_path", resources_path, string);
    ADD_MAP("latest_path", latest_path, string);
STOP_MAP

ResourceManager::ResourceManager() : JsonStore("resource_manager.cfg") {
  symlinks_supported = true;
  load_control_must = true;
  this->Load();
}

QString ResourceManager::getLatestPath(){
  QString latest = latest_path;
  if (latest == ""){
    ret_label:
    return QDir::currentPath();
  }
  QDir dir(latest);
  if (!dir.exists()){
    goto ret_label;
  }
  return latest;
}

bool ResourceManager::saveLink(QString str, QString path){
  str = _ent(str);
  latest_path = QFileInfo(path).absolutePath();
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
