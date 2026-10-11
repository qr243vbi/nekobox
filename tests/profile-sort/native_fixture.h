#pragma once
// Only domain storage/display and unused header geometry are synthetic.
// All model, view, proxy, selection, signal, and persistent-index behavior is Qt.
#include <QAbstractTableModel>
#include <QApplication>
#include <QHash>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>
#include <QTableView>
#include <QVector>
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include "production_classes.h"
#include "GroupSort.hpp"

namespace Configs {
struct Traffic { quint64 downlink = 0, uplink = 0; };
struct ProxyEntity {
    QString type, name, address, full_test_report;
    int latencyInt = 0, latencyOrder = 0;
    std::shared_ptr<Traffic> traffic_data = std::make_shared<Traffic>();
    QString DisplayAddress() const { return address; }
};
struct Group {
    QList<int> profiles;
    bool HasProfile(int id) const { return profiles.contains(id); }
};
struct ProfileManager {
    std::shared_ptr<Group> group = std::make_shared<Group>();
    std::map<int, std::shared_ptr<ProxyEntity>> profiles;
    std::shared_ptr<Group> CurrentGroup() const { return group; }
    std::shared_ptr<ProxyEntity> GetProfile(int id) const {
        const auto found = profiles.find(id);
        return found == profiles.end() ? nullptr : found->second;
    }
    int GetProfileLatency(int id) const {
        auto profile = GetProfile(id);
        return profile ? profile->latencyInt : 0;
    }
};
inline ProfileManager *profileManager = nullptr;
}

struct SyntheticHeader { void refresh() {} };
class MyTableModel : public QAbstractTableModel {
public:
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    int data_id(const QModelIndex &index) const;
    int data_id(int row) const;
    int count() const;
    std::shared_ptr<Configs::Group> m_data() const;
    void refresh();
    void sortProfiles(const std::function<bool(int, int)> &lessThan);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        const int id = data_id(index);
        if (role == SELECTION_KEEPER_ROLE) return id;
        const auto profile = Configs::profileManager->GetProfile(id);
        if (role != Qt::DisplayRole || !profile) return {};
        switch (index.column()) {
        case 0: return profile->type;
        case 1: return profile->address;
        case 2: return profile->name;
        case 3: return profile->latencyInt;
        case 4: return QVariant::fromValue(profile->traffic_data->downlink);
        default: return {};
        }
    }
    int old_count = -1;
    QTableView *m_view = nullptr;
    std::shared_ptr<SyntheticHeader> filter = std::make_shared<SyntheticHeader>();
};

inline int warnings = 0, missingGroupLogs = 0;
inline const QString software_name = "profile sort regression";
inline void MW_show_log(const char *) { ++missingGroupLogs; }
inline void MessageBoxWarning(const QString &, const QString &) { ++warnings; }
struct SyntheticUi { QTableView *proxyListTable = nullptr; };
class MainWindow {
public:
    SyntheticUi *ui = nullptr;
    MyTableModel *tableModel = nullptr;
    static QString tr(const char *text) { return QString::fromUtf8(text); }
    void refresh_proxy_list_impl(const int &id, GroupSortAction action);
    void refresh_proxy_list_impl_refresh_data(const int &id, bool stopping = false);
};
