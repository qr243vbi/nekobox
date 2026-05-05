#include "nekobox/dataStore/ProfileFilter.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

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
        void AsyncUpdate(
            const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob, 
            const QString &str, const std::function<QString(bool*,bool*,const QString&)> &info, 
            int _sub_gid = -1, const std::function<void()> &finish = nullptr);

        void Update(const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob, const QString &_str, int _sub_gid = -1, bool _not_sub_as_url = false);

    signals:

        void asyncUpdateCallback(int gid);
    };

    extern GroupUpdater *groupUpdater;
} // namespace Subscription

void UI_update_all_groups(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob, 
    bool onlyAllowed, const std::function<QString(bool*, bool*,const QString&)> &info);
