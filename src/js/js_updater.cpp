#include <nekobox/js/js_updater.h>
#include <QJSEngine>
#include <QVariantMap>
#include <nekobox/global/HTTPRequestHelper.hpp>
#include <QFile>
#include <iostream>
#include "nekobox/configs/ConfigBuilder.hpp"
#include "nekobox/dataStore/Utils.hpp"
#include <nekobox/js/version.h>
#include <nekobox/sys/Settings.h>
#include <iostream>
#include <QString>
#include <QProcessEnvironment>
#include <iostream>
#include <memory>
#include <functional>
#include <QCoreApplication>
#include <QLocale>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

JsTextWriter::JsTextWriter()
: QObject() {}

bool JsTextWriter::open(const QVariant path) {
    close();
    if (path.canConvert<QString>()) {
        file.setFileName(path.toString());
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            stream.setDevice(&file);
            return true;
        }
    }
    return false;
}

void JsTextWriter::write(const QVariant text) {
    if (file.isOpen() && text.canConvert<QString>()) {
        stream << text.toString();
    }
}

void JsTextWriter::close() {
    if (file.isOpen()) {
        file.close();
    }
}

JsTextWriter::~JsTextWriter() {
    close();
}

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
void JsUpdaterWindow::unlock(){
    mutex.unlock();
};

QString JsUpdaterWindow::curdir(){
    QDir dir("temp");
    return dir.absolutePath();
}

void JsUpdaterWindow::open_url(const QVariant url){
    QDesktopServices::openUrl(QUrl(url.toString()));
}

QString JsUpdaterWindow::download(const QVariant value, const QVariant title, const QVariant skipIfExists){
    bool skip_if_exists = skipIfExists.toBool();
    QString url = value.toString();
    QString fileName = "./" + title.toString();
    QString ret;
    if (skip_if_exists){
        QFile asset(fileName);
        if (asset.exists()){
            return "";
        };
    }
    mutex.lock();
    emit download_signal(url, fileName, &ret);
    mutex.lock();
    mutex.unlock();
    return ret;
}

int JsUpdaterWindow::ask(const QVariant value, const QVariant title, const QVariant map){
    QString value1 = value.toString();
    QString title1 = title.toString();
    QStringList map1 = map.toStringList();
    int ret;
    mutex.lock();
    emit ask_signal(value1, title1, map1, &ret);
    mutex.lock();
    mutex.unlock();
    return ret;
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
        QString key = QString::fromUtf8(pair.first);
        QVariant value = QString::fromUtf8(pair.second);
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

void exposeGlobalVariables(QJSEngine * ctx){
    QJSValue optionsObject = ctx->newObject();
    QSettings settings = getGlobal();
    QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        optionsObject.setProperty(key, ctx->toScriptValue<QVariant>(settings.value(key)));
    }
    ctx->globalObject().setProperty("GlobalMap", optionsObject);
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
	
	jsFactory = ctx->newQMetaObject(&JsTextWriter::staticMetaObject);
    ctx->globalObject().setProperty("TextWriter", jsFactory);
	
    ctx->globalObject().setProperty("archive_name", "nekobox.zip");

    ctx->globalObject().setProperty("NKR_VERSION", NKR_VERSION);
    ctx->globalObject().setProperty("APPLICATION_DIR_PATH", root_directory);


    QString script;

    script = "var configs = ";
    script = script + QString::fromUtf8(Configs::dataStore->ToJsonBytes());
    ctx->evaluate(script);

    exposeGlobalVariables(ctx);
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

void getStringList(QJSEngine& engine, QString name, QStringList * value){
    QJSValue jsvalue = engine.globalObject().property(name);
    *value = jsArrayToQStringList(jsvalue);
}

bool jsRouteProfileGetter(
    JsUpdaterWindow * factory,
    QString * updater_js,
    QStringList * list,
    QMap<QString, QString> * names,
    std::function<QString(QString, QString *, bool*)> * func
    ){
    std::shared_ptr<QJSEngine> ctx = std::make_shared<QJSEngine>();
    if (!jsInit(ctx.get(), updater_js, factory)){
        return false;
    };

    auto global = ctx->globalObject();

    *list = jsArrayToQStringList(global.property("route_profiles"));
    *names = jsValueToQMap(global.property("route_profile_names"));
    *func = [ctx] (QString profile, QString * url, bool * proxy) -> QString {
            QJSValue jsFunction = ctx->globalObject().property("route_profile_get_json");
            if (jsFunction.isError()) {
               qWarning() <<  "Error in JavaScript code: " << jsFunction.toString();
               return "";
            }
            qDebug() << "jsFunction " << jsFunction.toString();
            QJSValueList args ;
            args << profile ;
            bool proxy_set = false;
            
            QJSValue result = jsFunction.call(args);
            if (result.isError()) {
               qWarning() << "Error calling JavaScript function: " << result.toString();
               return "";
            } else if (result.isArray()){
                QJSValue key = result.property(1);
                if (key.isString()){
                    *url = key.toString();
                }
                QJSValue pr = result.property(2);
                if (pr.isBool()){
                    *proxy = pr.toBool();
                    proxy_set = true;
                }
                result = result.property(0);
            }
            QString str = result.toString();
            if (!proxy_set){
                *proxy = str.toLower().startsWith("bypass");
            }
            return str;
    };
    return true;
}


bool jsUpdater( JsUpdaterWindow* factory,
                QString * updater_js,
                QString * search,
        /*        QString * assets_name,
                QString * release_download_url,
                QString * release_url,
                QString * release_note,
                QString * note_pre_release, */
                QString * archive_name,
                bool * is_newer,
                QStringList * args,
                bool allow_updater){
    QJSEngine ctx;
    ctx.globalObject().setProperty("search", *search);
    ctx.globalObject().setProperty("UpdaterExists", allow_updater);

    if (!jsInit(&ctx, updater_js, factory)){
        return false;
    };

    getString(ctx, "archive_name", archive_name);
    getString(ctx, "search", search);
    getBoolean(ctx, "is_newer", is_newer);
    getStringList(ctx, "updater_args", args);

    return true;
};
