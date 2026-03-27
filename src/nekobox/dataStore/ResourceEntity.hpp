#pragma once
#include "nekobox/dataStore/ConfigItem.hpp"
#include "nekobox/dataStore/Configs.hpp"
#include <QDir>

namespace Configs {

class ResourceManager : public JsonStore {
public:
    DECLARE_STORE_TYPE(ResourceManager)
  ResourceManager();
  QString core_path;
  QString resources_path;
  QString latest_path = "";
  QString getLink(QString str);
  bool saveLink(QString str, QString path);
  bool symlinks_supported;

  virtual ConfJsMap _map() override;
  QString getLatestPath();
};

extern class ResourceManager *resourceManager;
} // namespace Configs
