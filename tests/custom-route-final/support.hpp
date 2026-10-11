// Explicit test-only boundaries. JSON, strings, URLs and sanitizer helpers are real.
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

namespace Configs {
struct Settings {
    int core_box_clash_api = 0;
    QString core_box_clash_listen_addr = "127.0.0.1";
    QString core_box_clash_api_secret;
};
Settings settings;
Settings *dataStore = &settings;
struct CustomBean {
    QString core;
    QString config_simple;
};
struct ProxyEntity {
    QString name;
    QString display_type;
    std::shared_ptr<Configs::CustomBean> storage = std::make_shared<Configs::CustomBean>();
    std::shared_ptr<Configs::CustomBean> CustomBean() { return storage; }
    std::shared_ptr<Configs::CustomBean> unlock(const std::shared_ptr<Configs::CustomBean> &value) { return value; }
};
struct ProfileManager {
    static std::shared_ptr<ProxyEntity> NewProxyEntity(const QString &) {
        return std::make_shared<ProxyEntity>();
    }
};
}
