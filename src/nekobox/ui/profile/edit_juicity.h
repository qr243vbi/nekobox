#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_juicity.h"
QT_BEGIN_NAMESPACE

namespace Ui {
    class EditJuicity;
}
QT_END_NAMESPACE
class EditJuicity : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditJuicity(QWidget *parent = nullptr);

    ~EditJuicity() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditJuicity *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
