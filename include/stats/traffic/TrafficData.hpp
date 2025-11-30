#pragma once

#include "include/global/Configs.hpp"

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

        explicit TrafficData(std::string tag) {
            this->tag = std::move(tag);
        };
        
    #define d_put(map1, X, Y, B) _put(map1, X, &this->Y, B)

        virtual ConfJsMap & _map(){
            static ConfJsMap map1;
            static bool mapinit = false;
            if (!mapinit){
                d_put(map1, "dl", downlink, itemType::integer64);
                d_put(map1, "ul", uplink, itemType::integer64);
            }
            return map1;
        }

        void Reset() {
            downlink = 0;
            uplink = 0;
            downlink_rate = 0;
            uplink_rate = 0;
        }

        [[nodiscard]] QString DisplaySpeed() const {
            return UNICODE_LRO + QString("%1↑ %2↓").arg(ReadableSize(uplink_rate), ReadableSize(downlink_rate));
        }

        [[nodiscard]] QString DisplayTraffic() const {
            if (downlink + uplink == 0) return "";
            return UNICODE_LRO + QString("%1↑ %2↓").arg(ReadableSize(uplink), ReadableSize(downlink));
        }
    };
} // namespace Stats
