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

ConfJsMap & ResourceManager::_map(){
  static ConfJsMap  ptr;
  static bool init = false;

    #ifndef  d_add
    #define  d_add(X, Y, B) _put(ptr, X, &this->Y, B)
    #endif

  if (init) return ptr;
  {
    d_add( "core_path", core_path, string);
    d_add( "resources_path", resources_path, string);
  }
  init = true;
  return ptr;
}

ResourceManager::ResourceManager() : JsonStore("resources/manager.json") {
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
