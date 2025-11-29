#pragma once
#include "include/global/Configs.hpp"
#include <QDir>

namespace Configs {

class ResourceManager : public JsonStore {
public:
  ResourceManager();
  QString core_path;
  QString resources_path;
  QString getLink(QString str);
  bool saveLink(QString str, QString path);
  bool symlinks_supported;
};

extern ResourceManager *resourceManager;
} // namespace Configs
