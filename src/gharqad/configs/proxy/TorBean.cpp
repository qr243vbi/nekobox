#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {


    CoreObjOutboundBuildResult TorBean::BuildCoreObjSingBox() const
    {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;
        QString path = this->executable_path;
        if (!path.isEmpty() || !QFile::exists(path)){
            path = QStandardPaths::findExecutable("tor");
        }

        QJsonObject outbound {
            {"type", this->type()},
            {"executable_path", path},
            {"extra_args", QListStr2QJsonArray(this->extra_args)},
            {"data_directory", this->data_directory},
            {"torrc", QJsonObject::fromVariantMap(this->torrc)}
        };
        result.outbound = outbound;
        return result;
    }

    QString TorBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("tor");
        url.setHost("tor");
        QUrlQuery q;
        add_query_args_nonempty( "extra_args", q, extra_args);
        add_query_nonempty("executable_path", q, executable_path);
        add_query_nonempty("data_directory", q, data_directory);
        add_query_map_nonempty("torrc", q, torrc);

        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }
}