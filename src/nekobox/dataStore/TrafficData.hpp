#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "Configs.hpp"
#include "ConfigItem.hpp"
namespace Stats {
    class TrafficData : public JsonStore {
    public:
        int id = -1; // ent id
        std::string tag;
        bool isChainTail = false;
        bool ignoreForRate = false;

        long long downlink = 0;
        long long uplink = 0;
        long long downlink_rate = 0;
        long long uplink_rate = 0;

        long long last_update;


        DECLARE_STORE_TYPE(NoSave)

        explicit TrafficData(std::string tag) {
            this->tag = std::move(tag);
        };
    
        NEW_MAP
                ADD_MAP( "dl", downlink, integer64);
                ADD_MAP( "ul", uplink, integer64);
        STOP_MAP

        void Reset() {
            downlink = 0;
            uplink = 0;
            downlink_rate = 0;
            uplink_rate = 0;
        }

        [[nodiscard]] QString DisplaySpeed() const;

        [[nodiscard]] QString DisplayTraffic() const;
    };
}
