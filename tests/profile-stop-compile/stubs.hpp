// Compile-only declarations for the exact MainWindow::profile_stop definition.
// These are synthetic type-checking boundaries, not Qt or application behavior.
// Nothing is linked or executed; there is deliberately no main() function.
#pragma once

#include <functional>
#include <initializer_list>
#include <vector>

struct QString {
    QString(const char *);
    bool isEmpty() const;
    QString arg(const QString &) const;
};
QString operator+(const char *, const QString &);
QString tr(const char *);
extern QString software_name;

struct Profile {
    int id;
    QString DisplayTypeAndName() const;
    void Save();
};

struct QMutex {
    bool tryLock();
    void lock();
    void unlock();
};
struct QSemaphore {
    void acquire();
    void release();
    bool tryAcquire(int, int);
};
struct QThread {
    static QThread *currentThread();
};
struct QEventLoop {
    enum ProcessEventsFlag { ExcludeUserInputEvents };
};
struct QCoreApplication {
    static QCoreApplication *instance();
    QThread *thread();
    static void processEvents(QEventLoop::ProcessEventsFlag, int);
};

struct MainWindow;
struct QMessageBox {
    enum Icon { Question };
    enum StandardButton { Yes = 1, No = 2 };
    QMessageBox(Icon, const QString &, const QString &, int, MainWindow *);
    static void warning(MainWindow *, const QString &, const QString &);
    void accepted();
    void deleteLater();
};
// Function templates with lambda parameters must have visible definitions.
template <class... Args> void connect(Args &&...) {}
struct MessageBoxTimer {
    MessageBoxTimer(MainWindow *, QMessageBox *, int);
    void cancel();
};

namespace Stats {
struct TrafficItem { int id; };
struct TrafficLooper {
    bool loop_enabled;
    QMutex loop_mutex;
    std::vector<TrafficItem *> items;
    void UpdateAll();
};
struct ConnectionLister { bool suspend; };
extern TrafficLooper *trafficLooper;
extern ConnectionLister *connection_lister;
}
namespace Configs {
struct ProfileManager { Profile *GetProfile(int); };
struct DataStore {
    bool need_keep_vpn_off;
    void UpdateStartedId(int);
};
extern ProfileManager *profileManager;
extern DataStore *dataStore;
}
namespace Icon {
enum class State { DISCONNECTING, RUNNING };
}
struct StartStopButton { void setState(Icon::State); };
struct Ui { StartStopButton *start_stop_button; };
struct Client { QString Stop(bool *); };
extern Client *defaultClient;

void MW_dialog_message(const char *, const char *);
void MW_show_log(const QString &);
void runOnUiThread(const std::function<void()> &);
void runOnNewThread(const std::function<void()> &);

struct MainWindow {
    Profile *running;
    Ui *ui;
    QMutex mu_stopping;
    Profile *runningCopy();
    void setRunning(Profile *);
    void UpdateConnectionListWithRecreate(std::initializer_list<int>);
    void refresh_proxy_list(int);
    void refresh_status();
    void refresh_proxy_list_impl_refresh_data(int, bool);
    void profile_stop(bool crash, bool block, bool manual);
};
