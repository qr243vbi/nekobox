#pragma once

#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/global/CountryHelper.hpp"
#include "nekobox/stats/traffic/TrafficData.hpp"
#include "nekobox/configs/proxy/AbstractBean.hpp"
#include <QColor>
#include "nekobox/configs/proxy/ExtraCore.h"

namespace Configs {
    class SocksHttpBean;

    class ShadowSocksBean;

    class VMessBean;

    class TrojanVLESSBean;

    class NaiveBean;

    class QUICBean;

    class AnyTLSBean;

    class MieruBean;

    class ShadowTLSBean;

    class WireguardBean;

    class TailscaleBean;

    class SSHBean;

    class CustomBean;

    class ChainBean;
}; // namespace Configs

namespace Configs {

    
    struct ColorRule{
        uint orderMin;
        uint orderRange;
        uint latencyMin;
        uint latencyRange;
        bool unavailable;
        QColor color;
    };
    

    extern std::list<ColorRule> latencyColorList;

    class ProxyEntity : public JsonStore {
    private:
        Stats::TrafficData * traffic_data_pointer = nullptr;
        Configs::AbstractBean * bean_pointer = nullptr; 
    public:
        virtual ConfJsMap _map() override;

        QString type;

        int id = -1;
        int gid = 0;
        int latencyInt = 0;
        int latencyOrder = 0;
        QString dl_speed;
        QString ul_speed;
        QString test_country;
        std::shared_ptr<Configs::AbstractBean> bean;
        std::shared_ptr<Stats::TrafficData> traffic_data = std::make_shared<Stats::TrafficData>("");

        QString full_test_report;

        ProxyEntity(Configs::AbstractBean *bean, const QString &type_);

        bool is_working = false;
        qint64 last_auto_test_time = 0;

        [[nodiscard]] QString DisplayTestResult() const;

        [[nodiscard]] QColor DisplayLatencyColor() const;

        [[nodiscard]] Configs::ChainBean *ChainBean() const {
            return (Configs::ChainBean *) bean.get();
        };

        [[nodiscard]] Configs::SocksHttpBean *SocksHTTPBean() const {
            return (Configs::SocksHttpBean *) bean.get();
        };

        [[nodiscard]] Configs::ShadowSocksBean *ShadowSocksBean() const {
            return (Configs::ShadowSocksBean *) bean.get();
        };

        [[nodiscard]] Configs::VMessBean *VMessBean() const {
            return (Configs::VMessBean *) bean.get();
        };

        [[nodiscard]] Configs::TrojanVLESSBean *TrojanVLESSBean() const {
            return (Configs::TrojanVLESSBean *) bean.get();
        };

        [[nodiscard]] Configs::NaiveBean *NaiveBean() const {
            return (Configs::NaiveBean *) bean.get();
        };

        [[nodiscard]] Configs::MieruBean *MieruBean() const {
            return (Configs::MieruBean *) bean.get();
        };

        [[nodiscard]] Configs::QUICBean *QUICBean() const {
            return (Configs::QUICBean *) bean.get();
        };

        [[nodiscard]] Configs::AnyTLSBean *AnyTLSBean() const {
            return (Configs::AnyTLSBean *) bean.get();
        };

        [[nodiscard]] Configs::ShadowTLSBean *ShadowTLSBean() const {
            return (Configs::ShadowTLSBean *) bean.get();
        };

        [[nodiscard]] Configs::WireguardBean *WireguardBean() const {
            return (Configs::WireguardBean *) bean.get();
        };

        [[nodiscard]] Configs::TailscaleBean *TailscaleBean() const
        {
            return (Configs::TailscaleBean *) bean.get();
        }

        [[nodiscard]] Configs::SSHBean *SSHBean() const {
            return (Configs::SSHBean *) bean.get();
        };

        [[nodiscard]] Configs::CustomBean *CustomBean() const {
            return (Configs::CustomBean *) bean.get();
        };

        [[nodiscard]] Configs::ExtraCoreBean *ExtraCoreBean() const {
            return (Configs::ExtraCoreBean *) bean.get();
        };
    };
} // namespace Configs
