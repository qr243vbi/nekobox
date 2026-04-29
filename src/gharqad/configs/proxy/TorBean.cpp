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
}