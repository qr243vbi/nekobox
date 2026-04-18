#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once
#include "ProxyEntity.hpp"
#include "Group.hpp"
#include "RouteEntity.h"
#include "TrafficData.hpp"

namespace Stats {
    class DatabaseLoggerItem : public JsonStore {
        public:
        int created;
        int deleted;
        DECLARE_STORE_TYPE(NoSave)
        NEW_MAP
            ADD_MAP("new", created, jsonStore);
            ADD_MAP("del", deleted, jsonStore);
        STOP_MAP
    };



    class DatabaseLogger: public JsonStore {
        public:
        int start_count = 0;
        long long first_launch_time = 0;
        long long last_launch_time = 0;

        std::shared_ptr<DatabaseLoggerItem> profiles = std::make_shared<DatabaseLoggerItem>();
        std::shared_ptr<DatabaseLoggerItem> groups = std::make_shared<DatabaseLoggerItem>();
        std::shared_ptr<DatabaseLoggerItem> routes = std::make_shared<DatabaseLoggerItem>();
        
        std::shared_ptr<TrafficData> total_proxy = std::make_shared<TrafficData>("proxy");
        std::shared_ptr<TrafficData> total_direct = std::make_shared<TrafficData>("direct");
        
        DECLARE_STORE_TYPE(DatabaseLogger)
        NEW_MAP
            ADD_MAP("profiles", profiles, jsonStore);
            ADD_MAP("groups", groups, jsonStore);
            ADD_MAP("routes", routes, jsonStore);
            ADD_MAP("start_count", start_count, integer);
            ADD_MAP("usage_time", usage_time, integer);
            ADD_MAP("first_launch_time", first_launch_time, integer);
            ADD_MAP("last_launch_time", last_launch_time, integer);
            ADD_MAP("total_proxy", total_proxy, jsonStore);
            ADD_MAP("total_direct", total_direct, jsonStore);
            #ifdef NKR_SOFTWARE_KEYS
            ADD_MAP("failed_auth_count", failed_auth_count, integer);
            #endif
        STOP_MAP

        virtual bool Save() override;
        static long GetTime();

        long long get_usage_time();
        void initialize();

        #ifdef NKR_SOFTWARE_KEYS
        int get_failed_auth_count(bool pre_save = false);
        #endif
        private:
        long long usage_time = 0;
        long long usage_time_update = 0;
        #ifdef NKR_SOFTWARE_KEYS
        int failed_auth_count = 0;
        #endif
    };

    extern std::unique_ptr<DatabaseLogger> databaseLogger;
}

namespace Configs {
    const int INVALID_ID = -99999;

    class ProfileManager;

    extern ProfileManager *profileManager;

    class ProfileManager : private JsonStore {
    public:
        // JsonStore
        virtual ConfJsMap _map() override;
        DECLARE_STORE_TYPE(ProxyManager)
        // order -> id
        QList<int> groupsTabOrder;

        // Manager

        std::map<int, std::shared_ptr<ProxyEntity>> profiles;
        std::map<int, std::shared_ptr<Group>> groups;
        std::map<int, std::shared_ptr<RoutingChain>> routes;

        ProfileManager();

        // LoadManager Reset and loads profiles & groups
        void LoadManager();

        void SaveManager();

        [[nodiscard]] static QString GetDisplayType(const QString & type);

        [[nodiscard]] static std::shared_ptr<ProxyEntity> NewProxyEntity(const QString &type);

        [[nodiscard]] static std::shared_ptr<Group> NewGroup();

        [[nodiscard]] static std::shared_ptr<RoutingChain> NewRouteChain();

        bool AddProfile(const std::shared_ptr<ProxyEntity> &ent, int gid = -1);

        bool AddProfileBatch(const QList<std::shared_ptr<ProxyEntity>> &ents, int gid = -1);

        bool MoveProfile(int id, int gid);

        bool MoveProfileBatch(const QList<int>& ids, int gid);
        bool MoveProfileBatch(const QList<std::shared_ptr<ProxyEntity>>& ids, int gid);

        void DeleteProfile(int id);

        void BatchDeleteProfiles(const QList<int>& ids);

        std::shared_ptr<ProxyEntity> GetProfile(int id);

        bool AddGroup(const std::shared_ptr<Group> &ent);

        void DeleteGroup(int gid);

        std::shared_ptr<Group> GetGroup(int id);

        std::shared_ptr<Group> CurrentGroup();

        bool AddRouteChain(const std::shared_ptr<RoutingChain>& chain);

        std::shared_ptr<RoutingChain> GetRouteChain(int id);

        void UpdateRouteChains(const QList<std::shared_ptr<RoutingChain>>& newChain);

        QStringList GetExtraCorePaths() const;

        bool AddExtraCorePath(const QString &path);

    private:
        // sort by id
        QList<int> profilesIdOrder;
        QList<int> groupsIdOrder;
        QList<int> routesIdOrder;
        QSet<QString> extraCorePaths;

        [[nodiscard]] int NewProfileID() const;

        [[nodiscard]] int NewGroupID() const;

        [[nodiscard]] int NewRouteChainID() const;

        static std::shared_ptr<ProxyEntity> LoadProxyEntity(
            int id
        //        const QString &jsonPath, const QString &beanPath
        );

        static std::shared_ptr<Group> LoadGroup(int id);

        static std::shared_ptr<RoutingChain> LoadRouteChain(int id);

        void deleteProfile(int id);
    };

    extern ProfileManager *profileManager;
} // namespace Configs
