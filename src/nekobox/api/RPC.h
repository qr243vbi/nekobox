#pragma once
#include <QString>
#include <nekobox/configs/ConfigBuilder.hpp>
#ifndef Q_MOC_RUN
#include <gen-cpp/InstanceService.h>
#include <gen-cpp/libcore_types.h>
#endif
#include <optional>

namespace API {
class InstanceHandler : 
#ifndef Q_MOC_RUN
    public libcore::InstanceServiceIf,
#endif 
    public QObject {
  Q_OBJECT
public:
#ifndef Q_MOC_RUN
  void wakeUp() override { emit wokeUp(); }
#endif
signals:
  void wokeUp();
};
} // namespace API

namespace API {

class Client {
public:
  explicit Client();

  // QString returns is error string

  QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

  std::tuple<QString, std::shared_ptr<Configs::BuildConfigResult>>
  StartEntity(bool *rpcOK, const std::shared_ptr<Configs::ProxyEntity> ent);

  QString Stop(bool *rpcOK);

  std::optional<libcore::QueryStatsResp> QueryStats();

  std::optional<libcore::CacheURLResult>
  CacheHTTP(bool *rpcOK, const libcore::CacheURLRequest &request);

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

  std::optional<libcore::QueryIPTestResponse>
  IPTest(bool *rpcOK, const libcore::IPTestRequest &request);

  std::optional<libcore::QueryIPTestResponse> QueryIPTest(bool *rpcOK);

  QString EnableSystemProxy(const QString &address, int port,
                            bool isSocksSupported, bool *rpcOK);
  QString DisableSystemProxy(bool *rpcOK);

private:
  static void onError(const QString &error);
};

inline std::unique_ptr<Client> defaultClient;
} // namespace API
