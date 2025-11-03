#include <include/js/js_updater.h>
#include <QJSEngine>
#include <QVariantMap>
#include <include/global/HTTPRequestHelper.hpp>
#include <QFile>
#include <iostream>
#include "include/configs/ConfigBuilder.hpp"
#include <include/js/version.h>
#include <iostream>
#include <QString>

#include <iostream>
#include <memory>
#include <functional>

JsHTTPRequest::JsHTTPRequest(const QString& url): QObject(nullptr){
    init(url);
}

JsHTTPRequest::~JsHTTPRequest(){
}

JsUpdaterWindow::JsUpdaterWindow(){ //MainWindow* msgq){
 /*   queue = msgq;
    connect(this, &JsUpdaterWindow::log_signal, msgq, &MainWindow::on_log_show);
    connect(this, &JsUpdaterWindow::warning_signal, msgq, &MainWindow::on_warning_show);
    connect(this, &JsUpdaterWindow::info_signal, msgq, &MainWindow::on_info_show); */
}
void JsUpdaterWindow::print(const QVariant value){
    std::cout << value.toString().toStdString() << std::endl;
 //   queue->Push(MessagePart{value.toString(), "", 4});
}
void JsUpdaterWindow::log(const QVariant value, const QVariant title){
    QString value1 = value.toString();
    QString title1 = title.toString();
    emit log_signal(value1, title1);
 //   queue->Push(MessagePart{value.toString(), title.toString(), 1});
}
void JsUpdaterWindow::warning(const QVariant value, const QVariant title){
    QString value1 = value.toString();
    QString title1 = title.toString();
    emit warning_signal(value1, title1);
 //   queue->Push(MessagePart{value.toString(), title.toString(), 2});
}
void JsUpdaterWindow::info(const QVariant value, const QVariant title){
    QString value1 = value.toString();
    QString title1 = title.toString();
    emit info_signal(value1, title1);
 //   queue->Push(MessagePart{value.toString(), title.toString(), 3});
}
QString JsUpdaterWindow::translate(const QVariant value){
    return QObject::tr(value.toString().toUtf8().constData());
}
QString JsUpdaterWindow::get_jsdelivr_link(const QVariant value){
    return Configs::get_jsdelivr_link(value.toString());
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


bool jsInit(
    QJSEngine * ctx,
    QString * updater_js,
    JsUpdaterWindow * factory
){
    QJSValue jsFactory = ctx->newQObject(factory);

    ctx->globalObject().setProperty("window", jsFactory);
    jsFactory = ctx->newQMetaObject(&JsHTTPRequest::staticMetaObject);


    ctx->globalObject().setProperty("HTTPResponse", jsFactory);
    ctx->globalObject().setProperty("archive_name", "nekobox.zip");

    ctx->globalObject().setProperty("NKR_VERSION", NKR_VERSION);

    QString script;

    script = "var configs = ";
    script = script + QString::fromUtf8(Configs::dataStore->ToJsonBytes());
    ctx->evaluate(script);

    script = [&] { QFile f(":/updater.js"); return f.open(QIODevice::ReadOnly) ? QTextStream(&f).readAll() : QString(); }();
    ctx->evaluate(script);
    if (updater_js != nullptr){
        script = *updater_js;
        std::cout << ctx->evaluate(script).toString().toStdString() << std::endl;
    }
    return true;
}

QStringList jsArrayToQStringList(const QJSValue &jsArray) {
    QStringList stringList;

    if (jsArray.isArray()) {
        // Get the length of the array
        int size = jsArray.property("length").toInt();

        // Iterate over the elements
        for (int i = 0; i < size; ++i) {
            QJSValue element = jsArray.property(i);
            stringList.append(element.toString());
        }
    } else {
        qWarning() << "Provided QJSValue is not an array.";
    }

    return stringList;
}

bool jsRouteProfileGetter(
    JsUpdaterWindow * factory,
    QString * updater_js,
    QStringList * list,
    std::function<QString(QString)> * func
    ){
    std::shared_ptr<QJSEngine> ctx = std::make_shared<QJSEngine>();
    if (!jsInit(ctx.get(), updater_js, factory)){
        return false;
    };

    *list = jsArrayToQStringList(ctx->globalObject().property("route_profiles"));
    *func = [ctx] (QString profile) -> QString {
            QJSValue jsFunction = ctx->globalObject().property("route_profile_get_json");
            if (jsFunction.isError()) {
               qWarning() <<  "Error in JavaScript code: " << jsFunction.toString();
               return "";
            }
            qDebug() << "jsFunction " << jsFunction.toString();
            QJSValueList args ;
            args << profile ;

            QJSValue result = jsFunction.call(args);
            if (result.isError()) {
               qWarning() << "Error calling JavaScript function: " << result.toString();
               return "";
            }
            return result.toString();
    };
    return true;
}


bool jsUpdater( JsUpdaterWindow* factory,
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
    ctx.globalObject().setProperty("search", *search);
    if (!jsInit(&ctx, updater_js, factory)){
        return false;
    };

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
