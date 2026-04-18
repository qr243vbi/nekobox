#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "ConfigItem.hpp"
#include "Configs.hpp"
#include "DataStore.hpp"
#include "Utils.hpp"
#include <QColor>
#include <memory>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <nekobox/configs/proxy/ExtraCore.h>
#include <nekobox/dataStore/TrafficData.hpp>
#include <nekobox/global/CountryHelper.hpp>

namespace Configs {
class HttpBean;

class SocksBean;

class ShadowSocksBean;

class VMessBean;

class TrojanVLESSBean;

class NaiveBean;

class QUICBean;

class AnyTLSBean;

class MieruBean;

class TrustTunnelBean;

class JuicityBean;

class ShadowTLSBean;

class WireguardBean;

class TailscaleBean;

class SSHBean;

class TorBean;

class CustomBean;

class ChainBean;

class NaiveBean;
}; // namespace Configs

namespace Configs {
class ProxyEntity : public JsonStore {
private:
  std::weak_ptr<Configs::AbstractBean> weak_bean;
  std::shared_ptr<Configs::AbstractBean> strong_bean;
  bool SavePrivate();

public:
  DECLARE_STORE_TYPE(Proxies)
  DECLARE_ID_RETURN
  virtual ConfJsMap _map() override;
  virtual bool Save() override;

  DECLARE_FLAG(same_path_for_bean, custom_flag2)
  //    DECLARE_FLAG(bean_path_not_defined, custom_flag)

  bool isValid() const;

  QString type;

  QString name = "";
  QString serverAddress = "127.0.0.1";
  int serverPort = 1080;

  int id = -1;
  int gid = 0;
  int latencyInt = 0;
  int latencyOrder = 0;
  bool is_working = false;

  //        QString bean_cfg;
  QString dl_speed;
  QString ul_speed;
  QString test_country;
  std::shared_ptr<const Configs::AbstractBean> bean() const;
  virtual bool UnknownKeyHash(const QByteArray &array) override;
  std::shared_ptr<Stats::TrafficData> traffic_data =
      std::make_shared<Stats::TrafficData>("");
  QString full_test_report;

  ProxyEntity(const QString &type_);
  virtual ~ProxyEntity() override;

  qint64 last_auto_test_time = 0;

  template <typename A>
  std::shared_ptr<A> unlock(std::shared_ptr<const A> bean) {
    auto ret = this->weak_bean.lock();
    if ((void *)ret.get() == (void *)bean.get()) {
      if (ret != nullptr) {
        ret->save_control_no_save(false);
      }
      return std::static_pointer_cast<A>(ret);
    }
    return nullptr;
  }

  [[nodiscard]] QString DisplayAddress();

  [[nodiscard]] QString DisplayName();

  [[nodiscard]] QString DisplayCoreType();

  [[nodiscard]] QString DisplayType();

  [[nodiscard]] QString DisplayTypeAndName();

  void ResetBeans();

  [[nodiscard]] QString DisplayTestResult() const;

  //     [[nodiscard]] QColor DisplayLatencyColor() const;

#define cast_func(X)                                                           \
  [[nodiscard]] std::shared_ptr<const Configs::X##Bean> X##Bean() const;

  cast_func(Chain) cast_func(Socks) cast_func(Http) cast_func(ShadowSocks)
      cast_func(VMess) cast_func(TrojanVLESS)
      //       cast_func(Naive)
      cast_func(Mieru) cast_func(QUIC) cast_func(AnyTLS) cast_func(ShadowTLS)
          cast_func(Wireguard) cast_func(Tailscale) cast_func(SSH)
              cast_func(Tor) cast_func(Custom) cast_func(ExtraCore)
                  cast_func(Naive) cast_func(TrustTunnel) cast_func(Juicity)

#undef cast_func
};

} // namespace Configs
