namespace {
int assertions = 0;
int failures = 0;
int scenarios = 0;
void check(bool value, const std::string &scenario, const char *expectation) {
    ++assertions;
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << scenario << ": " << expectation << '\n';
    }
}
void flushInertCallbacks() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    uiCallbacks.clear();
}
struct Fixture {
    Configs::InMemorySettings settings;
    Configs::InMemoryProfiles profiles;
    InertWindow target;
    Stats::ProxyAutoTester tester;
    Fixture() {
        Configs::dataStore = &settings;
        Configs::profileManager = &profiles;
        window = &target;
        uiCallbacks.clear();
        uiMessages.clear();
    }
    std::shared_ptr<Configs::InMemoryProfile> add(int id) {
        auto profile = std::make_shared<Configs::InMemoryProfile>();
        profiles.profiles.insert(id, profile);
        return profile;
    }
};

void matrix() {
    // Removing the TUN-only guard, applying it globally, reversing its polarity,
    // or returning before failure bookkeeping must make these tests fail.
    for (bool tun : {false, true})
    for (bool enabled : {false, true})
    for (bool active : {false, true})
    for (bool backup : {false, true})
    for (bool failedExists : {false, true}) {
        ++scenarios;
        const std::string name = std::string(tun ? "TUN" : "proxy") +
            (enabled ? "/checked" : "/unchecked") +
            (active ? "/active" : "/inactive") +
            (backup ? "/backup" : "/empty") +
            (failedExists ? "/profile-present" : "/profile-missing");
        Fixture fixture;
        fixture.settings.spmode_vpn = tun;
        fixture.settings.auto_test_tun_failover = enabled;
        fixture.settings.started_id = active ? 7 : 99;
        auto failed = failedExists ? fixture.add(7) : nullptr;
        if (backup) fixture.add(8);
        fixture.tester.workingProxyPool = backup ? QList<int>{7, 8, 7} : QList<int>{7, 7};
        fixture.tester.HandleProxyFailure(7, 2);

        // Explicit expected policy: the checkbox affects TUN only.
        bool allowed = false;
        if (active) {
            if (!tun) allowed = true;
            else if (enabled) allowed = true;
        }
        const bool switched = allowed && backup;
        const QList<int> expectedPool = backup ? QList<int>{8} : QList<int>{};
        check(!failed || !failed->is_working, name, "failed profile marked unhealthy");
        check(!failed || failed->saveCalls == 1, name, "one inert failure-save request");
        check(fixture.tester.workingProxyPool == expectedPool, name, "failed ID removed from working pool");
        check(fixture.tester.emittedPools == QList<QList<int>>{expectedPool}, name, "failure bookkeeping announced exactly once");
        check(fixture.tester.emittedFailovers.size() == (switched ? 1u : 0u), name, "failover signal obeys policy");
        if (switched) check(!fixture.tester.emittedFailovers.empty() && fixture.tester.emittedFailovers.front() == std::pair<int, int>{7, 8}, name, "correct signal IDs");
        check(fixture.tester.noBackupSignals == (allowed && !backup ? 1 : 0), name, "no-backup signal obeys policy");
        check(uiCallbacks.size() == (allowed ? 1u : 0u), name, "UI request queued only when policy permits");
        check(fixture.target.requests.empty(), name, "queued callback has not executed");
        check(fixture.tester.activeProxyFailureCount == (switched ? 0 : 2), name, "failure count reset only for permitted failover");
        flushInertCallbacks();
        check(fixture.target.requests.size() == (switched ? 1u : 0u), name, "inert switch request obeys policy");
        if (switched) check(!fixture.target.requests.empty() && fixture.target.requests.front() == std::pair<int, bool>{8, true}, name, "correct inert switch arguments");
        check(uiMessages.size() == (allowed ? 1 : 0), name, "UI failover message obeys policy");
    }
}

void changedSettingIsReadAtNextDecision() {
    ++scenarios;
    const std::string name = "TUN setting changed between decisions";
    Fixture fixture;
    fixture.settings.spmode_vpn = true;
    fixture.settings.started_id = 7;
    fixture.add(7);
    fixture.add(8);
    fixture.tester.workingProxyPool = {7, 8};
    fixture.tester.HandleProxyFailure(7, 2);
    check(fixture.tester.emittedFailovers.empty() && uiCallbacks.empty(), name, "unchecked suppresses first decision");
    fixture.settings.auto_test_tun_failover = true;
    fixture.tester.HandleProxyFailure(7, 3);
    check(fixture.tester.emittedFailovers.size() == 1 && uiCallbacks.size() == 1, name, "next decision reads current checkbox value");
    flushInertCallbacks();
    check(fixture.target.requests.size() == 1, name, "one inert switch after enabling");
}

void unavailableBackupIsPruned() {
    for (bool presentButUnhealthy : {false, true}) {
        ++scenarios;
        const std::string name = presentButUnhealthy ? "unhealthy backup" : "missing backup";
        Fixture fixture;
        fixture.settings.spmode_vpn = true;
        fixture.settings.auto_test_tun_failover = true;
        fixture.settings.started_id = 7;
        fixture.add(7);
        if (presentButUnhealthy) fixture.add(8)->is_working = false;
        fixture.tester.workingProxyPool = {7, 8};
        fixture.tester.HandleProxyFailure(7, 2);
        check(fixture.tester.workingProxyPool.isEmpty(), name, "real pool-pruning policy removes invalid backup");
        check(fixture.tester.emittedFailovers.empty(), name, "invalid backup not selected");
        check(fixture.tester.noBackupSignals == 1, name, "no-backup signal emitted");
        flushInertCallbacks();
        check(fixture.target.requests.empty(), name, "no inert switch request");
    }
}

void absentWindowRemainsSafe() {
    ++scenarios;
    const std::string name = "no main window";
    Fixture fixture;
    fixture.settings.spmode_vpn = true;
    fixture.settings.auto_test_tun_failover = true;
    fixture.settings.started_id = 7;
    fixture.add(7);
    fixture.add(8);
    fixture.tester.workingProxyPool = {7, 8};
    window = nullptr;
    fixture.tester.HandleProxyFailure(7, 2);
    check(fixture.tester.emittedFailovers.size() == 1, name, "existing signal policy preserved");
    flushInertCallbacks();
    check(fixture.target.requests.empty(), name, "no window dereference or switch");
}

void staleSwitchesAreCancelled() {
    for (const QString change : {"manual-switch", "manual-stop", "disabled", "tun-disabled",
                                 "shutdown", "core-stopped", "backup-deleted", "backup-unhealthy",
                                 "backup-removed-from-pool", "tester-stopped"}) {
        ++scenarios;
        const std::string name = "pending failover / " + change.toStdString();
        Fixture fixture;
        fixture.settings.started_id = 7;
        fixture.settings.spmode_vpn = true;
        fixture.settings.auto_test_tun_failover = true;
        fixture.add(7);
        const auto backup = fixture.add(8);
        fixture.tester.workingProxyPool = {7, 8};
        fixture.tester.HandleProxyFailure(7, 2);
        check(uiCallbacks.size() == 1, name, "request initially queued");
        if (change == "manual-switch") fixture.settings.started_id = 99;
        if (change == "manual-stop") fixture.settings.started_id = -1;
        if (change == "disabled") fixture.settings.auto_test_enable = false;
        if (change == "tun-disabled") fixture.settings.auto_test_tun_failover = false;
        if (change == "shutdown") fixture.settings.prepare_exit = true;
        if (change == "core-stopped") fixture.settings.core_running = false;
        if (change == "backup-deleted") fixture.profiles.profiles.remove(8);
        if (change == "backup-unhealthy") backup->is_working = false;
        if (change == "backup-removed-from-pool") fixture.tester.workingProxyPool.clear();
        if (change == "tester-stopped") fixture.tester.Stop();
        flushInertCallbacks();
        check(fixture.target.requests.empty(), name, "obsolete switch not delivered");
        check(uiMessages.empty(), name, "cancelled switch not announced");
    }
    ++scenarios;
    const std::string name = "pending failover / destroyed tester";
    Fixture fixture;
    fixture.settings.started_id = 7;
    fixture.add(7);
    fixture.add(8);
    auto tester = std::make_unique<Stats::ProxyAutoTester>();
    tester->workingProxyPool = {7, 8};
    tester->HandleProxyFailure(7, 2);
    tester.reset();
    flushInertCallbacks();
    check(fixture.target.requests.empty(), name, "destroyed tester cancels switch");
    check(uiMessages.empty(), name, "destroyed tester cancels announcement");
}

void stopRestartCancelsOnlyOldRequests() {
    ++scenarios;
    const std::string name = "stop / later failover decision";
    Fixture fixture;
    fixture.settings.started_id = 7;
    fixture.add(7);
    fixture.add(8);
    fixture.tester.workingProxyPool = {7, 8};
    fixture.tester.HandleProxyFailure(7, 2);
    fixture.tester.Stop();
    fixture.tester.HandleProxyFailure(7, 2);
    flushInertCallbacks();
    check(fixture.target.requests.size() == 1, name, "only the new generation may request a switch");
}

#ifdef HAS_BACKUP_VALIDATION
void concurrentPoolUpdates() {
    ++scenarios;
    const std::string name = "concurrent pool updates and snapshots";
    Fixture fixture;
    fixture.add(8);
    std::atomic<bool> start{false};
    std::thread writer([&] {
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 20000; ++i) {
            fixture.tester.updateWorkingPool(8, true);
            fixture.tester.updateWorkingPool(8, false);
        }
    });
    start = true;
    bool valid = true;
    for (int i = 0; i < 20000; ++i) {
        const auto snapshot = fixture.tester.GetWorkingProxies();
        valid = valid && (snapshot.isEmpty() || snapshot == QList<int>{8});
        fixture.tester.IsProxyWorking(8);
    }
    writer.join();
    check(valid, name, "snapshot remains valid during worker mutations");
    check(fixture.tester.GetWorkingProxies().isEmpty(), name, "final pool matches completed worker update");
}

void backupValidationWaitsForWorkerStatusUpdate() {
    ++scenarios;
    const std::string name = "backup validation during worker failure update";
    Fixture fixture;
    fixture.settings.started_id = 7;
    const auto backup = fixture.add(8);
    fixture.tester.workingProxyPool = {8};
    QSemaphore saving, finishSave, validating, validated;
    backup->onSave = [&] { saving.release(); finishSave.acquire(); };
    std::thread writer([&] { fixture.tester.HandleProxyFailure(8, 2); });
    saving.acquire();
    bool available = true;
    std::thread reader([&] {
        validating.release();
        available = fixture.tester.isBackupAvailable(8);
        validated.release();
    });
    validating.acquire();
    const bool completedDuringUpdate = validated.tryAcquire(1, 100);
    finishSave.release();
    writer.join();
    reader.join();
    check(!completedDuringUpdate, name, "validation waits for in-flight worker status and pool update");
    check(!available, name, "failed backup rejected after worker update completes");
    check(fixture.tester.GetWorkingProxies().isEmpty(), name, "failed backup removed before validation completes");
}
#endif

void staleNoBackupMessageIsCancelled() {
    ++scenarios;
    const std::string name = "pending no-backup message / manual switch";
    Fixture fixture;
    fixture.settings.started_id = 7;
    fixture.add(7);
    fixture.tester.workingProxyPool = {7};
    fixture.tester.HandleProxyFailure(7, 2);
    fixture.settings.started_id = 99;
    flushInertCallbacks();
    check(uiMessages.empty(), name, "obsolete no-backup message suppressed");
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    std::cout << "Qt compiled: " << QT_VERSION_STR << "; runtime: " << qVersion() << '\n';
    matrix();
    changedSettingIsReadAtNextDecision();
    unavailableBackupIsPruned();
    absentWindowRemainsSafe();
    staleSwitchesAreCancelled();
    stopRestartCancelsOnlyOldRequests();
    staleNoBackupMessageIsCancelled();
#ifdef HAS_BACKUP_VALIDATION
    concurrentPoolUpdates();
    backupValidationWaitsForWorkerStatusUpdate();
#endif
    std::cout << scenarios << " scenarios, " << assertions << " assertions, "
              << failures << " failures (real Qt queue, inert application effects)\n";
    return failures ? 1 : 0;
}
