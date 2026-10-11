// Test-only boundaries. No application headers, profiles, cores, RPC, network,
// settings persistence, QObject timers, or real MainWindow are linked.
#include <QList>
#include <QMap>
#include <QString>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Configs {
struct InMemorySettings {
    bool spmode_vpn = false;
    bool auto_test_tun_failover = false;
    int auto_test_working_pool_size = 10;
    int started_id = -1;
};
struct InMemoryProfile {
    bool is_working = true;
    int saveCalls = 0;
    void Save() { ++saveCalls; } // Counter only: never writes a file/database.
    QString DisplayTypeAndName() const { return "inert backup"; }
};
struct InMemoryProfiles {
    QMap<int, std::shared_ptr<InMemoryProfile>> profiles;
    std::shared_ptr<InMemoryProfile> GetProfile(int id) const {
        return profiles.value(id);
    }
};
inline InMemorySettings *dataStore = nullptr;
inline InMemoryProfiles *profileManager = nullptr;
}

struct InertWindow {
    std::vector<std::pair<int, bool>> requests;
    void profile_start(int id, bool force) { requests.emplace_back(id, force); }
};
inline InertWindow *window = nullptr;
inline std::vector<std::function<void()>> uiCallbacks;
inline QList<QString> uiMessages;
inline InertWindow *GetMainWindow() { return window; }
inline void MW_show_log(const QString &message) { uiMessages.append(message); }
inline void runOnUiThread(std::function<void()> callback) {
    uiCallbacks.push_back(std::move(callback)); // Queue only; no event loop.
}

namespace Stats {
class ProxyAutoTester {
public:
    QList<int> workingProxyPool;
    int activeProxyFailureCount = 2;
    QList<QList<int>> emittedPools;
    std::vector<std::pair<int, int>> emittedFailovers;
    int noBackupSignals = 0;
    QList<QString> statusMessages;

    // Definitions below are extracted verbatim from production source.
    void HandleProxyFailure(int proxyId, int attemptCount);
    int GetNextWorkingProxy();
    bool isTunFailoverEnabled() const;
    int getPoolSize() const;
    void pruneWorkingPool();

    // Inert signal/log boundaries. Real QObject delivery is not tested.
    void workingPoolUpdated(QList<int> pool) { emittedPools.append(pool); }
    void failoverTriggered(int oldId, int newId) {
        emittedFailovers.emplace_back(oldId, newId);
    }
    void noWorkingProxiesAvailable() { ++noBackupSignals; }
    void logStatus(const QString &message) { statusMessages.append(message); }
};
}
