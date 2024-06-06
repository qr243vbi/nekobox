#include "RouteItem.h"
#include "ui_RouteItem.h"
#include "db/RouteEntity.h"
#include "db/Database.hpp"

#define ADJUST_SIZE runOnUiThread([=] { adjustSize(); adjustPosition(mainwindow); }, this);

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

    for (const auto &item: chain->Rules) {
        ui->route_items->addItem(item->name);
    }

    QStringList outboundOptions = {"direct", "block", "dns_out"};
    outboundOptions << get_all_outbounds();

    ui->rule_attr->addItems(NekoGui::RouteRule::get_attributes());
    ui->rule_out->addItems(outboundOptions);
    ui->rule_attr_text->hide();
    ui->rule_attr_data->setTitle("");

    connect(ui->rule_attr_selector, &QComboBox::currentTextChanged, this, [=](const QString& text){
       if (currentIndex == -1) return;
       chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), {text});
    });

    connect(ui->rule_attr_text, &QTextEdit::textChanged, this, [=] {
        if (currentIndex == -1) return;
        auto currentVal = ui->rule_attr_text->toPlainText().split('\n');
        chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), currentVal);
    });

    connect(ui->route_items, &QListWidget::itemClicked, this, [=](const QListWidgetItem *item) {
        auto idx = getIndexOf(item->text());
        if (idx == -1) return;
        currentIndex = idx;
        auto ruleItem = chain->Rules[idx];
        ui->rule_out->setCurrentText(get_outbound_name(ruleItem->outboundID));
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
}

RouteItem::~RouteItem() {
    delete ui;
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
