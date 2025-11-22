#include "include/ui/mainwindow.h"

#include "include/dataStore/Database.hpp"
#include "include/configs/ConfigBuilder.hpp"
#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/api/RPC.h"
#include "include/ui/utils//MessageBoxTimer.h"
#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"

#include <QInputDialog>
#include <QPushButton>
#include <QDesktopServices>
#include <QMessageBox>

// rpc
using namespace API;

void MainWindow::setup_rpc() {
    // Setup Connection
    defaultClient = new Client(
        [=](const QString &errStr) {
            MW_show_log("[Error] Core: " + errStr);
        },
        "127.0.0.1", Configs::dataStore->core_port);

    // Looper
    runOnNewThread([=] { Stats::trafficLooper->Loop(); });
    runOnNewThread([=] {Stats::connection_lister->Loop(); });
}

void MainWindow::runURLTest(const QString& config, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID) {
    if (stopSpeedtest.load()) {
        MW_show_log(tr("Profile test aborted"));
        return;
    }

    libcore::TestReq req;
    req.setOutboundTags(outboundTags);

    req.setConfig(config);
    req.setUrl(Configs::dataStore->test_latency_url);
    req.setUseDefaultOutbound(useDefault);
    req.setMaxConcurrency(Configs::dataStore->test_concurrent);
    req.setTestTimeoutMs(Configs::dataStore->url_test_timeout_ms);

    auto done = new QMutex;
    done->lock();
    runOnNewThread([=,this]
    {
        bool ok;
        while (true)
        {
            QThread::msleep(200);
            if (done->try_lock()) break;
            auto resp = defaultClient->QueryURLTest(&ok);
            if (!ok || resp->results().isEmpty() )
            {
                continue;
            }

            bool needRefresh = false;
            for (const auto& res : resp->results())
            {
                int entid = -1;
                if (!tag2entID.isEmpty()) {
                    QString tag = (res.outboundTag());
                    entid = tag2entID.count(tag) == 0 ? -1 : tag2entID[tag];
                }
                if (entid == -1) {
                    continue;
                }
                auto ent = Configs::profileManager->GetProfile(entid);
                if (ent == nullptr) {
                    continue;
                }
                QString error = (res.error());
                if (error.isEmpty()) {
                    ent->latency = res.latencyMs();
                } else {
                    if (error.contains("test aborted") ||
                        error.contains("context canceled")) ent->latency=0;
                    else {
                        ent->latency = -1;
                        MW_show_log(tr("[%1] test error: %2").arg(
                            ent->bean->DisplayTypeAndName(), error));
                    }
                }
                ent->Save();
                needRefresh = true;
            }
            if (needRefresh)
            {
                runOnUiThread([=,this]{
                    refresh_proxy_list();
                });
            }
        }
        done->unlock();
        delete done;
    });
    bool rpcOK;
    auto result = defaultClient->Test(&rpcOK, req);
    done->unlock();
    //
    if (!rpcOK || result->results().isEmpty()) return;

    for (const auto &res: result->results()) {
        if (!tag2entID.isEmpty()) {
            auto tag = (res.outboundTag());
            entID = tag2entID.count(tag) == 0 ? -1 : tag2entID[tag];
        }
        if (entID == -1) {
            MW_show_log(tr("Something is very wrong, the subject ent cannot be found!"));
            continue;
        }

        auto ent = Configs::profileManager->GetProfile(entID);
        if (ent == nullptr) {
            MW_show_log(tr("Profile manager data is corrupted, try again."));
            continue;
        }
        auto error = (res.error());
        if (error.isEmpty()) {
            ent->latency = res.latencyMs();
        } else {
            if (error.contains("test aborted") ||
                error.contains("context canceled")) ent->latency=0;
            else {
                ent->latency = -1;
                MW_show_log(tr("[%1] test error: %2").arg(
                ent->bean->DisplayTypeAndName(), error));
            }
        }
        ent->Save();
    }
}

void MainWindow::urltest_current_group(const QList<std::shared_ptr<Configs::ProxyEntity>>& profiles) {
    if (profiles.isEmpty()) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last url test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    runOnNewThread([this, profiles]() {
        auto buildObject = Configs::BuildTestConfig(profiles, ruleSetMap);
        if (!buildObject->error.isEmpty()) {
            MW_show_log(tr("Failed to build test config: ") + buildObject->error);
            speedtestRunning.unlock();
            return;
        }

        std::atomic<int> counter(0);
        stopSpeedtest.store(false);
        auto testCount = buildObject->fullConfigs.size() + 
            (!buildObject->outboundTags.isEmpty());
        for (const auto &entID: buildObject->fullConfigs.keys()) {
            auto configStr = buildObject->fullConfigs[entID];
            auto func = [this, &counter, testCount, configStr, entID]() {
                MainWindow::runURLTest(configStr, true, {}, {}, entID);
                counter++;
                if (counter.load() == testCount) {
                    speedtestRunning.unlock();
                }
            };
            parallelCoreCallPool->start(func);
        }

        if (!buildObject->outboundTags.isEmpty()) {
            auto func = [this, &buildObject, &counter, testCount]() {
                MainWindow::runURLTest(QJsonObject2QString(buildObject->coreConfig, false), false, buildObject->outboundTags, buildObject->tag2entID);
                counter++;
                if (counter.load() == testCount) {
                    speedtestRunning.unlock();
                }
            };
            parallelCoreCallPool->start(func);
        }
        if (testCount == 0) speedtestRunning.unlock();

        speedtestRunning.lock();
        speedtestRunning.unlock();
        runOnUiThread([=,this]{
            refresh_proxy_list();
            MW_show_log(tr("URL test finished!"));
        });
    });
}

void MainWindow::stopTests() {
    stopSpeedtest.store(true);
    bool ok;
    defaultClient->StopTests(&ok);

    if (!ok) {
        MW_show_log(tr("Failed to stop tests"));
    }
}

void MainWindow::url_test_current() {
    last_test_time = QDateTime::currentSecsSinceEpoch();
    ui->label_running->setText(tr("Testing"));

    runOnNewThread([=,this] {
        libcore::TestReq req;
        req.setTestCurrent(true);
        req.setUrl(Configs::dataStore->test_latency_url);

        bool rpcOK;
        auto result = defaultClient->Test(&rpcOK, req);
        if (!rpcOK || result->results().isEmpty() ) return;

        auto results_0 = result->results().at(0);
        auto latency = results_0.latencyMs();
        last_test_time = QDateTime::currentSecsSinceEpoch();

        runOnUiThread([=,this] {
            if (!results_0.error().isEmpty()) {
                MW_show_log(QString("UrlTest error: %1").arg(
                    (results_0.error())));
            }
            if (latency <= 0) {
                ui->label_running->setText(tr("Test Result") + ": " + tr("Unavailable"));
            } else if (latency > 0) {
                ui->label_running->setText(tr("Test Result") + ": " + 
                    QString("%1 ms").arg(latency));
            }
        });
    });
}

void MainWindow::speedtest_current_group(const QList<std::shared_ptr<Configs::ProxyEntity>>& profiles, bool testCurrent)
{
    if (profiles.isEmpty() && !testCurrent) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last speed test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    runOnNewThread([this, profiles, testCurrent]() {
        if (!testCurrent)
        {
            auto buildObject = Configs::BuildTestConfig(profiles, ruleSetMap);
            if (!buildObject->error.isEmpty()) {
                MW_show_log(tr("Failed to build test config: ") + buildObject->error);
                speedtestRunning.unlock();
                return;
            }

            stopSpeedtest.store(false);
            for (const auto &entID: buildObject->fullConfigs.keys()) {
                auto configStr = buildObject->fullConfigs[entID];
                runSpeedTest(configStr, true, false, {}, {}, entID);
            }

            if (!buildObject->outboundTags.isEmpty()) {
                runSpeedTest(QJsonObject2QString(buildObject->coreConfig, false), false, false, buildObject->outboundTags, buildObject->tag2entID);
            }
        } else
        {
            stopSpeedtest.store(false);
            runSpeedTest("", true, true, {}, {});
        }

        speedtestRunning.unlock();
        runOnUiThread([=,this]{
            refresh_proxy_list();
            MW_show_log(tr("Speedtest finished!"));
        });
    });
}

void MainWindow::querySpeedtest(QDateTime lastProxyListUpdate, const QMap<QString, int>& tag2entID, bool testCurrent)
{
    bool ok;
    auto res = defaultClient->QueryCurrentSpeedTests(&ok);
    if (!ok || !res->isRunning())
    {
        return;
    }
    auto profile = testCurrent ? running : 
        Configs::profileManager->GetProfile(
            tag2entID[(res->result().outboundTag())]);
    if (profile == nullptr)
    {
        return;
    }
    runOnUiThread([=, this, &lastProxyListUpdate]
    {
        showSpeedtestData = true;
        currentSptProfileName = profile->bean->name;
        currentTestResult = res->result();
        UpdateDataView();
        auto result = res->result();

        if (result.error().isEmpty() && !result.cancelled() && 
            lastProxyListUpdate.msecsTo(QDateTime::currentDateTime()) >= 500)
        {
            auto dl_speed = result.dlSpeed();
            auto ul_speed = result.ulSpeed();
            auto latency = result.latency();
            auto country = result.serverCountry();
            if (!dl_speed.isEmpty()) profile->dl_speed = (dl_speed);
            if (!ul_speed.isEmpty()) profile->ul_speed = (ul_speed);
            if (profile->latency <= 0 && latency > 0) profile->latency = latency;
            if (!country.isEmpty()) profile->test_country = CountryNameToCode((country));
            refresh_proxy_list(profile->id);
            lastProxyListUpdate = QDateTime::currentDateTime();
        }
    });
}

void MainWindow::queryCountryTest(const QMap<QString, int>& tag2entID, bool testCurrent)
{
    bool ok;
    auto res = defaultClient->QueryCountryTestResults(&ok);
    if (!ok || res->results().isEmpty())
    {
        return;
    }
    for (const auto& result : res->results())
    {
        auto profile = testCurrent ? running : 
        Configs::profileManager->GetProfile(tag2entID[
            (result.outboundTag())]);
        if (profile == nullptr)
        {
            return;
        }
        runOnUiThread([=, this]
        {
            if (result.error().isEmpty() && !result.cancelled())
            {
                auto latency = result.latency();
                auto country = result.serverCountry();
                if (profile->latency <= 0 && latency > 0) profile->latency = latency;
                if (!country.isEmpty()) profile->test_country = CountryNameToCode(
                    (country));
                refresh_proxy_list(profile->id);
            }
        });
    }
}


void MainWindow::runSpeedTest(const QString& config, bool useDefault, bool testCurrent, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID)
{
    if (stopSpeedtest.load()) {
        MW_show_log(tr("Profile speed test aborted"));
        return;
    }

    libcore::SpeedTestRequest req;
    auto speedtestConf = Configs::dataStore->speed_test_mode;
    req.setOutboundTags(outboundTags);
    req.setConfig(config);
    req.setUseDefaultOutbound(useDefault);
    req.setTestDownload (speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::DL);
    req.setTestUpload (speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::UL);
    req.setSimpleDownload( speedtestConf == Configs::TestConfig::SIMPLEDL);
    req.setSimpleDownloadAddr( Configs::dataStore->simple_dl_url);
    req.setTestCurrent( testCurrent);
    req.setTimeoutMs( Configs::dataStore->speed_test_timeout_ms);
    req.setOnlyCountry( speedtestConf == Configs::TestConfig::COUNTRY);
    req.setCountryConcurrency( Configs::dataStore->test_concurrent);

    // loop query result
    auto doneMu = new QMutex;
    doneMu->lock();
    runOnNewThread([=,this]
    {
        QDateTime lastProxyListUpdate = QDateTime::currentDateTime();
        while (true) {
            QThread::msleep(100);
            if (doneMu->tryLock())
            {
                break;
            }
            if (speedtestConf == Configs::TestConfig::COUNTRY)
            {
                queryCountryTest(tag2entID, testCurrent);
            } else
            {
                querySpeedtest(lastProxyListUpdate, tag2entID, testCurrent);
            }
        }
        runOnUiThread([=, this]
        {
            showSpeedtestData = false;
            UpdateDataView(true);
        });
        doneMu->unlock();
        delete doneMu;
    });
    bool rpcOK;
    auto result = defaultClient->SpeedTest(&rpcOK, req);
    doneMu->unlock();
    //
    if (!rpcOK || result->results().isEmpty() ) return;

    for (const auto &res: result->results()) {
        if (testCurrent) entID = running ? running->id : -1;
        else {
            auto tag = (res.outboundTag());
            entID = tag2entID.count(tag) == 0 ? -1 : tag2entID[tag];
        }
        if (entID == -1) {
            MW_show_log(tr("Something is very wrong, the subject ent cannot be found!"));
            continue;
        }

        auto ent = Configs::profileManager->GetProfile(entID);
        if (ent == nullptr) {
            MW_show_log(tr("Profile manager data is corrupted, try again."));
            continue;
        }

        if (res.cancelled()) continue;

        if (res.error().isEmpty()) {
            ent->dl_speed = (res.dlSpeed());
            ent->ul_speed = (res.ulSpeed());
            auto latency = res.latency();
            if (ent->latency <= 0 && latency > 0) ent->latency = latency;
            auto country = res.serverCountry();
            if (!country.isEmpty()) ent->test_country = 
                CountryNameToCode(country);
        } else {
            ent->dl_speed = "N/A";
            ent->ul_speed = "N/A";
            ent->latency = -1;
            ent->test_country = "";
            MW_show_log(tr("[%1] speed test error: %2").arg(
                ent->bean->DisplayTypeAndName(), (res.error())));
        }
        ent->Save();
    }
}

bool MainWindow::set_system_dns(bool set, bool save_set) {
    if (!Configs::dataStore->enable_dns_server) {
        MW_show_log(tr("You need to enable hijack DNS server first"));
        return false;
    }
    if (!get_elevated_permissions(4)) {
        return false;
    }
    bool rpcOK;
    QString res;
    if (set) {
        res = defaultClient->SetSystemDNS(&rpcOK, false);
    } else {
        res = defaultClient->SetSystemDNS(&rpcOK, true);
    }
    if (!rpcOK) {
        MW_show_log(tr("Failed to set system dns: ") + res);
        return false;
    }
    if (save_set) Configs::dataStore->system_dns_set = set;
    return true;
}

void MainWindow::profile_start(int _id) {
    if (Configs::dataStore->prepare_exit) return;
#ifdef Q_OS_LINUX
    if (Configs::dataStore->enable_dns_server && Configs::dataStore->dns_server_listen_port <= 1024) {
        if (!get_elevated_permissions()) {
            MW_show_log(QString("Failed to get admin access, cannot listen on port %1 without it").arg(Configs::dataStore->dns_server_listen_port));
            return;
        }
    }
#endif

    auto ents = get_now_selected_list();
    auto ent = (_id < 0 && !ents.isEmpty()) ? ents.first() : Configs::profileManager->GetProfile(_id);
    if (ent == nullptr) return;

    if (select_mode) {
        emit profile_selected(ent->id);
        select_mode = false;
        refresh_status();
        return;
    }

    auto group = Configs::profileManager->GetGroup(ent->gid);
    if (group == nullptr || group->archive) return;

    auto result = BuildConfig(ent, ruleSetMap, false, false);
    if (!result->error.isEmpty()) {
        MessageBoxWarning(tr("BuildConfig return error"), result->error);
        return;
    }

    auto profile_start_stage2 = [=, this] {
        libcore::LoadConfigReq req;
        req.setCoreConfig(QJsonObject2QString(result->coreConfig, true));
        req.setDisableStats(Configs::dataStore->disable_traffic_stats);
        if (ent->type == "extracore")
        {
            req.setNeedExtraProcess(true);
            req.setExtraProcessPath(result->extraCoreData->path);
            req.setExtraProcessArgs(result->extraCoreData->args);
            req.setExtraProcessConf(result->extraCoreData->config);
            req.setExtraProcessConfDir(result->extraCoreData->configDir);
            req.setExtraNoOut(result->extraCoreData->noLog);
        }
        //
        bool rpcOK;
        QString error = defaultClient->Start(&rpcOK, req);
        if (!rpcOK) {
            return false;
        }
        if (!error.isEmpty()) {
            if (error.contains("configure tun interface")) {
                runOnUiThread([=, this] {

                    QMessageBox msg(
                        QMessageBox::Information,
                        tr("Tun device misbehaving"),
                        tr("If you have trouble starting VPN, you can force reset Core process here and then try starting the profile again. The error is %1").arg(error),
                        QMessageBox::NoButton,
                        this
                    );
                    msg.addButton(tr("Reset"), QMessageBox::ActionRole);
                    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);

                    msg.setDefaultButton(cancel);
                    msg.setEscapeButton(cancel);

                    int r = msg.exec() - 2;
                    if (r == 0) {
                        GetMainWindow()->StopVPNProcess();
                    }
                });
                return false;
            }
            runOnUiThread([=,this] { MessageBoxWarning("LoadConfig return error", error); });
            return false;
        }
        //
        Stats::trafficLooper->proxy = std::make_shared<Stats::TrafficData>("proxy");
        Stats::trafficLooper->direct = std::make_shared<Stats::TrafficData>("direct");
        Stats::trafficLooper->items = result->outboundStats;
        Stats::trafficLooper->isChain = ent->type == "chain";
        Stats::trafficLooper->loop_enabled = true;
        Stats::connection_lister->suspend = false;

        Configs::dataStore->UpdateStartedId(ent->id);
        running = ent;

        runOnUiThread([=, this] {
            refresh_status();
            refresh_proxy_list(ent->id);
        });

        return true;
    };

    if (!mu_starting.tryLock()) {
        MessageBoxWarning(software_name, tr("Another profile is starting..."));
        return;
    }
    if (!mu_stopping.tryLock()) {
        MessageBoxWarning(software_name, tr("Another profile is stopping..."));
        mu_starting.unlock();
        return;
    }
    mu_stopping.unlock();

    // check core state
    if (!Configs::dataStore->core_running) {
        runOnThread(
            [=, this] {
                MW_show_log(tr("Try to start the config, but the core has not listened to the RPC port, so restart it..."));
                core_process->start_profile_when_core_is_up = ent->id;
                core_process->Restart();
            },
            DS_cores);
        mu_starting.unlock();
        return; // let CoreProcess call profile_start when core is up
    }

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=,this] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 10000);

    runOnNewThread([=, this] {
        // stop current running
        if (running != nullptr) {
            profile_stop(false, true, true);
        }
        // do start
        MW_show_log(">>>>>>>> " + tr("Starting profile %1").arg(ent->bean->DisplayTypeAndName()));
        if (!profile_start_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to start profile %1").arg(ent->bean->DisplayTypeAndName()));
        }
        mu_starting.unlock();
        // cancel timeout
        runOnUiThread([=,this] {
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();
        });
    });
}

void MainWindow::set_spmode_system_proxy(bool enable, bool save) {
    if (enable != Configs::dataStore->spmode_system_proxy) {
        if (enable) {
            auto socks_port = Configs::dataStore->inbound_socks_port;
            SetSystemProxy(socks_port, socks_port, Configs::dataStore->proxy_scheme);
        } else {
            ClearSystemProxy();
        }
    }

    if (save) {
        Configs::dataStore->remember_spmode.removeAll("system_proxy");
        if (enable && Configs::dataStore->remember_enable) {
            Configs::dataStore->remember_spmode.append("system_proxy");
        }
        Configs::dataStore->Save();
    }

    Configs::dataStore->spmode_system_proxy = enable;
    refresh_status();
}

void MainWindow::profile_stop(bool crash, bool block, bool manual) {
    if (running == nullptr) {
        return;
    }
    auto id = running->id;

    auto profile_stop_stage2 = [=,this] {
        if (!crash) {
            bool rpcOK;
            QString error = defaultClient->Stop(&rpcOK);
            if (rpcOK && !error.isEmpty()) {
                runOnUiThread([=,this] { MessageBoxWarning(tr("Stop return error"), error); });
                return false;
            } else if (!rpcOK) {
                return false;
            }
        }
        return true;
    };

    if (!mu_stopping.tryLock()) {
        return;
    }
    QMutex blocker;
    if (block) blocker.lock();

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=,this] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 5000);

    Stats::trafficLooper->loop_enabled = false;
    Stats::connection_lister->suspend = true;
    UpdateConnectionListWithRecreate({});
    Stats::trafficLooper->loop_mutex.lock();
    Stats::trafficLooper->UpdateAll();
    for (const auto &item: Stats::trafficLooper->items) {
        if (item->id < 0) continue;
        Configs::profileManager->GetProfile(item->id)->Save();
        refresh_proxy_list(item->id);
    }
    Stats::trafficLooper->loop_mutex.unlock();

    restartMsgboxTimer->cancel();
    restartMsgboxTimer->deleteLater();
    restartMsgbox->deleteLater();

    runOnNewThread([=, this, &blocker] {
        // do stop
        MW_show_log(">>>>>>>> " + tr("Stopping profile %1").arg(running->bean->DisplayTypeAndName()));
        if (!profile_stop_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to stop, please restart the program."));
        }

        if (manual) Configs::dataStore->UpdateStartedId(-1919);
        Configs::dataStore->need_keep_vpn_off = false;
        running = nullptr;

        if (block) blocker.unlock();

        runOnUiThread([=, this, &blocker] {
            refresh_status();
            refresh_proxy_list_impl_refresh_data(id, true);

            mu_stopping.unlock();
        });
    });

    if (block)
    {
        blocker.lock();
        blocker.unlock();
    }
}
