#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <qnamespace.h>

namespace Configs
{

    #define _add(map1, X, Y, B) _put(map1, X, &this->Y, ITEM_TYPE(B))

    ConfJsMap ProxyEntity::_map(){
        static ConfJsMapStat map1;
        static ConfJsMapStat map2;
        static bool map1_init = true;
        if (map1_init ){
            map1_init = false;
            _add(map1, "type", type, string);
            _add(map1, "id", id, integer);
            _add(map1, "gid", gid, integer);
            _add(map1, "yc", latencyInt, integer);
            _add(map1, "dl", dl_speed, string);
            _add(map1, "ul", ul_speed, string);
            _add(map1, "report", full_test_report, string);
            _add(map1, "country", test_country, string);
            _add(map1, "is_working", is_working, boolean);
            _add(map1, "last_auto_test_time", last_auto_test_time, integer64);

            map2.insert(map1);
            _add(map2, "bean", bean_pointer, jsonStore);
            _add(map2, "traffic", traffic_data_pointer, jsonStore);
        }
        #undef _add
        
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
        if (latencyInt < 0) {
            result = QCoreApplication::translate("MainWindow", "Unavailable");
        } else if (latencyInt > 0) {
            if (!test_country.isEmpty()) result += UNICODE_LRO + CountryCodeToFlag(test_country) + " ";
            result += QString("%1 ms").arg(latencyInt);
        }
        if (!dl_speed.isEmpty() && dl_speed != "N/A") result += " ↓" + dl_speed;
        if (!ul_speed.isEmpty() && ul_speed != "N/A") result += " ↑" + ul_speed;
        return result;
    }

    std::list<ColorRule> latencyColorList = {  
        {1, 5, 0, 0, false, Qt::darkGreen},
        {6, 5, 0, 0, false, QColor(128, 0, 128)},
        {0,0,0,0, false, QColor(255, 165, 0)},
        {0, 0, 0, 0, true, Qt::red}  
    };

    QColor ProxyEntity::DisplayLatencyColor() const {
        if (latencyInt < 0){
            for (auto & color: latencyColorList){
                if (color.unavailable){
                    return color.color;
                }
            }
        } else {
            for (auto & color: latencyColorList){
                if (!color.unavailable){
                    if (color.orderRange > 0){
                        if (latencyOrder < (int)color.orderMin){
                            continue;
                        }
                        if (latencyOrder > (int)(color.orderMin + color.orderRange)){
                            continue;
                        }
                    }
                    if (color.latencyRange > 0){
                        if (latencyInt < (int)color.latencyMin){
                            continue;
                        }
                        if (latencyInt > (int)(color.latencyMin + color.latencyRange)){
                            continue;
                        }
                    }
                    return color.color;
                }
            }
        }
        return Qt::black;
    }
}
