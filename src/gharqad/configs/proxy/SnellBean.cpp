
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
/*
    bool SnellBean::TryParseLink(const QString& link)
    {
        using namespace From_Link;
        return true;
    }
*/
    CoreObjOutboundBuildResult SnellBean::BuildCoreObjSingBox() const
    {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;
        QJsonObject outbound = {
            {"psk", this->psk},
            {"reuse", this->reuse},
            {"version", this->version}
        };

        add_network(outbound, this);
        add_default_fields(outbound, this);

        if (this->obfs_mode->value > 0){
            outbound["obfs_mode"] = this->obfs_mode->toString();
            outbound["obfs_host"] = this->obfs_host;
        }

        result.outbound = outbound;
        return result;
    }

/*
    QString SnellBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        QUrlQuery q;
        return url.toString(QUrl::FullyEncoded);
    }
*/
    bool SnellBean::TryParseJson(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        add_network(this, obj);
        this->psk = obj["psk"].toString();
        this->version = obj["version"].toInt();
        this->reuse = obj["reuse"].toBool();
        *this->obfs_mode = obj["obfs_mode"].toString();
        this->obfs_host = obj["obfs_host"].toString();
        return true;
    }
}