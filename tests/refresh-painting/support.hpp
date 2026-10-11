#pragma once
// Offline dependency adapters. The refresh methods and sort declaration are
// production source; Qt strings, profiles, storage and window plumbing are not.
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "nekobox/ui/group/GroupSort.hpp"

using QChar = char;
struct QString : std::string {
    using std::string::string;
    QString(std::string value) : std::string(std::move(value)) {}
    bool isEmpty() const { return empty(); }
    QString arg(long long value, int width, int base, QChar fill) const {
        std::ostringstream out;
        if (base == 16) out << std::hex;
        out << std::setfill(fill) << std::setw(width) << value;
        return out.str();
    }
    QString toUpper() const {
        QString value = *this;
        for (char &c : value) c = std::toupper(static_cast<unsigned char>(c));
        return value;
    }
};

namespace Configs {
struct Traffic { long long downlink = 0, uplink = 0; };
struct ProxyEntity {
    QString type, name, address, full_test_report;
    std::shared_ptr<Traffic> traffic_data = std::make_shared<Traffic>();
    int latencyInt = 0, latencyOrder = 0;
    QString DisplayAddress() const { return address; }
};
struct ProfileIds : std::vector<int> {
    using std::vector<int>::vector;
    int count() const { return static_cast<int>(size()); }
};
struct Group {
    ProfileIds profiles;
    bool HasProfile(int id) const {
        return std::find(profiles.begin(), profiles.end(), id) != profiles.end();
    }
};
struct ProfileManager {
    std::shared_ptr<Group> group;
    std::map<int, std::shared_ptr<ProxyEntity>> profiles;
    std::shared_ptr<Group> CurrentGroup() const { return group; }
    std::shared_ptr<ProxyEntity> GetProfile(int id) const {
        auto i = profiles.find(id);
        return i == profiles.end() ? nullptr : i->second;
    }
    int GetProfileLatency(int id) const {
        auto profile = GetProfile(id);
        return profile ? profile->latencyInt : -1;
    }
};
inline ProfileManager *profileManager = nullptr;
}

// Optional callback drives a real QTableView from Python in the same call stack.
// No QWidget ABI or undocumented Qt implementation detail is imitated here.
using EventCallback = void (*)(int, int);
inline EventCallback eventCallback = nullptr;
inline void event(int kind, int value) {
    if (eventCallback) eventCallback(kind, value);
}
struct Table {
    bool updatesEnabled = true, signalsBlocked = false;
    void setUpdatesEnabled(bool value) {
        updatesEnabled = value;
        event(1, value);
    }
    void blockSignals(bool value) {
        signalsBlocked = value;
        event(2, value);
    }
};
struct Ui { Table *proxyListTable; };
struct Model {
    Table *table;
    int refreshes = 0;
    void refresh() {
        if (!table->updatesEnabled) throw std::runtime_error("model refreshed with painting disabled");
        ++refreshes;
        event(3, 0);
    }
};
inline int warnings = 0, logs = 0;
inline QString warningText;
inline QString software_name = "test-only";
inline QString tr(const char *text) { return text; }
inline void MW_show_log(const char *) { ++logs; }
inline void MessageBoxWarning(const QString &, const QString &text) {
    ++warnings;
    warningText = text;
    event(4, 0);
}
class MainWindow {
public:
    Ui *ui;
    Model *tableModel;
    void refresh_proxy_list_impl(const int &id, GroupSortAction action);
    void refresh_proxy_list_impl_refresh_data(const int &id, bool stopping = false);
};
