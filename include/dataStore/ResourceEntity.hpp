#pragma once
#include "include/global/Configs.hpp"
#include <QDir>

namespace Configs {

class ResourceEntity : public JsonStore {
public:
  QString name;
  QString path;
  QString summary;
  QString entity;
  bool Save() override;

private:
  ResourceEntity(QString str, bool symlinks_supported);
  bool symlinks_supported;

  friend class ResourceManager;
};

class ResourceManager : public JsonStore {
public:
  ResourceManager();
  QString core_path;
  QString resources_path;
  ResourceEntity getEntity(QString str);
  QString getLink(QString str);
  bool symlinks_supported;
};

extern ResourceManager *resourceManager;
} // namespace Configs
