#include "include/dataStore/ResourceEntity.hpp"
#include "include/sys/Settings.h"
#include <QString>
#include <qcoreapplication.h>

static inline QString _ent(QString name) {
  name = name.replace("/", "_P");
#if Q_OS_WIN
  name = name.replace("\\", "_p");
#endif
  name = name.replace("_", "__");
  return name;
}

namespace Configs {
ResourceEntity::ResourceEntity(QString fileent, bool sym)
    : JsonStore("resources/" + fileent + ".ent.json") {
  this->symlinks_supported = sym;
  _add(new configItem("name", &name, itemType::string));
  _add(new configItem("path", &path, itemType::string));
  _add(new configItem("summary", &summary, itemType::string));
  if (!this->Load()) {
    name = fileent.replace("_p", "\\").replace("__", "_").replace("_P", "/");
    summary = "";
    path = "";
    entity = fileent;
  };
  this->Load();
}

ResourceManager::ResourceManager() : JsonStore("resources/manager.json") {
  _add(new configItem("core_path", &core_path, itemType::string));
  _add(new configItem("resources_path", &resources_path, itemType::string));
  QFileInfo fileName("configs.json");
  QString linkName = "resources/nekobox.lnk.lnk";
  if (createSymlink(fileName.absoluteFilePath(), linkName)) {
    symlinks_supported = true;
    QFile f(linkName);
    f.remove();
  } else {
    symlinks_supported = false;
  };
  load_control_must = true;
  this->Load();
}

ResourceEntity ResourceManager::getEntity(QString str) {
 return ResourceEntity(_ent(str), this->symlinks_supported);
};

bool ResourceEntity::Save(){
  if (symlinks_supported) {
    QString val = this->path;
    QString linkpath = "resources/" + entity + ".ent.lnk";
    QFile::remove(linkpath);
    createSymlink(val, linkpath);
  }  
  return JsonStore::Save();
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
    ResourceEntity entity(str, this->symlinks_supported);
    if (!entity.path.isEmpty()) {
      filepath = entity.path;
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