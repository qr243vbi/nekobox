#pragma once

#include <QJsonObject>
<<<<<<< HEAD
#include "include/global/Configs.hpp"

namespace Configs
{
    struct BuildResult {
        QJsonObject object;
        QString error;
    };

    class baseConfig : public JsonStore
    {
    public:
        virtual bool ParseFromLink(const QString& link) {
            return false;
        }

        virtual bool ParseFromJson(const QJsonObject& object) {
            return false;
        }

        virtual QString ExportToLink() {
            return {};
        }

        virtual QJsonObject ExportToJson() {
            return {};
        }

        virtual BuildResult Build() {
            return {{}, "base class function called!"};
        }
=======
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
>>>>>>> main
    };
}
