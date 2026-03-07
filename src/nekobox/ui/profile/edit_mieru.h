#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_mieru.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditMieru;
}
QT_END_NAMESPACE

class EditMieru : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditMieru(QWidget *parent = nullptr);
    ~EditMieru() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditMieru *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
