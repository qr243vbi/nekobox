#include <include/js/js_updater.h>
#include <QJSEngine>
#include <QVariantMap>
#include <include/global/HTTPRequestHelper.hpp>
#include <QFile>
#include <iostream>
#include "include/configs/ConfigBuilder.hpp"
#include <include/js/version.h>

JsHTTPRequest::JsHTTPRequest(const QString& url)
: QObject(nullptr)
{
    init(url);
}

JsHTTPRequest::~JsHTTPRequest()
{
}

JsUpdaterWindow::JsUpdaterWindow(MessageQueue* msgq){
    queue = msgq;
}
void JsUpdaterWindow::print(const QVariant value){
    queue->Push(MessagePart{value.toString(), "", 4});
}
void JsUpdaterWindow::log(const QVariant value, const QVariant title){
    queue->Push(MessagePart{value.toString(), title.toString(), 1});
}
void JsUpdaterWindow::warning(const QVariant value, const QVariant title){
    queue->Push(MessagePart{value.toString(), title.toString(), 2});
}
void JsUpdaterWindow::info(const QVariant value, const QVariant title){
    queue->Push(MessagePart{value.toString(), title.toString(), 3});
}
QString JsUpdaterWindow::translate(const QVariant value){
    return QObject::tr(value.toString().toUtf8().constData());
}

void JsHTTPRequest::init(const QString& url)
{
    auto resp = NetworkRequestHelper::HttpGet(url);
    m_text = QString::fromUtf8(resp.data);
    m_error = resp.error;
    m_header.clear();

    for (const auto& pair : resp.header) {
        // Convert the key from QByteArray to QString using UTF-8
        QString key = QString::fromUtf8(pair.first);

        // Convert the value from QByteArray to QVariant
        // Here, we store it as a QByteArray within the QVariant.
        QVariant value = QString::fromUtf8(pair.second);

        // Insert the key-value pair into the map
        m_header.insert(key, value);
    }
}

QString JsHTTPRequest::get_text() const {
    return m_text;
}

QString JsHTTPRequest::get_error() const {
    return m_error;
}

QVariantMap JsHTTPRequest::get_header() const {
    return m_header;
}

void getString(QJSEngine& engine, QString name, QString * value){
    *value = engine.globalObject().property(name).toString();
}

void getBoolean(QJSEngine& engine, QString name, bool * value){
    *value = engine.globalObject().property(name).toBool();
}

bool jsUpdater( MessageQueue* queue,
                QString * updater_js,
                QString * search,
                QString * assets_name,
                QString * release_download_url,
                QString * release_url,
                QString * release_note,
                QString * note_pre_release,
                QString * archive_name,
                bool * is_newer){
    QJSEngine ctx;
    JsUpdaterWindow factory(queue);
    QJSValue jsFactory = ctx.newQObject(&factory);

    ctx.globalObject().setProperty("window", jsFactory);
    jsFactory = ctx.newQMetaObject(&JsHTTPRequest::staticMetaObject);

    ctx.globalObject().setProperty("HTTPResponse", jsFactory);
    ctx.globalObject().setProperty("search", *search);
    ctx.globalObject().setProperty("archive_name", "nekobox.zip");

    ctx.globalObject().setProperty("NKR_VERSION", NKR_VERSION);

    QString script;

    script = "var configs = ";
    script = script + QString::fromUtf8(Configs::dataStore->ToJsonBytes());
    ctx.evaluate(script);

    script = [&] { QFile f(":/updater.js"); return f.open(QIODevice::ReadOnly) ? QTextStream(&f).readAll() : QString(); }();
    ctx.evaluate(script);
    script = *updater_js;
    std::cout << ctx.evaluate(script).toString().toStdString() << std::endl;;

    getString(ctx, "assets_name", assets_name);
    getString(ctx, "release_download_url", release_download_url);
    getString(ctx, "release_url", release_url);
    getString(ctx, "release_note", release_note);
    getString(ctx, "note_pre_release", note_pre_release);
    getString(ctx, "archive_name", archive_name);
    getString(ctx, "search", search);
    getBoolean(ctx, "is_newer", is_newer);

    return true;
};
