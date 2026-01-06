#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_tor.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditTor;
}
QT_END_NAMESPACE

class EditTor : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTor(QWidget *parent = nullptr);
    ~EditTor() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditTor *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
