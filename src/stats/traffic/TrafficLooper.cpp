#include "include/stats/traffic/TrafficLooper.hpp"

#include "include/api/gRPC.h"
#include "include/ui/mainwindow_interface.h"

#include <QThread>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>

namespace NekoGui_traffic {

    TrafficLooper *trafficLooper = new TrafficLooper;
    QElapsedTimer elapsedTimer;

    TrafficData *TrafficLooper::update_stats(TrafficData *item) {
        if (NekoGui::dataStore->disable_traffic_stats) {
            return nullptr;
        }
        // last update
        auto now = elapsedTimer.elapsed();
        auto interval = now - item->last_update;
        item->last_update = now;
        if (interval <= 0) return nullptr;

        // query
        auto uplink = NekoGui_rpc::defaultClient->QueryStats(item->tag, "uplink");
        auto downlink = NekoGui_rpc::defaultClient->QueryStats(item->tag, "downlink");

        // add diff
        item->downlink += downlink;
        item->uplink += uplink;
        item->downlink_rate = downlink * 1000 / interval;
        item->uplink_rate = uplink * 1000 / interval;

        // return diff
        auto ret = new TrafficData(item->tag);
        ret->downlink = downlink;
        ret->uplink = uplink;
        ret->downlink_rate = item->downlink_rate;
        ret->uplink_rate = item->uplink_rate;
        return ret;
    }

    void TrafficLooper::UpdateAll() {
        if (NekoGui::dataStore->disable_traffic_stats) {
            return;
        }
        std::map<std::string, TrafficData *> updated; // tag to diff
        for (const auto &item: this->items) {
            auto data = item.get();
            auto diff = updated[data->tag];
            // 避免重复查询一个 outbound tag
            if (diff == nullptr) {
                diff = update_stats(data);
                updated[data->tag] = diff;
            } else {
                data->uplink += diff->uplink;
                data->downlink += diff->downlink;
                data->uplink_rate = diff->uplink_rate;
                data->downlink_rate = diff->downlink_rate;
            }
        }
        updated[direct->tag] = update_stats(direct);
        //
        for (const auto &pair: updated) {
            delete pair.second;
        }
    }

    void TrafficLooper::Loop() {
        if (NekoGui::dataStore->disable_traffic_stats) {
            return;
        }
        elapsedTimer.start();
        while (true) {
            auto sleep_ms = NekoGui::dataStore->traffic_loop_interval;
            if (sleep_ms < 500 || sleep_ms > 5000) sleep_ms = 1000;
            QThread::msleep(sleep_ms);
            if (NekoGui::dataStore->traffic_loop_interval == 0) continue; // user disabled

            // profile start and stop
            if (!loop_enabled) {
                // 停止
                if (looping) {
                    looping = false;
                    runOnUiThread([=] {
                        auto m = GetMainWindow();
                        m->refresh_status("STOP");
                    });
                }
                continue;
            } else {
                // 开始
                if (!looping) {
                    looping = true;
                }
            }

            // do update
            loop_mutex.lock();

            UpdateAll();

            loop_mutex.unlock();

            // post to UI
            runOnUiThread([=] {
                auto m = GetMainWindow();
                if (proxy != nullptr) {
                    m->refresh_status(QObject::tr("Proxy: %1\nDirect: %2").arg(proxy->DisplaySpeed(), direct->DisplaySpeed()));
                }
                for (const auto &item: items) {
                    if (item->id < 0) continue;
                    m->refresh_proxy_list(item->id);
                }
            });
        }
    }

} // namespace NekoGui_traffic
