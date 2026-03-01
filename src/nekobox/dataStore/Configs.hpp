#pragma once
#include "Const.hpp"
#include "Utils.hpp"
#include "ConfigItem.hpp"
#include "DataStore.hpp"

// Switch core support

namespace Configs {
// Source - https://stackoverflow.com/a
// Posted by Kuba hasn't forgotten Monica
// Retrieved 2026-01-13, License - CC BY-SA 3.0

    QByteArray hash(const QString & str);

    QString FindCoreRealPath();

    bool IsAdmin(bool forceRenew=false);

    bool isSetuidSet(const std::string& path);

    QString GetBasePath();
} // namespace Configs
