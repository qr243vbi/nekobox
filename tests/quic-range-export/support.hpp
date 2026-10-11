#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <iostream>
#include <memory>
#include <utility>
// Only enum storage and bean/entity ownership are test stand-ins.
// Actual field declarations/defaults and complete constructor/exporter/helpers
// are extracted by the runner. No importer, storage, core, app or network runs.
struct NetworkEnum : QString {
    using QString::operator=;
    int value = 0;
    explicit NetworkEnum(int) {}
};
namespace Configs {
struct ProxyEntity { ENTITY_FIELDS };
struct AbstractBean {
    ProxyEntity *entity;
    AbstractBean(ProxyEntity *value, int) : entity(value) {}
};
}
