#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QMap>
#include <QString>
#include <QStringList>
#include <iostream>
#include <memory>

// Only enum storage, reflection declarations, logging and global preferences
// are inert adapters. Qt and all emitter/conversion bodies are production code.
using V2RAYTransportsEnum = QString;
struct VmessPacketEncodingsEnum : QString {
    using QString::operator=;
    explicit VmessPacketEncodingsEnum(int) {}
};
#define DECLARE_STORE_TYPE(...)
struct KCPExtra { KCP_FIELDS };
QStringList messages;
void MW_show_log(const QString &s) { messages.append(s); }
namespace Configs {
struct V2rayStreamSettings {
    STREAM_FIELDS
    void BuildStreamSettingsSingBox(QJsonObject *);
    HEADER_PAIRS
};
struct DataStore { bool skip_cert = false; } defaults;
auto dataStore = &defaults;
}
