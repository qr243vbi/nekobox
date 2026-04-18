#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_trusttunnel.h"
QT_BEGIN_NAMESPACE

namespace Ui {
    class EditTrustTunnel;
}
QT_END_NAMESPACE
class EditTrustTunnel : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTrustTunnel(QWidget *parent = nullptr);

    ~EditTrustTunnel() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditTrustTunnel *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
