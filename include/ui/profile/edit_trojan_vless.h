#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_trojan_vless.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditTrojanVLESS;
}
QT_END_NAMESPACE

class EditTrojanVLESS : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTrojanVLESS(QWidget *parent = nullptr);

    ~EditTrojanVLESS() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

    QComboBox* flow_;

private:
    Ui::EditTrojanVLESS *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
