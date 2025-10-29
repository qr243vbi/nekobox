#pragma once

#include <QJsonObject>
#include <QString>
#include "include/global/ConfigItem.hpp"

namespace Configs
{
    class baseConfig : public JsonStore
    {
    public:
        virtual bool ParseFromLink(QString link);

        virtual bool ParseFromJson(QJsonObject object);

        virtual QString ExportToLink();

        virtual QJsonObject ExportToJson();

        virtual QJsonObject Build();
    };
}
