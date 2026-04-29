#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    

    CoreObjOutboundBuildResult SSHBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound{
            {"type", "ssh"},
            {"server", entity->serverAddress},
            {"server_port", entity->serverPort},
            {"user", user},
            {"password", password},
        };
        add_non_empty(outbound, "private_key", privateKey);
        add_non_empty(outbound, "private_key_path", privateKeyPath);
        add_non_empty(outbound, "private_key_passphrase", privateKeyPass);
        if (!hostKey.isEmpty()) outbound["host_key"] = QListStr2QJsonArray(hostKey);
        if (!hostKeyAlgs.isEmpty()) outbound["host_key_algorithms"] = QListStr2QJsonArray(hostKeyAlgs);
        add_non_empty(outbound, "client_version", clientVersion);

        result.outbound = outbound;
        return result;
    }

}