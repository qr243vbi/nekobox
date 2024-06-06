#include "dialog_manage_routes.h"
#include "ui_dialog_manage_routes.h"
#include "db/Database.hpp"
//#include "ui_RouteItem.h"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "3rdparty/qv2ray/v3/components/GeositeReader/GeositeReader.hpp"
#include "main/GuiUtils.hpp"
#include "fmt/Preset.hpp"

#include <QFile>
#include <QMessageBox>

QList<QString> getRouteProfiles() {
    auto routeProfiles = NekoGui::profileManager->routes;
    QList<QString> res;

    for (const auto &item: routeProfiles) {
        res << item.second->name;
    }
    return res;
}

int getRouteID(const QString& name) {
    auto routeProfiles = NekoGui::profileManager->routes;

    for (const auto &item: routeProfiles) {
        if (item.second->name == name) return item.first;
    }

    return -1;
}

QString getRouteName(int id) {
    return NekoGui::profileManager->routes.count(id) ? NekoGui::profileManager->routes[id]->name : "";
}

QList<QString> deleteItemFromList(const QList<QString>& base, const QString& target) {
    QList<QString> res;
    for (const auto &item: base) {
        if (item == target) continue;
        res << item;
    }
    return res;
}

void DialogManageRoutes::reloadProfileItems() {
    ui->route_prof->clear();
    ui->route_profiles->clear();
    ui->route_prof->addItems(currentRouteProfiles);
    ui->route_profiles->addItems(currentRouteProfiles);
}

DialogManageRoutes::DialogManageRoutes(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageRoutes) {
    ui->setupUi(this);
    currentRouteProfiles = getRouteProfiles();

    QStringList qsValue = {""};
    QString dnsHelpDocumentUrl;

    ui->outbound_domain_strategy->addItems(Preset::SingBox::DomainStrategy);
    ui->domainStrategyCombo->addItems(Preset::SingBox::DomainStrategy);
    qsValue += QString("prefer_ipv4 prefer_ipv6 ipv4_only ipv6_only").split(" ");
    ui->dns_object->setPlaceholderText(DecodeB64IfValid("ewogICJzZXJ2ZXJzIjogW10sCiAgInJ1bGVzIjogW10sCiAgImZpbmFsIjogIiIsCiAgInN0cmF0ZWd5IjogIiIsCiAgImRpc2FibGVfY2FjaGUiOiBmYWxzZSwKICAiZGlzYWJsZV9leHBpcmUiOiBmYWxzZSwKICAiaW5kZXBlbmRlbnRfY2FjaGUiOiBmYWxzZSwKICAicmV2ZXJzZV9tYXBwaW5nIjogZmFsc2UsCiAgImZha2VpcCI6IHt9Cn0="));
    dnsHelpDocumentUrl = "https://sing-box.sagernet.org/configuration/dns/";

    ui->direct_dns_strategy->addItems(qsValue);
    ui->remote_dns_strategy->addItems(qsValue);
    //
    connect(ui->use_dns_object, &QCheckBox::stateChanged, this, [=](int state) {
        auto useDNSObject = state == Qt::Checked;
        ui->simple_dns_box->setDisabled(useDNSObject);
        ui->dns_object->setDisabled(!useDNSObject);
    });
    ui->use_dns_object->stateChanged(Qt::Unchecked); // uncheck to uncheck
    connect(ui->dns_document, &QPushButton::clicked, this, [=] {
        MessageBoxInfo("DNS", dnsHelpDocumentUrl);
    });
    connect(ui->format_dns_object, &QPushButton::clicked, this, [=] {
        auto obj = QString2QJsonObject(ui->dns_object->toPlainText());
        if (obj.isEmpty()) {
            MessageBoxInfo("DNS", "invaild json");
        } else {
            ui->dns_object->setPlainText(QJsonObject2QString(obj, false));
        }
    });
    ui->sniffing_mode->setCurrentIndex(NekoGui::dataStore->routing->sniffing_mode);
    ui->outbound_domain_strategy->setCurrentText(NekoGui::dataStore->routing->outbound_domain_strategy);
    ui->domainStrategyCombo->setCurrentText(NekoGui::dataStore->routing->domain_strategy);
    ui->use_dns_object->setChecked(NekoGui::dataStore->routing->use_dns_object);
    ui->dns_object->setPlainText(NekoGui::dataStore->routing->dns_object);
    ui->dns_routing->setChecked(NekoGui::dataStore->routing->dns_routing);
    ui->remote_dns->setCurrentText(NekoGui::dataStore->routing->remote_dns);
    ui->remote_dns_strategy->setCurrentText(NekoGui::dataStore->routing->remote_dns_strategy);
    ui->direct_dns->setCurrentText(NekoGui::dataStore->routing->direct_dns);
    ui->direct_dns_strategy->setCurrentText(NekoGui::dataStore->routing->direct_dns_strategy);
    ui->dns_final_out->setCurrentText(NekoGui::dataStore->routing->dns_final_out);
    reloadProfileItems();
    ui->route_prof->setCurrentText(getRouteName(NekoGui::dataStore->routing->current_route_id));


    connect(ui->delete_route, &QPushButton::clicked, this, [=]{
        auto current = ui->route_profiles->currentItem()->text();
        currentRouteProfiles = deleteItemFromList(currentRouteProfiles, current);
        reloadProfileItems();
    });



    ADD_ASTERISK(this)
}

DialogManageRoutes::~DialogManageRoutes() {
    delete ui;
}

void DialogManageRoutes::accept() {
    if (currentRouteProfiles.empty()) {
        MessageBoxInfo("Invalid settings", "Routing profile cannot be empty");
        return;
    }

    NekoGui::dataStore->routing->sniffing_mode = ui->sniffing_mode->currentIndex();
    NekoGui::dataStore->routing->domain_strategy = ui->domainStrategyCombo->currentText();
    NekoGui::dataStore->routing->outbound_domain_strategy = ui->outbound_domain_strategy->currentText();
    NekoGui::dataStore->routing->use_dns_object = ui->use_dns_object->isChecked();
    NekoGui::dataStore->routing->dns_object = ui->dns_object->toPlainText();
    NekoGui::dataStore->routing->dns_routing = ui->dns_routing->isChecked();
    NekoGui::dataStore->routing->remote_dns = ui->remote_dns->currentText();
    NekoGui::dataStore->routing->remote_dns_strategy = ui->remote_dns_strategy->currentText();
    NekoGui::dataStore->routing->direct_dns = ui->direct_dns->currentText();
    NekoGui::dataStore->routing->direct_dns_strategy = ui->direct_dns_strategy->currentText();
    NekoGui::dataStore->routing->dns_final_out = ui->dns_final_out->currentText();

    // TODO add mine

    QDialog::accept();
}