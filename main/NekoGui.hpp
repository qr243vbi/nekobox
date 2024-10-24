#pragma once

#include "Const.hpp"
#include "NekoGui_Utils.hpp"
#include "NekoGui_ConfigItem.hpp"
#include "NekoGui_DataStore.hpp"

// Switch core support

namespace NekoGui {
    inline int coreType = CoreType::SING_BOX;

    QString FindCoreAsset(const QString &name);

    QString FindNekoBoxCoreRealPath();

    bool IsAdmin();

    QString GetBasePath();
} // namespace NekoGui

#define IS_NEKO_BOX (NekoGui::coreType == NekoGui::CoreType::SING_BOX)
#define ROUTES_PREFIX_NAME QString("route_profiles")
#define ROUTES_PREFIX QString(ROUTES_PREFIX_NAME + "/")
#define RULE_SETS_DIR QString("rule_sets")
