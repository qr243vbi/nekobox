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
#include <QProcessEnvironment>
#include <iostream>
#include <memory>
#include <functional>
#include <QCoreApplication>
#include <QLocale>

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
bool JsUpdaterWindow::file_exists(const QVariant value){
    QString filepath = value.toString();
    if (filepath == ""){
        return false;
    }
    QFile file(filepath);
    return file.exists();
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

QString JsUpdaterWindow::get_locale() const {
    return QLocale().name();
}

void getString(QJSEngine& engine, QString name, QString * value){
    *value = engine.globalObject().property(name).toString();
}

void getBoolean(QJSEngine& engine, QString name, bool * value){
    *value = engine.globalObject().property(name).toBool();
}

void exposeEnvironmentVariables(QJSEngine *engine) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QJSValue envObject = engine->newObject();
    for (const QString &key : env.keys()) {
        envObject.setProperty(key, env.value(key));
    }
    engine->globalObject().setProperty("env", envObject);
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
    ctx->globalObject().setProperty("APPLICATION_DIR_PATH", QCoreApplication::applicationDirPath());

    QString script;

    script = "var configs = ";
    script = script + QString::fromUtf8(Configs::dataStore->ToJsonBytes());
    ctx->evaluate(script);

    exposeEnvironmentVariables(ctx);

    script = [&] { QFile f(":/updater.js"); return f.open(QIODevice::ReadOnly) ? QTextStream(&f).readAll() : QString(); }();
    ctx->evaluate(script);
    if (updater_js != nullptr){
        script = *updater_js;
        std::cout << ctx->evaluate(script).toString().toStdString() << std::endl;
    }

    return true;
}


QMap<QString, QString> jsValueToQMap(const QJSValue &jsValue) {
    QMap<QString, QString> resultMap;
    // Check if jsValue is an object
    if (jsValue.isObject()) {
        // Iterate through the keys
        for (const auto &key : jsValue.toVariant().toMap().asKeyValueRange()) {
            resultMap[key.first] = key.second.toString();           
        }
    }

    return resultMap;
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
    QMap<QString, QString> * names,
    std::function<QString(QString, QString *)> * func
    ){
    std::shared_ptr<QJSEngine> ctx = std::make_shared<QJSEngine>();
    if (!jsInit(ctx.get(), updater_js, factory)){
        return false;
    };

    auto global = ctx->globalObject();

    *list = jsArrayToQStringList(global.property("route_profiles"));
    *names = jsValueToQMap(global.property("route_profile_names"));
    *func = [ctx] (QString profile, QString * url) -> QString {
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
            } else if (result.isArray()){
                QJSValue key = result.property(1);
                if (key.isString()){
                    *url = key.toString();
                }
                return result.property(0).toString();
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
