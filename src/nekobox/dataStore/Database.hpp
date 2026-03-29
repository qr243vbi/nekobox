#pragma once

#include "Configs.hpp"
#include "ProxyEntity.hpp"
#include "Group.hpp"
#include "RouteEntity.h"

namespace Configs {
    const int INVALID_ID = -99999;

    class ProfileManager;

    extern ProfileManager *profileManager;

    class ProfileManager : private JsonStore {
    public:
        // JsonStore
        virtual ConfJsMap _map() override;
        DECLARE_STORE_TYPE(TrafficLooper)
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
