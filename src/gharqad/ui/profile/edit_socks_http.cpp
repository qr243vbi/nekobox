#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/ui/profile/edit_socks_http.h>
#include <nekobox/configs/proxy/HttpBean.hpp>
#include <nekobox/configs/proxy/SocksBean.hpp>

EditSocksHttp::EditSocksHttp(QWidget *parent) : QWidget(parent),
                                                ui(new Ui::EditSocksHttp) {
    ui->setupUi(this);

}

EditSocksHttp::~EditSocksHttp() {
    delete ui;
}

bool EditSocksHttp::onStartHttp() {
    {
        ui->version->setVisible(false);
        ui->version_l->setVisible(false);
        ui->uot->setVisible(false);
        ui->uot_l->setVisible(false);
    }
    auto bean = this->ent->HttpBean();
    P_LOAD_STRING(username)
    P_LOAD_STRING(password)
    P_LOAD_STRING(path)
    P_LOAD_STRINGMAP(headers)

    return true;
}

bool EditSocksHttp::onEndHttp() {
    auto bean = ent->unlock(this->ent->HttpBean());
    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    P_SAVE_STRING(path)
    P_SAVE_STRINGMAP(headers)
    return true;
}

bool EditSocksHttp::onStartSocks() {
    auto bean = this->ent->SocksBean();

    if (bean->socks_http_type == Configs::SocksBean::type_Socks4) {
        ui->version->setCurrentIndex(1);
    } else {
        ui->version->setCurrentIndex(0);
    }
    ui->uot->setCurrentIndex(bean->uot);
    {
        ui->path->setVisible(false);
        ui->path_l->setVisible(false);
        ui->headers->setVisible(false);
        ui->headers_l->setVisible(false);
    }

    P_LOAD_STRING(username)
    P_LOAD_STRING(password)
    return true;
}


bool EditSocksHttp::onEndSocks() {
    auto bean = ent->unlock(ent->SocksBean());

    if (ui->version->isVisible()) {
        if (ui->version->currentIndex() == 1) {
            bean->socks_http_type = Configs::SocksBean::type_Socks4;
        } else {
            bean->socks_http_type = Configs::SocksBean::type_Socks5;
        }
    }
    bean->uot = ui->uot->currentIndex();

    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    return true;
}

void EditSocksHttp::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    socks_type = _ent->type == "socks";

    if (socks_type) {
        onStartSocks();
    } else {
        onStartHttp();
    }
    /*
    auto bean = this->ent->SocksHTTPBean();

    if (bean->socks_http_type == Configs::SocksHttpBean::type_Socks4) {
        ui->version->setCurrentIndex(1);
    } else {
        ui->version->setCurrentIndex(0);
    }
    if (bean->socks_http_type == Configs::SocksHttpBean::type_HTTP) {
        ui->version->setVisible(false);
        ui->version_l->setVisible(false);
    }

    ui->username->setText(bean->username);
    ui->password->setText(bean->password);
    */
}

bool EditSocksHttp::onEnd() {
    if (socks_type) {
        onEndSocks();
    } else {
        onEndHttp();
    }
    /*
    auto bean = ent->unlock(ent->SocksHTTPBean());

    if (ui->version->isVisible()) {
        if (ui->version->currentIndex() == 1) {
            bean->socks_http_type = Configs::SocksHttpBean::type_Socks4;
        } else {
            bean->socks_http_type = Configs::SocksHttpBean::type_Socks5;
        }
    }

    bean->username = ui->username->text();
    bean->password = ui->password->text();
    */
    return true;
}
