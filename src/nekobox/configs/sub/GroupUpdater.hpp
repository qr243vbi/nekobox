#pragma once

#include <nekobox/dataStore/ProfileFilter.hpp>
#include <nekobox/dataStore/Database.hpp>
#include <QJsonValue>
#include <QObject>


namespace Subscription {
    class RawUpdater {
    public:
        void updateSIP008(const QJsonObject& str);
        
        bool updateClash(const QString &str);

        void update(const QString &str);

        void updateSingBox(const QJsonObject &str);

        bool updateWireguardFileConfig(const QString &str);

        int gid_add_to = -1;

        QMap<Configs::ProfileFilterKey, bool> ignore_map;

        QList<std::shared_ptr<Configs::ProxyEntity>> proxies;

        bool AddProxy(std::shared_ptr<Configs::ProxyEntity>);
    };

    class GroupUpdater : public QObject {
        Q_OBJECT
    public:
        void AsyncUpdateGroup(
            std::shared_ptr<Configs::Group> group,
            const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
            const std::function<QString(bool*,bool*,const QString&)> &info,
            const std::function<void()> &finish = nullptr,
            const std::function<std::shared_ptr<const Configs::GroupExtra>(
                    std::shared_ptr<const Configs::GroupExtra>)> &updater = nullptr
        );

        void AsyncUpdate(
            const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob, 
            const QString &str,
            const std::function<QString(bool*,bool*,const QString&)> &info,
            int _sub_gid = -1,
            const std::function<void()> &finish = nullptr,
            const QMap<QString, QString> &headers = {},
            const QByteArray & array = {},
            bool no_hwid = false
        );

        void Update(
            const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
            const QString &_str,
            int _sub_gid = -1,
            bool _not_sub_as_url = false,
            bool _auto_name = false,
            const QMap<QString, QString> &headers = {},
            const QByteArray & array = {},
            bool no_hwid = false
        );

    signals:

        void asyncUpdateCallback(int gid);
    };

    extern GroupUpdater *groupUpdater;
} // namespace Subscription

void UI_update_all_groups(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob, 
    bool onlyAllowed, const std::function<QString(bool*, bool*,const QString&)> &info,
    const std::function<std::shared_ptr<const Configs::GroupExtra>(
            std::shared_ptr<const Configs::GroupExtra>)> &updater
        );
