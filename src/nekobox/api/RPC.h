#pragma once
#include "nekobox/configs/ConfigBuilder.hpp"
#include <QString>
#include <gen-cpp/libcore_types.h>
#include <optional>

namespace API {
class Client {
public:
  explicit Client(std::function<void(const QString &)> onError,
                  const QString &host, int port);

  // QString returns is error string

  QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

  std::tuple<QString, std::shared_ptr<Configs::BuildConfigResult>> StartEntity(bool *rpcOK,
                      const std::shared_ptr<Configs::ProxyEntity> ent) {
    std::shared_ptr<Configs::BuildConfigResult> result =
        Configs::BuildConfig(ent, false, false);
    if (!result->error.isEmpty()) {
      *rpcOK = false;
      return std::make_tuple(result->error, result);
    }

    libcore::LoadConfigReq req;
    req.core_config =
        (QJsonObject2QString(result->coreConfig, true)).toStdString();
    req.disable_stats = (Configs::dataStore->disable_traffic_stats);
    if (ent->type == "extracore") {
      req.need_extra_process = (true);
      req.extra_process_path = (result->extraCoreData->path).toStdString();
      req.extra_process_args = (result->extraCoreData->args).toStdString();
      req.extra_process_conf = (result->extraCoreData->config).toStdString();
      req.extra_process_conf_dir =
          (result->extraCoreData->configDir).toStdString();
      req.extra_no_out = (result->extraCoreData->noLog);
    }
    return std::make_tuple(Start(rpcOK, req), result);
  };

  QString Stop(bool *rpcOK);

  std::optional<libcore::QueryStatsResp> QueryStats();

  std::optional<libcore::TestResp> Test(bool *rpcOK,
                                        const libcore::TestReq &request);

  void StopTests(bool *rpcOK);

  std::optional<libcore::QueryURLTestResponse> QueryURLTest(bool *rpcOK);

  QString SetSystemDNS(bool *rpcOK, bool clear) const;

  std::optional<libcore::ListConnectionsResp>
  ListConnections(bool *rpcOK) const;

  QString CheckConfig(bool *rpcOK, const QString &config) const;

  bool IsPrivileged(bool *rpcOK) const;

  std::optional<libcore::SpeedTestResponse>
  SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request);

  std::optional<libcore::QuerySpeedTestResponse>
  QueryCurrentSpeedTests(bool *rpcOK);

  std::optional<libcore::QueryCountryTestResponse>
  QueryCountryTestResults(bool *rpcOK);

private:
  std::string domain;
  int port;
  std::function<void(const QString &)> onError;
};

inline Client *defaultClient;
} // namespace API
