#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ResourceEntity.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/sys/Settings.h>
#include <QString>
#include <qcoreapplication.h>
#include <nekobox/configs/proxy/AbstractBean.hpp>

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
    ADD_MAP("core_path", core_path, string);
    ADD_MAP("resources_path", resources_path, string);
    ADD_MAP("latest_path", latest_path, string);
STOP_MAP

ResourceManager::ResourceManager() : JsonStore() {
  symlinks_supported = true;
  //load_control_must = true;
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
  latest_path = QFileInfo(path).absoluteFilePath();
  QDir this_path (Configs::GetBasePath());

  QString relative = this_path.relativeFilePath(latest_path);
  if (!relative.startsWith("..")){
    latest_path = relative;
    if (symlinks_supported){
      latest_path = QString("..") + QDir::separator() + latest_path;
    }
  }  
  if (str == NKR_CORE_NAME){
    this->core_path = latest_path;
    return this->Save();
  } else {
    if (symlinks_supported) {
      QString file(this_path.absoluteFilePath(QString("resources") + QDir::separator() + str + ".ent.lnk"));
      QFile::remove(file);
      if (path == "") return true;
      return createSymlink(latest_path, file);
    } else {
      QFile file(this_path.absoluteFilePath(QString("resources") + QDir::separator() + str + ".ent.txt"));
      if (path == "") {
        file.remove();
        return true;
      }
      return WriteFileText(file, latest_path);
    }
  }
}

QString ResourceManager::getLink(QString str) {
  QString filepath = "";
  str = _ent(str);
  if (str == NKR_CORE_NAME){
    filepath = this->core_path;
  } else {
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
  }
  QFileInfo info(filepath);
  if (info.exists()) {
    return info.absoluteFilePath();
  } else {
    return "";
  }

};

  extern class ResourceManager  * resourceManager;
} // namespace Configs
