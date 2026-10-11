// Test-only boundaries. No application headers, profiles, cores, RPC, network,
// settings persistence or real MainWindow are linked. QObject lifetime and
// Stop's timers and queued UI delivery are real Qt. Profile-start effects are inert.
#include <QCoreApplication>
#include <QEvent>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSemaphore>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <QString>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Configs {
struct InMemorySettings {
    bool auto_test_enable = true;
    bool core_running = true;
    bool prepare_exit = false;
    bool spmode_vpn = false;
    bool auto_test_tun_failover = false;
    int auto_test_working_pool_size = 10;
    int started_id = -1;
};
struct InMemoryProfile {
    bool is_working = true;
    int saveCalls = 0;
    std::function<void()> onSave;
    void Save() { ++saveCalls; if (onSave) onSave(); } // Counter only: never writes a file/database.
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
inline std::vector<int> uiCallbacks; // Count pending posts, not delivery adapters.
inline QList<QString> uiMessages;
inline InertWindow *GetMainWindow() { return window; }
inline void MW_show_log(const QString &message) { uiMessages.append(message); }
void postProductionUiThread(const std::function<void()> &callback);
inline void runOnUiThread(std::function<void()> callback) {
    uiCallbacks.push_back(0);
    postProductionUiThread(callback);
}

namespace Stats {
class ProxyAutoTester : public QObject {
public:
    QTimer *testTimer = new QTimer(this);
    QTimer *healthCheckTimer = new QTimer(this);
    bool isTestingInProgress = false;
    std::atomic<quint64> runGeneration{0};
    mutable QRecursiveMutex workingPoolMutex;
    QList<int> workingProxyPool;
    int activeProxyFailureCount = 2;
    QList<QList<int>> emittedPools;
    std::vector<std::pair<int, int>> emittedFailovers;
    int noBackupSignals = 0;
    QList<QString> statusMessages;

    // Definitions below are extracted verbatim from production source.
    void HandleProxyFailure(int proxyId, int attemptCount);
    void Stop();
    bool isEnabled() const;
    bool canDeliverFailover(int proxyId, quint64 generation) const;
    bool isBackupAvailable(int proxyId) const;
    QList<int> GetWorkingProxies() const;
    bool IsProxyWorking(int proxyId) const;
    void updateWorkingPool(int proxyId, bool isWorking);
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
