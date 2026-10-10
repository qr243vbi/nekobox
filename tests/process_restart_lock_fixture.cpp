// Boundaries only: the restart decision and Restart() body come from production.
// No Qt event loop, process launch, wall-clock sleep, or privilege operation.
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using QString = std::string;
struct QObject {
    static QString tr(const char *text) { return text; }
};
struct WarningSink {
    template <class T> WarningSink &operator<<(const T &) { return *this; }
};
WarningSink qWarning() { return {}; }

// Non-recursive try-lock semantics, including rejection of a second acquisition.
// This test is single-threaded; it makes no claim about Qt thread dispatch.
struct QMutex {
    std::atomic_flag held = ATOMIC_FLAG_INIT;
    int acquisitions = 0;
    bool tryLock() {
        if (held.test_and_set()) return false;
        ++acquisitions;
        return true;
    }
    void unlock() {
        if (!held.test()) throw std::logic_error("unlock without ownership");
        held.clear();
    }
};
std::int64_t now = 1000;
struct QElapsedTimer {
    bool valid = false;
    std::int64_t began = 0;
    bool isValid() const { return valid; }
    void start() { valid = true; began = now; }
    std::int64_t restart() {
        const auto elapsed = now - began;
        start();
        return elapsed;
    }
};
struct QProcess {
    enum ProcessState { NotRunning, Starting, Running };
    ProcessState current = Running;
    std::function<void(ProcessState)> callback;
    int kills = 0;
    int waits = 0;
    int waitTimeout = 0;
    void stateChanged(ProcessState) {}
    void signal(ProcessState state) {
        current = state;
        callback(state);
    }
    void kill() {
        ++kills;
        if (current != NotRunning) signal(NotRunning);
    }
    void waitForFinished(int timeout) { ++waits; waitTimeout = timeout; }
};
template <class Context, class Callback>
void connect(QProcess *process, void (QProcess::*)(QProcess::ProcessState),
             Context *, Callback callback) {
    process->callback = callback;
}
namespace Configs {
struct DataStore {
    bool core_running = true;
    bool prepare_exit = false;
    int started_id = 42;
};
DataStore *dataStore = nullptr;
}
std::vector<QString> logs;
void MW_show_log(const QString &text) { logs.push_back(text); }
struct MainWindow {
    int stops = 0;
    bool crashArgument = false;
    bool blockArgument = false;
    std::function<void()> duringStop;
    void profile_stop(bool crash, bool block) {
        ++stops;
        crashArgument = crash;
        blockArgument = block;
        if (duringStop) duringStop();
    }
};
MainWindow *window = nullptr;
MainWindow *GetMainWindow() { return window; }
struct Pending {
    std::function<void()> callback;
    int delay;
};
std::deque<Pending> pending;
void setTimeout(const std::function<void()> &callback, QObject *, int delay) {
    pending.push_back({callback, delay});
}
void deliver() {
    if (pending.empty()) throw std::logic_error("no pending callback");
    auto task = std::move(pending.front());
    pending.pop_front();
    now += task.delay;
    task.callback();
}
namespace Configs_sys {
struct CoreProcess : QObject {
    QProcess process;
    QMutex restarting;
    QElapsedTimer coreRestartTimer;
    bool failed_to_start = false;
    bool started = true;
    int start_profile_when_core_is_up = -1;
    int starts = 0;
    bool startObservedReset = false;
    bool startObservedLock = false;
    CoreProcess();
    void Restart();
    void Start() {
        ++starts;
        startObservedReset = !started;
        startObservedLock = restarting.held.test();
        started = true;
        process.signal(QProcess::Running);
    }
};
}
#include "process_restart_source.inc"

int assertions = 0;
int failures = 0;
void check(const char *label, bool condition) {
    ++assertions;
    if (!condition) ++failures;
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
}
struct Fixture {
    Configs::DataStore data;
    MainWindow mainWindow;
    Configs_sys::CoreProcess core;
    Fixture() {
        now = 1000;
        logs.clear();
        pending.clear();
        Configs::dataStore = &data;
        window = &mainWindow;
    }
    ~Fixture() {
        pending.clear();
        window = nullptr;
        Configs::dataStore = nullptr;
    }
    void crash() { core.process.signal(QProcess::NotRunning); }
};

int main() {
    {
        Fixture f;
        f.crash();
        check("first crash queues exactly one restart", pending.size() == 1);
        check("first restart keeps its 200 ms delay", !pending.empty() && pending.front().delay == 200);
        check("cleanup preserves crash/block arguments",
              f.mainWindow.stops == 1 && f.mainWindow.crashArgument && f.mainWindow.blockArgument);
        check("first crash releases the lock", !f.core.restarting.held.test());
        check("first crash preserves the selected profile", f.core.start_profile_when_core_is_up == 42);
        deliver();
        check("queued restart invokes Start", f.core.starts == 1 && f.data.core_running);
        now += 1;
        f.crash();
        check("rapid crash still stops automatic retries", pending.empty());
        check("rapid crash still reports the rate limit",
              !logs.empty() && logs.back().find("exits too frequently") != QString::npos);
        check("rapid crash resets the restart timer", !f.core.coreRestartTimer.isValid());
        check("rapid crash releases the lock", !f.core.restarting.held.test());
        f.core.Restart();
        check("explicit restart after rapid crash reaches Start", f.core.starts == 2);
        check("explicit restart after rapid crash marks core running", f.data.core_running);
        check("explicit restart after rapid crash releases its lock", !f.core.restarting.held.test());
        f.crash();
        check("a new crash cycle can schedule a retry", pending.size() == 1);
    }
    for (const auto elapsed : {9999, 10000, 10001}) {
        Fixture f;
        f.crash();
        deliver();
        now = 1000 + elapsed;
        f.crash();
        const bool limited = elapsed < 10000;
        check("rate-limit boundary retains less-than 10 seconds", pending.empty() == limited);
        check("rate-limit boundary releases lock on either outcome", !f.core.restarting.held.test());
        check("rate-limit boundary resets timer only when limited",
              f.core.coreRestartTimer.isValid() != limited);
        check("rate-limit boundary still performs cleanup", f.mainWindow.stops == 2);
    }
    for (int earlyExit = 0; earlyExit < 3; ++earlyExit) {
        Fixture f;
        if (earlyExit == 0) Configs::dataStore = nullptr;
        if (earlyExit == 1) f.data.prepare_exit = true;
        if (earlyExit == 2) f.core.failed_to_start = true;
        f.crash();
        check("shutdown/missing store/failed start do not retry", pending.empty());
        check("early exits do not acquire the restart lock", f.core.restarting.acquisitions == 0);
        check("early exits do not clean up profiles", f.mainWindow.stops == 0);
    }
    {
        Fixture f;
        f.data.core_running = false;
        f.core.process.signal(QProcess::Running);
        check("Running updates core status", f.data.core_running);
        f.core.process.signal(QProcess::Starting);
        check("other states do not restart or acquire lock",
              pending.empty() && f.core.restarting.acquisitions == 0);
    }
    {
        Fixture f;
        f.core.restarting.tryLock();
        f.crash();
        f.core.Restart();
        check("busy restart lock rejects nested restart", f.core.starts == 0 && pending.empty());
        check("busy-lock exits preserve the owner's lock", f.core.restarting.held.test());
        check("busy-lock exit avoids cleanup", f.mainWindow.stops == 0);
        f.core.restarting.unlock();
    }
    {
        Fixture f;
        bool cleanupHeldLock = false;
        f.mainWindow.duringStop = [&] {
            cleanupHeldLock = f.core.restarting.held.test();
            if (f.mainWindow.stops == 1) f.crash();
        };
        f.crash();
        check("cleanup executes while lock is owned", cleanupHeldLock);
        check("reentrant NotRunning during cleanup is suppressed", f.mainWindow.stops == 1);
        check("reentrant cleanup schedules one restart", pending.size() == 1);
        check("outer cleanup releases the lock", !f.core.restarting.held.test());
    }
    {
        Fixture f;
        f.core.Restart();
        check("Restart suppresses kill-generated cleanup", f.mainWindow.stops == 0 && pending.empty());
        check("Restart waits for process exit with original timeout",
              f.core.process.kills == 1 && f.core.process.waits == 1 && f.core.process.waitTimeout == 500);
        check("Restart resets started before Start", f.core.startObservedReset);
        check("Restart owns lock through Start", f.core.startObservedLock);
        check("Restart releases lock afterward", !f.core.restarting.held.test());
    }
    {
        Fixture f;
        window = nullptr;
        f.crash();
        check("missing main window still allows restart", pending.size() == 1);
        check("missing main window still releases lock", !f.core.restarting.held.test());
    }
    {
        Fixture f;
        f.mainWindow.duringStop = [] { Configs::dataStore = nullptr; };
        f.crash();
        check("store removed during cleanup uses fallback profile", f.core.start_profile_when_core_is_up == -1);
        check("store removed during cleanup releases lock", !f.core.restarting.held.test());
    }
    std::cout << "SUMMARY assertions=" << assertions << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
