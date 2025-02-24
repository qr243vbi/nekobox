#pragma once

#include "include/dataStore/Database.hpp"

namespace NekoGui_sub {
    class RawUpdater {
    public:
        void updateClash(const QString &str);

        void update(const QString &str, bool needParse);

        void updateSingBox(const QString &str);

        int gid_add_to = -1;

        QList<std::shared_ptr<NekoGui::ProxyEntity>> updated_order;
    };

    class GroupUpdater : public QObject {
        Q_OBJECT

    public:
        void AsyncUpdate(const QString &str, int _sub_gid = -1, const std::function<void()> &finish = nullptr);

        void Update(const QString &_str, int _sub_gid = -1, bool _not_sub_as_url = false);

    signals:

        void asyncUpdateCallback(int gid);
    };

    extern GroupUpdater *groupUpdater;
} // namespace NekoGui_sub

void UI_update_all_groups(bool onlyAllowed = false);
