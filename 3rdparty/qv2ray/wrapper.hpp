#pragma once

// Qv2ray wrapper

#include <QJsonDocument>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#define LOG(...) qDebug() << __VA_ARGS__
#define DEBUG(...) qDebug() << __VA_ARGS__
namespace Qv2ray {
    namespace base {
        template<typename... T>
        inline void log_internal(T... v) {}
    } // namespace base
} // namespace Qv2ray

// QString >> QJson
inline QJsonObject QString2QJsonObject(const QString &jsonString) {
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonString.toUtf8());
    QJsonObject jsonObject = jsonDocument.object();
    return jsonObject;
}

// QJson >> QString
inline QString 
QJsonObject2QString(const QJsonObject &jsonObject, bool compact) {
    return QJsonDocument(jsonObject).toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
}

#define JsonToString(a) QJsonObject2QString(a, false)
#define JsonFromString(a) QString2QJsonObject(a)

inline QString VerifyJsonString(const QString &source) {
    QJsonParseError error{};
    QJsonDocument doc = QJsonDocument::fromJson(source.toUtf8(), &error);
    Q_UNUSED(doc)

    if (error.error == QJsonParseError::NoError) {
        return "";
    } else {
        // LOG("WARNING: Json parse returns: " + error.errorString());
        return error.errorString();
    }
}

#define RED(obj)                                 \
    {                                            \
        auto _temp = obj->palette();             \
        _temp.setColor(QPalette::Text, Qt::red); \
        obj->setPalette(_temp);                  \
    }

#define BLACK(obj) obj->setPalette(QWidget::palette());
