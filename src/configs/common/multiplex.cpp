#include "include/configs/common/multiplex.h"

namespace Configs {
    bool TcpBrutal::ParseFromLink(const QString& link) { return false; }
    bool TcpBrutal::ParseFromJson(const QJsonObject& object) { return false; }
    QString TcpBrutal::ExportToLink() { return {}; }
    QJsonObject TcpBrutal::ExportToJson() { return {}; }
    BuildResult TcpBrutal::Build() { return {}; }

    bool Multiplex::ParseFromLink(const QString& link) { return false; }
    bool Multiplex::ParseFromJson(const QJsonObject& object) { return false; }
    QString Multiplex::ExportToLink() { return {}; }
    QJsonObject Multiplex::ExportToJson() { return {}; }
    BuildResult Multiplex::Build() { return {}; }
}


