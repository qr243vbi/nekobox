#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#ifndef EDIT_NAIVE_H
#define EDIT_NAIVE_H

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_naive.h"

namespace Ui {
    class EditNaive;
}

class EditNaive : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditNaive(QWidget *parent = nullptr);

    ~EditNaive() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditNaive *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};

#endif // EDIT_SHADOWSOCKS_H
