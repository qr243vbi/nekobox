#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include <QList>
#include <QMutex>
#include <QString>
#include <nekobox/dataStore/ConfigItem.hpp>
#include <nekobox/dataStore/TrafficData.hpp>
#include <nekobox/dataStore/Utils.hpp>

namespace Stats {

class TrafficLooper : public JsonStore {
public:
  DECLARE_STORE_TYPE(TrafficLooper)

  NEW_MAP
  ADD_MAP("proxy", proxy, jsonStore);
  ADD_MAP("direct", direct, jsonStore);
  STOP_MAP

  bool loop_enabled = false;
  bool looping = false;
  QMutex loop_mutex;

  QList<std::shared_ptr<TrafficData>> items;
  bool isChain;

  void UpdateAll();

  TrafficLooper();

  void initialize();

  void Loop();

  int total_direct_download();

  int total_proxy_download();

  int total_direct_upload();

  int total_proxy_upload();

private:
  std::shared_ptr<TrafficData> proxy;
  std::shared_ptr<TrafficData> direct;
};

extern TrafficLooper *trafficLooper;
} // namespace Stats
