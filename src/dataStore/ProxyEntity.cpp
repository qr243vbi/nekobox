#include <include/dataStore/ProxyEntity.hpp>

namespace Configs
{

    #define d_put(map1, X, Y, B) _put(map1, X, &this->Y, B)

    ConfJsMap & ProxyEntity::_map(){
        static ConfJsMap map1;
        static ConfJsMap map2;
        static bool map1_init = true;
        if (map1_init ){
            map1_init = false;
            d_put(map1, "type", type, itemType::string);
            d_put(map1, "id", id, itemType::integer);
            d_put(map1, "gid", gid, itemType::integer);
            d_put(map1, "yc", latency, itemType::integer);
            d_put(map1, "dl", dl_speed, itemType::string);
            d_put(map1, "ul", ul_speed, itemType::string);
            d_put(map1, "report", full_test_report, itemType::string);
            d_put(map1, "country", test_country, itemType::string);

            map2 = ConfJsMap(map1);
            d_put(map2, "bean", bean_pointer, itemType::jsonStore);
            d_put(map2, "traffic", traffic_data_pointer, itemType::jsonStore);
        }
        #undef d_put
        
        if (bean_pointer == nullptr){
            return map1;
        } else {
            return map2;
        }
    }

    ProxyEntity::ProxyEntity(Configs::AbstractBean *bean, const QString &type_) {
        if (type_ != nullptr) this->type = type_;
        if (bean != nullptr) {
            this->bean = std::shared_ptr<Configs::AbstractBean>(bean);
            bean_pointer = bean;
            traffic_data_pointer = traffic_data.get();
        }
    };

    QString ProxyEntity::DisplayTestResult() const {
        QString result;
        if (latency < 0) {
            result = QCoreApplication::translate("MainWindow", "Unavailable");
        } else if (latency > 0) {
            if (!test_country.isEmpty()) result += UNICODE_LRO + CountryCodeToFlag(test_country) + " ";
            result += QString("%1 ms").arg(latency);
        }
        if (!dl_speed.isEmpty() && dl_speed != "N/A") result += " ↓" + dl_speed;
        if (!ul_speed.isEmpty() && ul_speed != "N/A") result += " ↑" + ul_speed;
        return result;
    }

    QColor ProxyEntity::DisplayLatencyColor() const {
        if (latency < 0) {
            return Qt::red;
        } else if (latency > 0) {
            if (latency <= 120) {
                return Qt::green;
            } else if (latency <= 250){
                return Qt::darkGreen;
            } else if (latency <= 420){
                return Qt::darkYellow;
            } else {
                return Qt::darkGray;
            }
        } else {
            return {};
        }
    }
}
