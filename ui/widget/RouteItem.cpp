#include "RouteItem.h"
#include "ui_RouteItem.h"
#include "db/RouteEntity.h"
#include "db/Database.hpp"


int RouteItem::getIndexOf(const QString& name) const {
    for (int i=0;i<chain->Rules.size();i++) {
        if (chain->Rules[i]->name == name) return i;
    }

    return -1;
}

QString get_outbound_name(int id) {
    // -2 is direct -3 is block -4 is dns_out
    if (id == -2) return "direct";
    if (id == -3) return "block";
    if (id == -4) return "dns_out";
    auto profiles = NekoGui::profileManager->profiles;
    if (profiles.count(id)) return profiles[id]->bean->name;
    return "INVALID OUTBOUND";
}

int get_outbound_id(const QString& name) {
    if (name == "direct") return -2;
    if (name == "block") return -3;
    if (name == "dns_out") return -4;
    auto profiles = NekoGui::profileManager->profiles;
    for (const auto& item: profiles) {
        if (item.second->bean->name == name) return item.first;
    }

    return -1;
}

QStringList get_all_outbounds() {
    QStringList res;
    auto profiles = NekoGui::profileManager->profiles;
    for (const auto &item: profiles) {
        res.append(item.second->bean->name);
    }

    return res;
}

RouteItem::RouteItem(QWidget *parent, const std::shared_ptr<NekoGui::RoutingChain>& routeChain)
    : QGroupBox(parent), ui(new Ui::RouteItem) {
    ui->setupUi(this);

    // make a copy
    chain = routeChain;

    std::map<QString, int> valueMap;
    for (auto &item: chain->Rules) {
        auto baseName = item->name;
        int randPart;
        if (baseName == "") {
            randPart = GetRandomUint64()%1000;
            baseName = "rule_" + Int2String(randPart);
            lastNum = std::max(lastNum, randPart);
        }
        while (true) {
            valueMap[baseName]++;
            if (valueMap[baseName] > 1) {
                valueMap[baseName]--;
                randPart = GetRandomUint64()%1000;
                baseName = "rule_" + Int2String(randPart);
                lastNum = std::max(lastNum, randPart);
                continue;
            }
            item->name = baseName;
            break;
        }
        ui->route_items->addItem(item->name);
    }

    QStringList outboundOptions = {"direct", "block", "dns_out"};
    outboundOptions << get_all_outbounds();

    ui->rule_attr->addItems(NekoGui::RouteRule::get_attributes());
    ui->rule_out->addItems(outboundOptions);
    ui->rule_attr_text->hide();
    ui->rule_attr_data->setTitle("");

    connect(ui->route_name, &QLineEdit::textChanged, this, [=](const QString& text) {
       chain->name = text;
    });

    connect(ui->route_view_json, &QPushButton::clicked, this, [=] {
        QString res;
        auto rules = chain->get_route_rules(true);
        for (int i=0;i<rules.size();i++) {
            auto item = rules[i];
            res += QJsonObject2QString(item.toObject(), false);
            if (i != rules.size()-1) res+=",\n";
        }
        MessageBoxInfo("JSON object", res);
    });

    connect(ui->rule_name, &QLineEdit::textChanged, this, [=](const QString& text) {
        if (currentIndex == -1) return;
        chain->Rules[currentIndex]->name = text;
    });

    connect(ui->rule_attr_selector, &QComboBox::currentTextChanged, this, [=](const QString& text){
       if (currentIndex == -1) return;
       chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), {text});
    });

    connect(ui->rule_attr_text, &QTextEdit::textChanged, this, [=] {
        if (currentIndex == -1) return;
        auto currentVal = ui->rule_attr_text->toPlainText().split('\n');
        chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), currentVal);
    });

    connect(ui->rule_out, &QComboBox::currentTextChanged, this, [=](const QString& text) {
        if (currentIndex == -1) return;
        auto id = get_outbound_id(text);
        if (id == -1) {
            MessageBoxWarning("Invalid state", "selected outbound does not exists in the database, try restarting the app.");
            return;
        }
        chain->Rules[currentIndex]->outboundID = id;
    });

    connect(ui->route_items, &QListWidget::itemClicked, this, [=](const QListWidgetItem *item) {
        auto idx = getIndexOf(item->text());
        if (idx == -1) return;
        currentIndex = idx;
        auto ruleItem = chain->Rules[idx];
        ui->rule_out->setCurrentText(get_outbound_name(ruleItem->outboundID));
        setDefaultRuleData(ruleItem->ip_version);
    });

    connect(ui->rule_attr, &QComboBox::currentTextChanged, this, [=](const QString& text){
        if (currentIndex == -1) return;
        ui->rule_attr_data->setTitle(text);
        auto inputType = NekoGui::RouteRule::get_input_type(text);
        switch (inputType) {
            case NekoGui::trufalse: {
                    QStringList items = {"", "true", "false"};
                    auto currentValPtr = chain->Rules[currentIndex]->get_current_value_bool(text);
                    QString currentVal = currentValPtr == nullptr ? "" : *currentValPtr ? "true" : "false";
                    showSelectItem(items, currentVal);
                    break;
                }
            case NekoGui::select: {
                    auto items = NekoGui::RouteRule::get_values_for_field(text);
                    auto currentVal = chain->Rules[currentIndex]->get_current_value_string(text)[0];
                    showSelectItem(items, currentVal);
                    break;
                }
            case NekoGui::text: {
                    auto currentItems = chain->Rules[currentIndex]->get_current_value_string(text);
                    showTextEnterItem(currentItems);
                    break;
            }
        }
    });

    connect(ui->new_route_item, &QPushButton::clicked, this, &RouteItem::on_new_route_item_clicked);
    connect(ui->moveup_route_item, &QPushButton::clicked, this, &RouteItem::on_moveup_route_item_clicked);
    connect(ui->movedown_route_item, &QPushButton::clicked, this, &RouteItem::on_movedown_route_item_clicked);
    connect(ui->delete_route_item, &QPushButton::clicked, this, &RouteItem::on_delete_route_item_clicked);
}

RouteItem::~RouteItem() {
    delete ui;
}

void RouteItem::updateRouteItemsView() {
    ui->route_items->clear();
    if (chain->Rules.empty()) return;

    for (const auto& item: chain->Rules) {
        ui->route_items->addItem(item->name);
    }
    ui->route_items->setCurrentRow(currentIndex);
}

void RouteItem::setDefaultRuleData(const QString& currentData) {
    ui->rule_attr->setCurrentText("ip_version");
    ui->rule_attr_data->setTitle("ip_version");
    showSelectItem(NekoGui::RouteRule::get_values_for_field("ip_version"), currentData);
}

void RouteItem::showSelectItem(const QStringList& items, const QString& currentItem) {
    ui->rule_attr_text->hide();
    ui->rule_attr_selector->clear();
    ui->rule_attr_selector->show();
    QStringList fullItems = {""};
    fullItems << items;
    ui->rule_attr_selector->addItems(fullItems);
    ui->rule_attr_selector->setCurrentText(currentItem);
    adjustSize();
}

void RouteItem::showTextEnterItem(const QStringList& items) {
    ui->rule_attr_selector->hide();
    ui->rule_attr_text->clear();
    ui->rule_attr_text->show();
    ui->rule_attr_text->setText(items.join('\n'));
    adjustSize();
}

void RouteItem::on_new_route_item_clicked() {
    auto routeItem = std::make_shared<NekoGui::RouteRule>(NekoGui::RouteRule());
    routeItem->name = "rule_" + Int2String(++lastNum);
    chain->Rules << routeItem;
    currentIndex = chain->Rules.size() - 1;
    updateRouteItemsView();
    setDefaultRuleData("");
}

void RouteItem::on_moveup_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == 0) return;
    chain->Rules.swapItemsAt(currentIndex, currentIndex-1);
    currentIndex--;
    updateRouteItemsView();
}

void RouteItem::on_movedown_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == chain->Rules.size() - 1) return;
    chain->Rules.swapItemsAt(currentIndex, currentIndex+1);
    currentIndex++;
    updateRouteItemsView();
}

void RouteItem::on_delete_route_item_clicked() {
    if (currentIndex == -1) return;
    chain->Rules.removeAt(currentIndex);
    if (chain->Rules.empty()) currentIndex = -1;
    else {
        currentIndex--;
        if (currentIndex == -1) currentIndex = 0;
        setDefaultRuleData(chain->Rules[currentIndex]->ip_version);
    }
    updateRouteItemsView();
}