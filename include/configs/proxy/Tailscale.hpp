#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class TailscaleBean : public AbstractBean {
    public:
        QString state_directory = "$HOME/.tailscale";
        QString auth_key;
        QString control_url = "https://controlplane.tailscale.com";
        bool ephemeral = false;
        QString hostname;
        bool accept_routes = false;
        QString exit_node;
        bool exit_node_allow_lan_access = false;
        QStringList advertise_routes;
        bool advertise_exit_node = false;
        bool globalDNS = false;

        explicit TailscaleBean() : AbstractBean(0) {
        }
        #define _add(X, Y, B) ADD_MAP(X, Y, B)

        INIT_MAP
            _add("state_directory", state_directory, string);
            _add("auth_key", auth_key, string);
            _add("control_url", control_url, string);
            _add("ephemeral", ephemeral, boolean);
            _add("hostname", hostname, string);
            _add("accept_routes", accept_routes, boolean);
            _add("exit_node", exit_node, string);
            _add("exit_node_allow_lan_access", exit_node_allow_lan_access, boolean);
            _add("advertise_routes", advertise_routes, stringList);
            _add("advertise_exit_node", advertise_exit_node, boolean);
            _add("globalDNS", globalDNS, boolean);
        STOP_MAP
        

        QString DisplayType() override { return "Tailscale"; }

        QString DisplayAddress() override {return control_url; }

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;

        bool IsEndpoint() override {return true;}
    };
} // namespace Configs
