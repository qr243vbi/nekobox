#include "include/stats/traffic/TrafficLooper.hpp"

#include "include/api/gRPC.h"
#include "include/ui/mainwindow_interface.h"

#include <QThread>
#include <QJsonDocument>
#include <QElapsedTimer>

namespace NekoGui_traffic {

    TrafficLooper *trafficLooper = new TrafficLooper;
    QElapsedTimer elapsedTimer;

    void TrafficLooper::UpdateAll() {
        if (NekoGui::dataStore->disable_traffic_stats) {
            return;
        }

        auto resp = NekoGui_rpc::defaultClient->QueryStats();
        proxy->uplink_rate = 0;
        proxy->downlink_rate = 0;

        for (const auto &item: this->items) {
            if (!resp.ups().contains(item->tag)) continue;
            auto now = elapsedTimer.elapsed();
            auto interval = now - item->last_update;
            item->last_update = now;
            if (interval <= 0) continue;
            auto up = resp.ups().at(item->tag);
            auto down = resp.downs().at(item->tag);
            item->uplink += up;
            item->downlink += down;
            item->uplink_rate = up * 1000 / interval;
            item->downlink_rate = down * 1000 / interval;
            auto isInter = false;
            for (const auto& inter_tag : resp.intermediate_tags())
            {
                if (inter_tag == item->tag)
                {
                    isInter = true;
                    break;
                }
            }
            if (isInter) continue;
            if (item->tag == "direct")
            {
                direct->uplink_rate = item->uplink_rate;
                direct->downlink_rate = item->downlink_rate;
            } else
            {
                proxy->uplink_rate += item->uplink_rate;
                proxy->downlink_rate += item->downlink_rate;
            }
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
