



#pragma once

#include "ProxyEntity.hpp"
#include "Configs.hpp"
#include "ConfigItem.hpp"

namespace Configs
{
    struct GroupExtra : public JsonStore {
        virtual void fallback_job(JsonStore * ) override;
        DECLARE_STORE_TYPE(GroupBeans)
        DECLARE_ID_RETURN
        int id = -1;
        bool enable_custom_headers = false;
        bool enable_custom_payload = false;
        bool enable_hwid = false;
        QString custom_hwid = "";
        QVariantMap custom_headers;
        QString text_payload = "";
        QString javascript_payload = "";
        bool skip_auto_update = false;

        QString url = "";
        QString info = "";
        long long sub_last_update = 0;
        NEW_MAP
            ADD_MAP("enable_custom_headers", enable_custom_headers, boolean);
            ADD_MAP("enable_custom_payload", enable_custom_payload, boolean);
            ADD_MAP("enable_hwid", enable_hwid, boolean);
            ADD_MAP("custom_hwid", custom_hwid, string);
            ADD_MAP("url", url, string);
            ADD_MAP("info", info, string);
            ADD_MAP("sub_last_update", sub_last_update, integer64);
            ADD_MAP("skip_auto_update", skip_auto_update, boolean);
            ADD_MAP("text_payload", text_payload, string);
            ADD_MAP("javascript_payload", javascript_payload, string);
            ADD_MAP("custom_headers", custom_headers, stringMap);
        STOP_MAP
    };

    class Group : public JsonStore {
    public:

        DECLARE_STORE_TYPE(Groups)
        DECLARE_ID_RETURN
        int id = -1;
        bool archive = false;
        bool is_subscription = false;
        QString name = "";
        int front_proxy_id = -1;
        int landing_proxy_id = -1;

        QList<int> profiles;

        Group();

        std::shared_ptr<GroupExtra> getExtra();

        virtual std::shared_ptr<JsonStore> fallback() override;

        virtual ConfJsMap _map() override;

        [[nodiscard]] QList<int> Profiles() const;

        int DropNulls();

        bool RemoveProfile(int id);

        bool AddProfile(int id);

        bool SwapProfiles(int idx1, int idx2);

        bool EmplaceProfile(int idx, int newIdx);

        bool HasProfile(int id) const;

        QString getNotes() const;

        bool saveNotes(const QString&);
    };
}// namespace Configs
