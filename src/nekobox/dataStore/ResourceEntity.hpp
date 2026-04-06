#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once
#include <nekobox/dataStore/ConfigItem.hpp>
#include <nekobox/dataStore/Configs.hpp>
#include <QDir>

#ifdef Q_OS_WIN
  #define NKR_CORE_NAME "nekobox_core.exe"
#else
  #define NKR_CORE_NAME "nekobox_core"
#endif

namespace Configs {

class ResourceManager : public JsonStore {
public:
    DECLARE_STORE_TYPE(ResourceManager)
  ResourceManager();
  QString core_path = "";
  QString resources_path = "";
  QString latest_path = "";
  QString getLink(QString str);
  bool saveLink(QString str, QString path);
  bool symlinks_supported;

  virtual ConfJsMap _map() override;
  QString getLatestPath();
};

extern class ResourceManager *resourceManager;
} // namespace Configs
