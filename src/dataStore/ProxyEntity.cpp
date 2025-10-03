#include <include/dataStore/ProxyEntity.hpp>

namespace Configs
{
    ProxyEntity::ProxyEntity(Configs::AbstractBean *bean, const QString &type_) {
        if (type_ != nullptr) this->type = type_;

        _add(new configItem("type", &type, itemType::string));
        _add(new configItem("id", &id, itemType::integer));
        _add(new configItem("gid", &gid, itemType::integer));
        _add(new configItem("yc", &latency, itemType::integer));
        _add(new configItem("dl", &dl_speed, itemType::string));
        _add(new configItem("ul", &ul_speed, itemType::string));
        _add(new configItem("report", &full_test_report, itemType::string));
        _add(new configItem("country_emoji", &test_country_emoji, itemType::string));

        if (bean != nullptr) {
            this->bean = std::shared_ptr<Configs::AbstractBean>(bean);
            _add(new configItem("bean", dynamic_cast<JsonStore *>(bean), itemType::jsonStore));
            _add(new configItem("traffic", dynamic_cast<JsonStore *>(traffic_data.get()), itemType::jsonStore));
        }
    };

    QString ProxyEntity::DisplayTestResult() const {
        QString result;
        if (latency < 0) {
            result = "Unavailable";
        } else if (latency > 0) {
            if (!test_country_emoji.isEmpty()) result += UNICODE_LRO + test_country_emoji + " ";
            result += QString("%1 ms").arg(latency);
        }
        if (!dl_speed.isEmpty() && dl_speed != "N/A") result += " ↓" + dl_speed;
        if (!ul_speed.isEmpty() && ul_speed != "N/A") result += " ↑" + ul_speed;
        return result;
    }

    QColor ProxyEntity::DisplayLatencyColor() const {
        if (latency < 0) {
            return Qt::darkGray;
        } else if (latency > 0) {
            if (latency <= 100) {
                return Qt::darkGreen;
            } else if (latency <= 300)
            {
                return Qt::darkYellow;
            } else {
                return Qt::red;
            }
        } else {
            return {};
        }
    }
}