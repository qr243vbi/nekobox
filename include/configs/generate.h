#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <include/dataStore/ProxyEntity.hpp>
#include <include/stats/traffic/TrafficData.hpp>

namespace Configs
{
    enum OSType
    {
        Unknown = 0,
        Linux = 1,
        Windows = 2,
        Darwin = 3,
    };

    class ExtraCoreData
    {
        public:
        QString path;
        QString args;
        QString config;
        QString configDir;
        bool noLog;
    };

    class DNSDeps
    {
        public:
        bool needDirectDnsRules = false;
        QJsonArray directDomains;
        QJsonArray directRuleSets;
        QJsonArray directSuffixes;
        QJsonArray directKeywords;
        QJsonArray directRegexes;
    };

    class HijackDeps
    {
        public:
        QJsonArray hijackDomains;
        QJsonArray hijackDomainSuffix;
        QJsonArray hijackDomainRegex;
        QJsonArray hijackGeoAssets;
    };

    class TunDeps
    {
        public:
        QJsonArray directIPSets;
        QJsonArray directIPCIDRs;
    };

    class RoutingDeps
    {
        public:
        int defaultOutboundID;
        QList<int> neededOutbounds;
        QStringList neededRuleSets;
        std::map<int, QString> outboundMap;
    };

    class BuildPrerequisities
    {
        public:
        std::shared_ptr<ExtraCoreData> extraCoreData = std::make_shared<ExtraCoreData>();
        std::shared_ptr<DNSDeps> dnsDeps = std::make_shared<DNSDeps>();
        std::shared_ptr<HijackDeps> hijackDeps = std::make_shared<HijackDeps>();
        std::shared_ptr<TunDeps> tunDeps = std::make_shared<TunDeps>();
        std::shared_ptr<RoutingDeps> routingDeps = std::make_shared<RoutingDeps>();
    };

    class BuildConfigResult {
    public:
        QString error;
        QJsonObject coreConfig;
        std::shared_ptr<ExtraCoreData> extraCoreData;

        QList<std::shared_ptr<Stats::TrafficData>> outboundStats;
    };

    class BuildSingBoxConfigContext
    {
        public:
        bool forTest = false;
        bool forExport = false;
        bool tunEnabled = false;
        bool isResolvedUsed = false;
        std::shared_ptr<ProxyEntity> ent = std::make_shared<ProxyEntity>(nullptr, nullptr, nullptr);
        std::shared_ptr<BuildPrerequisities> buildPrerequisities = std::make_shared<BuildPrerequisities>();
        OSType os;

        QString error;
        QStringList warnings;
        QJsonArray outbounds;
        QJsonArray endpoints;
        std::shared_ptr<BuildConfigResult> buildConfigResult = std::make_shared<BuildConfigResult>();
    };

    inline QString get_jsdelivr_link(QString link)
    {
        if(dataStore->routing->ruleset_mirror == Mirrors::GITHUB)
            return link;
        if(auto url = QUrl(link); url.isValid() && url.host() == "raw.githubusercontent.com")
        {
            QStringList list = url.path().split('/');
            QString result;
            switch(dataStore->routing->ruleset_mirror) {
            case Mirrors::GCORE: result = "https://gcore.jsdelivr.net/gh"; break;
            case Mirrors::QUANTIL: result = "https://quantil.jsdelivr.net/gh"; break;
            case Mirrors::FASTLY: result = "https://fastly.jsdelivr.net/gh"; break;
            case Mirrors::CDN: result = "https://cdn.jsdelivr.net/gh"; break;
            default: result = "https://testingcf.jsdelivr.net/gh";
            }

            int index = 0;
            foreach(QString item, list)
            {
                if(!item.isEmpty())
                {
                    if(index == 2)
                        result += "@" + item;
                    else
                        result += "/" + item;
                    index++;
                }
            }
            return result;
        }
        return link;
    }

    void CalculatePrerequisities(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    void buildLogSections(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    void buildDNSSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx, bool useDnsObj = true);

    void buildNTPSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    void buildCertificateSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    void buildInboundSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    void buildOutboundsSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    void buildRouteSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    void buildExperimentalSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx);

    std::shared_ptr<BuildConfigResult> BuildSingBoxConfig(const std::shared_ptr<ProxyEntity> &ent);

    class BuildTestConfigResult {
    public:
        QString error;
        QMap<int, QString> fullConfigs;
        QMap<QString, int> tag2entID;
        QJsonObject coreConfig;
        QStringList outboundTags;
    };

    bool IsValid(const std::shared_ptr<ProxyEntity> &ent);

    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<ProxyEntity> > &profiles);
}
