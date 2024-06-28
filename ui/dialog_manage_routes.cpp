#include "dialog_manage_routes.h"
#include "ui_dialog_manage_routes.h"
#include "db/Database.hpp"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "3rdparty/qv2ray/v3/components/GeositeReader/GeositeReader.hpp"
#include "main/GuiUtils.hpp"
#include "fmt/Preset.hpp"

#include <QFile>
#include <QMessageBox>
#include <QShortcut>

void DialogManageRoutes::reloadProfileItems() {
    if (chainList.empty()) {
        MessageBoxWarning("Invalid state", "The list of routing profiles is empty, this should be an unreachable state, crashes may occur now");
        return;
    }

    QSignalBlocker blocker = QSignalBlocker(ui->route_prof); // apparently the currentIndexChanged will make us crash if we clear the QComboBox
    ui->route_prof->clear();

    ui->route_profiles->clear();
    bool selectedChainGone = true;
    int i=0;
    for (const auto &item: chainList) {
        ui->route_prof->addItem(item->name);
        ui->route_profiles->addItem(item->name);
        if (item->id == currentRouteProfileID) {
            ui->route_prof->setCurrentIndex(i);
            selectedChainGone=false;
        }
        i++;
    }
    if (selectedChainGone) {
        currentRouteProfileID=chainList[0]->id;
        ui->route_prof->setCurrentIndex(0);
    }
    blocker.unblock();
}

DialogManageRoutes::DialogManageRoutes(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageRoutes) {
    ui->setupUi(this);
    auto profiles = NekoGui::profileManager->routes;
    for (const auto &item: profiles) {
        chainList << item.second;
    }
    if (chainList.empty()) {
        auto defaultChain = NekoGui::RoutingChain::GetDefaultChain();
        NekoGui::profileManager->AddRouteChain(defaultChain);
        chainList.append(defaultChain);
    }
    currentRouteProfileID = NekoGui::dataStore->routing->current_route_id;
    if (currentRouteProfileID < 0) currentRouteProfileID = chainList[0]->id;

    QStringList qsValue = {""};
    QString dnsHelpDocumentUrl;

    ui->default_out->setCurrentText(NekoGui::dataStore->routing->def_outbound);
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
    ui->remote_dns->setCurrentText(NekoGui::dataStore->routing->remote_dns);
    ui->remote_dns_strategy->setCurrentText(NekoGui::dataStore->routing->remote_dns_strategy);
    ui->direct_dns->setCurrentText(NekoGui::dataStore->routing->direct_dns);
    ui->direct_dns_strategy->setCurrentText(NekoGui::dataStore->routing->direct_dns_strategy);
    ui->dns_final_out->setCurrentText(NekoGui::dataStore->routing->dns_final_out);
    reloadProfileItems();

    connect(ui->route_profiles, &QListWidget::itemDoubleClicked, this, [=](const QListWidgetItem* item){
        on_edit_route_clicked();
    });

    connect(ui->route_prof, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCurrentRouteProfile(int)));

    deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);

    connect(deleteShortcut, &QShortcut::activated, this, [=]{
        on_delete_route_clicked();
    });

    ADD_ASTERISK(this)
}

void DialogManageRoutes::updateCurrentRouteProfile(int idx) {
    currentRouteProfileID = chainList[idx]->id;
}

DialogManageRoutes::~DialogManageRoutes() {
    delete ui;
}

void DialogManageRoutes::accept() {
    if (chainList.empty()) {
        MessageBoxInfo("Invalid settings", "Routing profile cannot be empty");
        return;
    }

    NekoGui::dataStore->routing->sniffing_mode = ui->sniffing_mode->currentIndex();
    NekoGui::dataStore->routing->domain_strategy = ui->domainStrategyCombo->currentText();
    NekoGui::dataStore->routing->outbound_domain_strategy = ui->outbound_domain_strategy->currentText();
    NekoGui::dataStore->routing->use_dns_object = ui->use_dns_object->isChecked();
    NekoGui::dataStore->routing->dns_object = ui->dns_object->toPlainText();
    NekoGui::dataStore->routing->remote_dns = ui->remote_dns->currentText();
    NekoGui::dataStore->routing->remote_dns_strategy = ui->remote_dns_strategy->currentText();
    NekoGui::dataStore->routing->direct_dns = ui->direct_dns->currentText();
    NekoGui::dataStore->routing->direct_dns_strategy = ui->direct_dns_strategy->currentText();
    NekoGui::dataStore->routing->dns_final_out = ui->dns_final_out->currentText();

    NekoGui::profileManager->UpdateRouteChains(chainList);
    NekoGui::dataStore->routing->current_route_id = currentRouteProfileID;
    NekoGui::dataStore->routing->def_outbound = ui->default_out->currentText();


    //
    QStringList msg{"UpdateDataStore"};
    msg << "RouteChanged";
    MW_dialog_message("", msg.join(","));

    QDialog::accept();
}

void DialogManageRoutes::on_new_route_clicked() {
    routeChainWidget = new RouteItem(this, NekoGui::ProfileManager::NewRouteChain());
    routeChainWidget->setWindowModality(Qt::ApplicationModal);
    routeChainWidget->show();
    connect(routeChainWidget, &RouteItem::settingsChanged, this, [=](const std::shared_ptr<NekoGui::RoutingChain>& chain) {
        chainList << chain;
        reloadProfileItems();
    });
}

void DialogManageRoutes::on_edit_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    routeChainWidget = new RouteItem(this, chainList[idx]);
    routeChainWidget->setWindowModality(Qt::ApplicationModal);
    routeChainWidget->show();
    connect(routeChainWidget, &RouteItem::settingsChanged, this, [=](const std::shared_ptr<NekoGui::RoutingChain>& chain) {
        if (chain->isViewOnly()) return;
        chainList[idx] = chain;
        reloadProfileItems();
    });

}

void DialogManageRoutes::on_delete_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;
    if (chainList.size() == 1) {
        MessageBoxWarning("Invalid operation", "Routing Profiles cannot be empty, try adding another profile or editing this one");
        return;
    }

    auto profileToDel = chainList[idx];
    if (profileToDel->isViewOnly()) return;
    chainList.removeAt(idx);
    if (profileToDel->id == currentRouteProfileID) {
        currentRouteProfileID = chainList[0]->id;
    }
    reloadProfileItems();
}