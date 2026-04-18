#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_socks_http.h"

namespace Ui {
    class EditSocksHttp;
}

class EditSocksHttp : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditSocksHttp(QWidget *parent = nullptr);
    bool socks_type = false;
    ~EditSocksHttp() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;


    bool onEndSocks();
    bool onEndHttp();
    bool onStartSocks();
    bool onStartHttp();

private:
    Ui::EditSocksHttp *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
