#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_shadowtls.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditShadowTLS;
}
QT_END_NAMESPACE

class EditShadowTLS : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditShadowTLS(QWidget *parent = nullptr);

    ~EditShadowTLS() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditShadowTLS *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
