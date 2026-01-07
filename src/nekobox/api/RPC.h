#pragma once
#include "nekobox/configs/ConfigBuilder.hpp"
#include <QString>
#ifndef Q_MOC_RUN
#include <gen-cpp/libcore_types.h>
#endif
#include <optional>

namespace API {
class Client {
public:
  explicit Client(std::function<void(const QString &)> onError,
                  const QString &host, int port);

  // QString returns is error string

  QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

  std::tuple<QString, std::shared_ptr<Configs::BuildConfigResult>> StartEntity(bool *rpcOK,
                      const std::shared_ptr<Configs::ProxyEntity> ent);

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
