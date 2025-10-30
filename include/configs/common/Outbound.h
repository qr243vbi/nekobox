#pragma once
#include "include/configs/baseConfig.h"

namespace Configs
{
    class OutboundCommons : public baseConfig
    {
        public:
        QString type;
        QString name;
        int id = -1;
        QString server;
        int server_port = 0;

        OutboundCommons()
        {
            _add(new configItem("type", &type, string));
            _add(new configItem("name", &name, string));
            _add(new configItem("id", &id, integer));
            _add(new configItem("server", &server, string));
            _add(new configItem("server_port", &server_port, integer));
        }
    };
}
