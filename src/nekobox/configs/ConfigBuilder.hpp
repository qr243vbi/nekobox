#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/sys/Process.hpp>

#include <map>
#include <string>
extern  QVariantMap ruleSetMap;

namespace Configs {

    class ExtraCoreData
    {
    public:
        QString path;
        QString args;
        QString config;
        QString configDir;
        bool noLog;
    };

    class BuildConfigResult {
    public:
        QString error;
        QJsonObject coreConfig;
        std::shared_ptr<ExtraCoreData> extraCoreData;

        QList<std::shared_ptr<Stats::TrafficData>> outboundStats; // all, but not including "bypass" "block"
    };

    class BuildTestConfigResult {
    public:
        QString error;
        QMap<int, QString> fullConfigs;
        QMap<QString, int> tag2entID;
        QJsonObject coreConfig;
        QStringList outboundTags;
    };

    class BuildConfigStatus {
    public:
        std::shared_ptr<BuildConfigResult> result;
        std::shared_ptr<ProxyEntity> ent;
        int chainID = 0;
        bool forTest;
        bool forExport;

        // xxList is V2Ray format string list

        QStringList domainListDNSDirect;

        // config format

    //    QJsonArray routingRules;
        QJsonArray inbounds;
        QJsonArray outbounds;
        QJsonArray endpoints;
    };


    bool IsValid(const std::shared_ptr<ProxyEntity> &ent);

    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<ProxyEntity>>& profiles);

    std::shared_ptr<BuildConfigResult> BuildConfig(const std::shared_ptr<ProxyEntity> &ent, bool forTest, bool forExport, int chainID = 0);

    void BuildConfigSingBox(const std::shared_ptr<BuildConfigStatus> &status);

    QJsonObject BuildDnsObject(QString address, bool tunEnabled);

    QString BuildChain(int chainId, const std::shared_ptr<BuildConfigStatus> &status);

    QString BuildChainInternal(int chainId, const QList<std::shared_ptr<ProxyEntity>> &ents,
                               const std::shared_ptr<BuildConfigStatus> &status, int route_suffix = -1);

    QList<std::shared_ptr<ProxyEntity>> ResolveChainInternal(const std::shared_ptr<BuildConfigStatus> &status, const std::shared_ptr<ProxyEntity> &ent);


    void BuildOutbound(const std::shared_ptr<ProxyEntity> &ent, const std::shared_ptr<BuildConfigStatus> &status, QJsonObject& outbound, const QString& tag);

    QString getTunName();
    QString getTunAddress();
    QString getTunAddress6();

    QString get_jsdelivr_link(QString link);

    QJsonObject BuildTunInbound(const QStringList &directIPSets, const QStringList &directIPCIDRs);

} // namespace Configs
