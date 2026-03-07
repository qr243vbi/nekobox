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

        INIT_MAP
            ADD_MAP("state_directory", state_directory, string);
            ADD_MAP("auth_key", auth_key, string);
            ADD_MAP("control_url", control_url, string);
            ADD_MAP("ephemeral", ephemeral, boolean);
            ADD_MAP("hostname", hostname, string);
            ADD_MAP("accept_routes", accept_routes, boolean);
            ADD_MAP("exit_node", exit_node, string);
            ADD_MAP("exit_node_allow_lan_access", exit_node_allow_lan_access, boolean);
            ADD_MAP("advertise_routes", advertise_routes, stringList);
            ADD_MAP("advertise_exit_node", advertise_exit_node, boolean);
            ADD_MAP("globalDNS", globalDNS, boolean);
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
