#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QGroupBox>

#include "db/RouteEntity.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class RouteItem;
}
QT_END_NAMESPACE

class RouteItem : public QGroupBox {
    Q_OBJECT

public:
    explicit RouteItem(QWidget *parent = nullptr, const std::shared_ptr<NekoGui::RoutingChain>& routeChain = nullptr);
    ~RouteItem() override;

    std::shared_ptr<NekoGui::RoutingChain> chain;
signals:
    void settingsChanged(const std::shared_ptr<NekoGui::RoutingChain> routeChain);

private:
    Ui::RouteItem *ui;
    int currentIndex = -1;

    [[nodiscard]] int getIndexOf(const QString& name) const;

    void showSelectItem(const QStringList& items, const QString& currentItem);

    void showTextEnterItem(const QStringList& items);

private slots:
    void on_ok_button_clicked();
    void on_cancel_button_clicked();
    void on_new_route_item_clicked();
    void on_moveup_route_item_clicked();
    void on_movedown_route_item_clicked();
    void on_route_view_json_clicked();
};
