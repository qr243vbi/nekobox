#include <nekobox/js/js_updater.h>
#include <quickjs.h>
#include <QVariantMap>
#include <QFile>
#include <iostream>
#include <nekobox/configs/ConfigBuilder.hpp>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/js/version.h>
#include <nekobox/sys/Settings.h>
#include <iostream>
#include <QString>
#include <QProcessEnvironment>
#include <iostream>
#include <functional>
#include <QCoreApplication>
#include <QLocale>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>



struct JS_RuntimeOpaque {
    JSClassID js_updater_window_class_id = 0;
    JSClassID js_text_writer_class_id = 0;
};

JS_RuntimeOpaque * get_runtime_opaque(JSRuntime * rt){
    void * ptr = JS_GetRuntimeOpaque(rt);
    if (ptr == nullptr){
        ptr = malloc(sizeof(JS_RuntimeOpaque));
        ((JS_RuntimeOpaque*)ptr)->js_text_writer_class_id = 0;
        ((JS_RuntimeOpaque*)ptr)->js_updater_window_class_id = 0;
        JS_SetRuntimeOpaque(rt, ptr);
    }
    return (JS_RuntimeOpaque*)ptr;
}

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
    return QDir::currentPath();
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


QString JsUpdaterWindow::get_locale() const {
    return QLocale().name();
}
#include "quickjs.h"
#include <QString>

void getString(JSContext* ctx, QString name, QString* value) {
    // 1. Get the global object
    JSValue global_obj = JS_GetGlobalObject(ctx);
    
    // 2. Fetch the property from the global object
    JSValue prop = JS_GetPropertyStr(ctx, global_obj, name.toUtf8().constData());
    
    // 3. Extract the value if it's a string (or convert it to a string string)
    const char* str = JS_ToCString(ctx, prop);
    if (str) {
        *value = QString::fromUtf8(str);
        JS_FreeCString(ctx, str); // Must free C strings allocated by JS_ToCString
    } else {
        *value = QString(); // Fallback if conversion fails
    }
    
    // 4. Critical cleanup: JS_GetGlobalObject and JS_GetPropertyStr both increment ref counts
    JS_FreeValue(ctx, prop);
    JS_FreeValue(ctx, global_obj);
}

void getBoolean(JSContext* ctx, QString name, bool* value) {
    // 1. Get the global object
    JSValue global_obj = JS_GetGlobalObject(ctx);
    
    // 2. Fetch the property from the global object
    JSValue prop = JS_GetPropertyStr(ctx, global_obj, name.toUtf8().constData());
    
    // 3. Evaluate the truthiness of the value (matches JavaScript rules)
    int bool_res = JS_ToBool(ctx, prop);
    *value = (bool_res > 0);
    
    // 4. Critical cleanup: Free the fetched handles
    JS_FreeValue(ctx, prop);
    JS_FreeValue(ctx, global_obj);
}

// Forward declaration so nested lists and maps can call it recursively
JSValue qvariant_to_jsvalue(JSContext* ctx, const QVariant& val);

JSValue qvariant_to_jsvalue(JSContext* ctx, const QVariant& val) {
    if (val.isNull() || !val.isValid()) {
        return JS_NULL;
    }

    // Capture the type ID of the specific variant payload
    auto typeId = val.typeId();

    // 1. Handle Nested Lists (QVariantList / QStringList)
    if (typeId == QMetaType::QVariantList || typeId == QMetaType::QStringList) {
        JSValue js_array = JS_NewArray(ctx);
        QVariantList list = val.toList();
        
        for (int i = 0; i < list.size(); ++i) {
            JSValue element = qvariant_to_jsvalue(ctx, list[i]);
            
            // JS_SetPropertyUint32 CONSUMES the element value automatically
            JS_SetPropertyUint32(ctx, js_array, i, element);
        }
        return js_array;
    }

    // 2. Handle Nested Maps (QVariantMap / QVariantHash)
    if (typeId == QMetaType::QVariantMap || typeId == QMetaType::QVariantHash) {
        JSValue js_object = JS_NewObject(ctx);
        QVariantMap map = val.toMap();
        
        QMapIterator<QString, QVariant> it(map);
        while (it.hasNext()) {
            it.next();
            JSValue prop_value = qvariant_to_jsvalue(ctx, it.value());
            
            // JS_SetPropertyStr CONSUMES the prop_value automatically
            JS_SetPropertyStr(ctx, js_object, it.key().toUtf8().constData(), prop_value);
        }
        return js_object;
    }

    // 3. Handle Primitives (Exact matching conditions from before)
    switch (typeId) {
        case QMetaType::Bool:
            return JS_NewBool(ctx, val.toBool());
        case QMetaType::Int:
        case QMetaType::UInt:
            return JS_NewInt32(ctx, val.toInt());
        case QMetaType::Double:
            return JS_NewFloat64(ctx, val.toDouble());
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            return JS_NewBigInt64(ctx, val.toLongLong());
        default:
            // Fallback default catch-all for native strings
            return JS_NewString(ctx, val.toString().toUtf8().constData());
    }
}


// Converts a JavaScript value from QuickJS into a strongly typed QVariant
QVariant jsvalue_to_qvariant(JSContext* ctx, JSValueConst val) {
    int tag = JS_VALUE_GET_TAG(val);
    if (JS_TAG_IS_FLOAT64(tag)) {
        double d;
        JS_ToFloat64(ctx, &d, val);
        return QVariant(d);
    }
    switch (tag) {
        case JS_TAG_BOOL:
            return QVariant((bool)JS_ToBool(ctx, val));
        case JS_TAG_INT: {
            int32_t i;
            JS_ToInt32(ctx, &i, val);
            return QVariant(i);
        }
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            return QVariant();
        case JS_TAG_STRING: {
            const char* str = JS_ToCString(ctx, val);
            QString qstr = QString::fromUtf8(str);
            JS_FreeCString(ctx, str);
            return QVariant(qstr);
        }
        case JS_TAG_OBJECT: {
            // For complex fallback (Arrays or Objects), serialize via JSON stringification
            JSValue json_obj = JS_GetGlobalObject(ctx);
            JSValue json_ns = JS_GetPropertyStr(ctx, json_obj, "JSON");
            JSValue stringify = JS_GetPropertyStr(ctx, json_ns, "stringify");
            JSValue json_str_val = JS_Call(ctx, stringify, json_ns, 1, &val);
            
            const char* c_str = JS_ToCString(ctx, json_str_val);
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray(c_str));
            
            JS_FreeCString(ctx, c_str);
            JS_FreeValue(ctx, json_str_val);
            JS_FreeValue(ctx, stringify);
            JS_FreeValue(ctx, json_ns);
            JS_FreeValue(ctx, json_obj);
            
            if (doc.isArray()) return doc.array().toVariantList();
            if (doc.isObject()) return doc.object().toVariantMap();
            return QVariant();
        }
        default:
            return QVariant();
    }
}


void exposeGlobalVariables(JSContext* ctx) {
    // 1. Create a clean plain target object
    JSValue optionsObject = JS_NewObject(ctx);
    
    QSettings settings = getGlobal();
    QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        // Generate the matching typed JSValue payload
        JSValue val = qvariant_to_jsvalue(ctx, settings.value(key));
        
        // JS_SetPropertyStr CONSUMES 'val'. No need to call JS_FreeValue(ctx, val) afterward.
        JS_SetPropertyStr(ctx, optionsObject, key.toUtf8().constData(), val);
    }
    
    // 2. Attach our map object directly into the global execution context
    JSValue global_obj = JS_GetGlobalObject(ctx);
    
    // JS_SetPropertyStr CONSUMES 'optionsObject' here for the global handle
    JS_SetPropertyStr(ctx, global_obj, "GlobalMap", optionsObject);
    
    // 3. Clean up the global pointer reference
    JS_FreeValue(ctx, global_obj);
}

void exposeEnvironmentVariables(JSContext* ctx) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // 1. Create a clean container object
    JSValue envObject = JS_NewObject(ctx);
    
    for (const QString &key : env.keys()) {
        // Stringify the value environment payload safely
        JSValue val = JS_NewString(ctx, env.value(key).toUtf8().constData());
        
        // JS_SetPropertyStr CONSUMES 'val'
        JS_SetPropertyStr(ctx, envObject, key.toUtf8().constData(), val);
    }
    
    // 2. Link the env object into the global scope
    JSValue global_obj = JS_GetGlobalObject(ctx);
    
    // JS_SetPropertyStr CONSUMES 'envObject'
    JS_SetPropertyStr(ctx, global_obj, "env", envObject);
    
    // 3. Clean up the global pointer reference
    JS_FreeValue(ctx, global_obj);
}


// Bridge macro helper to reduce redundant boilerplate code
#define GET_WINDOW_INSTANCE() \
    auto opaque = get_runtime_opaque(JS_GetRuntime(ctx)); \
    JsUpdaterWindow* self = (JsUpdaterWindow*)JS_GetOpaque(this_val, opaque->js_updater_window_class_id); \
    if (!self) return JS_ThrowTypeError(ctx, "Invalid instance context"); 

static JSValue js_window_print(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    if (argc < 1) return JS_UNDEFINED;
    self->print(jsvalue_to_qvariant(ctx, argv[0]));
    return JS_UNDEFINED;
}

static JSValue js_window_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    QVariant val = argc > 0 ? jsvalue_to_qvariant(ctx, argv[0]) : QVariant();
    QVariant title = argc > 1 ? jsvalue_to_qvariant(ctx, argv[1]) : QVariant();
    self->log(val, title);
    return JS_UNDEFINED;
}

static JSValue js_window_warning(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    QVariant msg = argc > 0 ? jsvalue_to_qvariant(ctx, argv[0]) : QVariant();
    QVariant title = argc > 1 ? jsvalue_to_qvariant(ctx, argv[1]) : QVariant();
    self->warning(msg, title);
    return JS_UNDEFINED;
}

static JSValue js_window_info(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    QVariant msg = argc > 0 ? jsvalue_to_qvariant(ctx, argv[0]) : QVariant();
    QVariant title = argc > 1 ? jsvalue_to_qvariant(ctx, argv[1]) : QVariant();
    self->info(msg, title);
    return JS_UNDEFINED;
}

static JSValue js_window_file_exists(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    if (argc < 1) return JS_FALSE;
    bool res = self->file_exists(jsvalue_to_qvariant(ctx, argv[0]));
    return JS_NewBool(ctx, res);
}

static JSValue js_window_ask(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    QVariant val = argc > 0 ? jsvalue_to_qvariant(ctx, argv[0]) : QVariant();
    QVariant title = argc > 1 ? jsvalue_to_qvariant(ctx, argv[1]) : QVariant();
    QVariant map = argc > 2 ? jsvalue_to_qvariant(ctx, argv[2]) : QVariant();
    int res = self->ask(val, title, map);
    return JS_NewInt32(ctx, res);
}

static JSValue js_window_get_jsdelivr_link(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    if (argc < 1) return JS_NULL;
    QString res = self->get_jsdelivr_link(jsvalue_to_qvariant(ctx, argv[0]));
    return JS_NewString(ctx, res.toUtf8().constData());
}

static JSValue js_window_translate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    if (argc < 1) return JS_NULL;
    QString res = self->translate(jsvalue_to_qvariant(ctx, argv[0]));
    return JS_NewString(ctx, res.toUtf8().constData());
}

static JSValue js_window_get_locale(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    QString res = self->get_locale();
    return JS_NewString(ctx, res.toUtf8().constData());
}

static JSValue js_window_download(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    QVariant url = argc > 0 ? jsvalue_to_qvariant(ctx, argv[0]) : QVariant();
    QVariant name = argc > 1 ? jsvalue_to_qvariant(ctx, argv[1]) : QVariant();
    QVariant skip = argc > 2 ? jsvalue_to_qvariant(ctx, argv[2]) : QVariant();
    QString res = self->download(url, name, skip);
    return JS_NewString(ctx, res.toUtf8().constData());
}

static JSValue js_window_curdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    QString res = self->curdir();
    return JS_NewString(ctx, res.toUtf8().constData());
}

static JSValue js_window_open_url(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WINDOW_INSTANCE();
    if (argc < 1) return JS_UNDEFINED;
    self->open_url(jsvalue_to_qvariant(ctx, argv[0]));
    return JS_UNDEFINED;
}

// Explicit definition map linking script method properties to C bridges
static const JSCFunctionListEntry js_updater_window_proto_funcs[] = {
    JS_CFUNC_DEF("print", 1, js_window_print),
    JS_CFUNC_DEF("log", 2, js_window_log),
    JS_CFUNC_DEF("warning", 2, js_window_warning),
    JS_CFUNC_DEF("info", 2, js_window_info),
    JS_CFUNC_DEF("file_exists", 1, js_window_file_exists),
    JS_CFUNC_DEF("ask", 3, js_window_ask),
    JS_CFUNC_DEF("get_jsdelivr_link", 1, js_window_get_jsdelivr_link),
    JS_CFUNC_DEF("translate", 1, js_window_translate),
    JS_CFUNC_DEF("get_locale", 0, js_window_get_locale),
    JS_CFUNC_DEF("download", 3, js_window_download),
    JS_CFUNC_DEF("curdir", 0, js_window_curdir),
    JS_CFUNC_DEF("open_url", 1, js_window_open_url),
};

JSValue newJsUpdaterWindow(JSContext* ctx, JsUpdaterWindow* factory) {
    auto rt = JS_GetRuntime(ctx);
    auto opaque = get_runtime_opaque(rt);
    // 1. Uniquely allocate class ID if it hasn't been configured yet
    if (opaque->js_updater_window_class_id == 0) {
        JS_NewClassID(rt, &opaque->js_updater_window_class_id);
        
        static JSClassDef js_updater_window_class_def = {
            "JSUpdaterWindow",
        };
        JS_NewClass(rt, opaque->js_updater_window_class_id, &js_updater_window_class_def);
    }

    // 2. Build the prototype object holding the functions array list
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_updater_window_proto_funcs, 
                               sizeof(js_updater_window_proto_funcs) / sizeof(JSCFunctionListEntry));

    // 3. Set the prototype into the context system definition registry
    JS_SetClassProto(ctx, opaque->js_updater_window_class_id, proto);

    // 4. Create the final instance object mapped directly to the prototype structure
    JSValue instance = JS_NewObjectProtoClass(ctx, proto, opaque->js_updater_window_class_id);
    
    // 5. Opaque mapping binds the raw C++ memory address pointer to this JS object structure
    JS_SetOpaque(instance, factory);

    return instance;
}


// The actual C constructor triggered by "new JsHTTPRequest(url)"
static JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
    // 1. Safety verification: Ensure it is called with 'new'
    if (JS_IsUndefined(new_target)) {
        return JS_ThrowTypeError(ctx, "JsHTTPRequest must be called with 'new'");
    }

    // 2. Parse the URL argument string passed from JavaScript
    QString url;
    if (argc > 0) {
        const char* url_cstr = JS_ToCString(ctx, argv[0]);
        if (url_cstr) {
            url = QString::fromUtf8(url_cstr);
            JS_FreeCString(ctx, url_cstr);
        }
    }

    // 3. Allocate and execute the underlying synchronous C++ Qt network payload
    auto request = NetworkRequestHelper::HttpGet(url);

    // 4. Create a plain clean target object container to snapshot the results
    JSValue obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) {
        return JS_EXCEPTION;
    }

    // 5. Populate text, error, and headers directly into the plain JS object properties
    QString text(request.data);
    JS_SetPropertyStr(ctx, obj, "text", JS_NewString(ctx, text.toUtf8().constData()));
    JS_SetPropertyStr(ctx, obj, "error", JS_NewString(ctx, request.error.toUtf8().constData()));
    
    // Transform and map the nested QVariantMap properties recursively
    JSValue header_obj = qvariant_to_jsvalue(ctx, QVariant::fromValue(QMapString2QVariantMap(request.header)));
    JS_SetPropertyStr(ctx, obj, "header", header_obj); // JS_SetPropertyStr consumes header_obj


    return obj;
}

JSValue newHTTPRequestClass(JSContext* ctx) {
    // 1. Create the fundamental empty base prototype object 
    JSValue proto = JS_NewObject(ctx);

    // 2. Create the constructor function using the standard JS_CFUNC_constructor configuration tag
    // Parameters: context, function pointer, name, argument count expectation, flag, magic value
    JSValue ctor = JS_NewCFunction2(ctx, js_http_request_constructor, "JsHTTPRequest", 1, JS_CFUNC_constructor, 0);

    // 3. Synchronize the standard constructor prototype binding link lines together
    JS_SetConstructor(ctx, ctor, proto);

    // 4. Balance references: JS_SetConstructor borrows references and increments internal values.
    // Free the local handle to proto, as ctor now retains a shared engine handle to it.
    JS_FreeValue(ctx, proto);

    // 5. Return the callable constructor function object back to the setup runtime environment
    return ctor;
}

// This finalizer automatically destroys the C++ heap memory when GC triggers
static void js_text_writer_finalizer(JSRuntime *rt, JSValue val) {
    auto opaque = get_runtime_opaque(rt);
    JsTextWriter* writer = (JsTextWriter*)JS_GetOpaque(val, opaque->js_text_writer_class_id);
    if (writer) {
        delete writer;
    }
}

// Forward declaration of your custom value extractor from earlier

#define GET_WRITER_INSTANCE() \
    auto rt = JS_GetRuntime(ctx);   \
    auto opaque = get_runtime_opaque(rt);   \
    JsTextWriter* self = (JsTextWriter*)JS_GetOpaque(this_val, opaque->js_text_writer_class_id); \
    if (!self) return JS_ThrowTypeError(ctx, "Invalid instance context");

static JSValue js_writer_open(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WRITER_INSTANCE();
    QVariant path = argc > 0 ? jsvalue_to_qvariant(ctx, argv[0]) : QVariant();
    bool result = self->open(path);
    return JS_NewBool(ctx, result);
}

static JSValue js_writer_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WRITER_INSTANCE();
    QVariant text = argc > 0 ? jsvalue_to_qvariant(ctx, argv[0]) : QVariant();
    self->write(text);
    return JS_UNDEFINED;
}

static JSValue js_writer_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    GET_WRITER_INSTANCE();
    self->close();
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_text_writer_proto_funcs[] = {
    JS_CFUNC_DEF("open", 1, js_writer_open),
    JS_CFUNC_DEF("write", 1, js_writer_write),
    JS_CFUNC_DEF("close", 0, js_writer_close),
};

// The constructor triggered by "new TextWriter()"
static JSValue js_text_writer_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
    auto rt = JS_GetRuntime(ctx);
    auto opaque = get_runtime_opaque(rt);
    if (JS_IsUndefined(new_target)) {
        return JS_ThrowTypeError(ctx, "TextWriter must be called with 'new'");
    }

    // 1. Resolve the prototype attached to the constructor
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    
    // 2. Allocate the empty JavaScript instance bound to our Class ID
    JSValue obj = JS_NewObjectProtoClass(ctx, proto, opaque->js_text_writer_class_id);
    JS_FreeValue(ctx, proto);
    
    if (JS_IsException(obj)) return JS_EXCEPTION;

    // 3. Instantiate the raw long-lived C++ class instance on the heap
    JsTextWriter* writer = new JsTextWriter();

    // 4. Attach the heap instance to the QuickJS object container
    JS_SetOpaque(obj, writer);

    return obj;
}


JSValue newTextWriter(JSContext* ctx) {
    auto rt = JS_GetRuntime(ctx);
    auto opaque = get_runtime_opaque(rt);
    // 1. One-time initialization of the class metadata structure definitions
    if (opaque->js_text_writer_class_id == 0) {
        JS_NewClassID(rt, &opaque->js_text_writer_class_id);
        
        static JSClassDef js_text_writer_class_def = {
            "TextWriter",
            js_text_writer_finalizer // Tied cleanly to memory tracking
        };
        JS_NewClass(rt, opaque->js_text_writer_class_id, &js_text_writer_class_def);
    }

    // 2. Create the prototype object container and map the shared function methods array
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_text_writer_proto_funcs, 
                               sizeof(js_text_writer_proto_funcs) / sizeof(JSCFunctionListEntry));

    // 3. Create the callable constructor function object wrapper with flag configuration
    JSValue ctor = JS_NewCFunction2(ctx, js_text_writer_constructor, "TextWriter", 0, JS_CFUNC_constructor, 0);

    // 4. Link the prototype and constructor chains seamlessly together
    JS_SetConstructor(ctx, ctor, proto);
    JS_SetClassProto(ctx, opaque->js_text_writer_class_id, proto); // Transfers ownership of proto to the class pipeline

    // 5. Yield the finalized constructor instance function back to the module wrapper scope
    return ctor;
}

#include "quickjs.h"
#include <QString>
#include <QFile>
#include <QTextStream>
#include <iostream>

// Helper to evaluate a script and log errors if it fails
static bool plain_js_eval(JSContext* ctx, const QString& source, const char* filename = "<eval>") {
    QByteArray bytes = source.toUtf8();
    JSValue res = JS_Eval(ctx, bytes.constData(), bytes.length(), filename, JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(res)) {
        // Retrieve and log the engine's error exception context
        JSValue exception = JS_GetException(ctx);
        const char* exception_str = JS_ToCString(ctx, exception);
        if (exception_str) {
            std::cerr << "[JS Error] " << exception_str << std::endl;
            JS_FreeCString(ctx, exception_str);
        }
        JS_FreeValue(ctx, exception);
        JS_FreeValue(ctx, res);
        return false;
    }
    
    JS_FreeValue(ctx, res);
    return true;
}

bool jsInit(
    JSContext* ctx,
    QString* updater_js,
    JsUpdaterWindow* factory
) {
    auto rt = JS_GetRuntime(ctx);
    auto opaque = get_runtime_opaque(rt);
    
    // 1. Fetch the Global Object container
    JSValue global_obj = JS_GetGlobalObject(ctx);

    // 2. Set "window" instance
    JSValue jsFactory = newJsUpdaterWindow(ctx, factory);
    JS_SetPropertyStr(ctx, global_obj, "window", jsFactory); // Consumes jsFactory

    // 3. Set Constructors ("HTTPResponse" and "TextWriter")
    jsFactory = newHTTPRequestClass(ctx);
    JS_SetPropertyStr(ctx, global_obj, "HTTPResponse", jsFactory); // Consumes jsFactory

    jsFactory = newTextWriter(ctx);
    JS_SetPropertyStr(ctx, global_obj, "TextWriter", jsFactory); // Consumes jsFactory

    // 4. Bind Global Primitive String Properties
    JS_SetPropertyStr(ctx, global_obj, "archive_name", JS_NewString(ctx, "nekobox.zip"));
    JS_SetPropertyStr(ctx, global_obj, "NKR_VERSION", JS_NewString(ctx, NKR_VERSION));
    JS_SetPropertyStr(ctx, global_obj, "APPLICATION_DIR_PATH", JS_NewString(ctx, root_directory.toUtf8().constData()));
    JS_SetPropertyStr(ctx, global_obj, "NKR_SOFTWARE_NAME", JS_NewString(ctx, software_name.toUtf8().constData()));

    // 5. Build and expose the "languages" array payload
    auto& languages = languageCodes();
    JSValue langArray = JS_NewArray(ctx);
    for (auto ptr : languages) {
        JSValue codeStr = JS_NewString(ctx, ptr->code.toUtf8().constData());
        // JS_SetPropertyUint32 explicitly consumes 'codeStr'
        JS_SetPropertyUint32(ctx, langArray, (uint32_t)ptr->index, codeStr);
    }
    JS_SetPropertyStr(ctx, global_obj, "languages", langArray); // Consumes langArray

    // 6. Clean up the global_obj pointer since we are done setting properties directly on it
    JS_FreeValue(ctx, global_obj);

    // 7. Inject "configs" variable using script evaluation string
    QString script = "var configs = " + QString::fromUtf8(Configs::dataStore->ToJsonBytes());
    if (!plain_js_eval(ctx, script, "configs_init.js")) {
        return false;
    }

    // 8. Run external maps and settings mapping procedures
    exposeGlobalVariables(ctx);
    exposeEnvironmentVariables(ctx);

    // 9. Load, execute, and compile the base "updater.js" payload resource asset
    script = [&] { 
        QFile f(":/updater.js"); 
        return f.open(QIODevice::ReadOnly) ? QTextStream(&f).readAll() : QString(); 
    }();
    if (!plain_js_eval(ctx, script, "updater.js")) {
        return false;
    }

    // 10. Execute the custom runtime user overrides script buffer if provided
    if (updater_js != nullptr) {
        script = *updater_js;
        QByteArray bytes = script.toUtf8();
        JSValue user_res = JS_Eval(ctx, bytes.constData(), bytes.length(), "user_updater.js", JS_EVAL_TYPE_GLOBAL);
        
        if (JS_IsException(user_res)) {
            JSValue exception = JS_GetException(ctx);
            const char* exception_str = JS_ToCString(ctx, exception);
            if (exception_str) {
                std::cout << "[User JS Exception] " << exception_str << std::endl;
                JS_FreeCString(ctx, exception_str);
            }
            JS_FreeValue(ctx, exception);
            JS_FreeValue(ctx, user_res);
            return false;
        }

        // Print the final stringified evaluations to the output stream
        const char* res_cstr = JS_ToCString(ctx, user_res);
        if (res_cstr) {
            std::cout << res_cstr << std::endl;
            JS_FreeCString(ctx, res_cstr);
        }
        JS_FreeValue(ctx, user_res);
    }

    return true;
}


// Forward declarations from your previous conversion code
bool jsInit(JSContext* ctx, QString* updater_js, JsUpdaterWindow* factory);
void getString(JSContext* ctx, QString name, QString* value);
void getBoolean(JSContext* ctx, QString name, bool* value);

// 1. Convert JSValue Object to QMap<QString, QString>
QMap<QString, QString> jsValueToQMap(JSContext* ctx, JSValueConst jsValue) {
    QMap<QString, QString> resultMap;
    
    if (JS_IsObject(jsValue) && !JS_IsFunction(ctx, jsValue)) {
        JSPropertyEnum* ptab = nullptr;
        uint32_t len = 0;
        
        // Retrieve all enumerable property keys from the object
        if (JS_GetOwnPropertyNames(ctx, &ptab, &len, jsValue, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) >= 0) {
            for (uint32_t i = 0; i < len; ++i) {
                JSValue key_atom_val = JS_AtomToValue(ctx, ptab[i].atom);
                const char* key_cstr = JS_ToCString(ctx, key_atom_val);
                
                if (key_cstr) {
                    QString qkey = QString::fromUtf8(key_cstr);
                    JSValue prop_val = JS_GetProperty(ctx, jsValue, ptab[i].atom);
                    const char* val_cstr = JS_ToCString(ctx, prop_val);
                    
                    if (val_cstr) {
                        resultMap[qkey] = QString::fromUtf8(val_cstr);
                        JS_FreeCString(ctx, val_cstr);
                    }
                    
                    JS_FreeValue(ctx, prop_val);
                    JS_FreeCString(ctx, key_cstr);
                }
                JS_FreeValue(ctx, key_atom_val);
                JS_FreeAtom(ctx, ptab[i].atom);
            }
            js_free(ctx, ptab);
        }
    }
    return resultMap;
}

// 2. Convert JSValue Array to QStringList
QStringList jsArrayToQStringList(JSContext* ctx, JSValueConst jsArray) {
    QStringList stringList;

    if (JS_IsArray(jsArray)) {
        JSValue len_prop = JS_GetPropertyStr(ctx, jsArray, "length");
        int64_t size = 0;
        JS_ToInt64(ctx, &size, len_prop);
        JS_FreeValue(ctx, len_prop);

        for (int64_t i = 0; i < size; ++i) {
            JSValue element = JS_GetPropertyUint32(ctx, jsArray, i);
            const char* str = JS_ToCString(ctx, element);
            if (str) {
                stringList.append(QString::fromUtf8(str));
                JS_FreeCString(ctx, str);
            }
            JS_FreeValue(ctx, element);
        }
    } else {
        qWarning() << "Provided QJSValue is not an array.";
    }

    return stringList;
}

// 3. Helper to fetch global string array lists
void getStringList(JSContext* ctx, QString name, QStringList* value) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue jsvalue = JS_GetPropertyStr(ctx, global_obj, name.toUtf8().constData());
    
    *value = jsArrayToQStringList(ctx, jsvalue);
    
    JS_FreeValue(ctx, jsvalue);
    JS_FreeValue(ctx, global_obj);
}

// 4. Convert jsRouteProfileGetter
bool jsRouteProfileGetter(
    JsUpdaterWindow* factory,
    QString* updater_js,
    QStringList* list,
    QMap<QString, QString>* names,
    std::function<QString(QString, QString*, bool*)>* func
) {
    // Creating runtime and context explicitly
    JSRuntime* rt = JS_NewRuntime();
    if (!rt) return false;
    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        return false;
    }

    if (!jsInit(ctx, updater_js, factory)) {
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return false;
    }

    JSValue global = JS_GetGlobalObject(ctx);

    JSValue route_profiles = JS_GetPropertyStr(ctx, global, "route_profiles");
    *list = jsArrayToQStringList(ctx, route_profiles);
    JS_FreeValue(ctx, route_profiles);

    JSValue route_profile_names = JS_GetPropertyStr(ctx, global, "route_profile_names");
    *names = jsValueToQMap(ctx, route_profile_names);
    JS_FreeValue(ctx, route_profile_names);

    JS_FreeValue(ctx, global);

    // Capture context and runtime pointers in lambda. 
    // WARNING: Caller must invoke this function before destroying the parent application loops.
    *func = [ctx, rt](QString profile, QString* url, bool* proxy) -> QString {
        JSValue global_obj = JS_GetGlobalObject(ctx);
        JSValue jsFunction = JS_GetPropertyStr(ctx, global_obj, "route_profile_get_json");
        JS_FreeValue(ctx, global_obj);

        if (JS_IsException(jsFunction) || !JS_IsFunction(ctx, jsFunction)) {
            qWarning() << "Error in JavaScript code: Function route_profile_get_json not found.";
            JS_FreeValue(ctx, jsFunction);
            return "";
        }

        bool proxy_set = false;
        JSValue arg = JS_NewString(ctx, profile.toUtf8().constData());
        
        // Invoke the JS function
        JSValue result = JS_Call(ctx, jsFunction, JS_UNDEFINED, 1, &arg);
        JS_FreeValue(ctx, arg);
        JS_FreeValue(ctx, jsFunction);

        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(ctx);
            const char* exc_str = JS_ToCString(ctx, exception);
            qWarning() << "Error calling JavaScript function: " << exc_str;
            JS_FreeCString(ctx, exc_str);
            JS_FreeValue(ctx, exception);
            JS_FreeValue(ctx, result);
            return "";
        }

        QString return_str;

        if (JS_IsArray(result)) {
            JSValue key = JS_GetPropertyUint32(ctx, result, 1);
            if (JS_VALUE_GET_TAG(key) == JS_TAG_STRING) {
                const char* url_cstr = JS_ToCString(ctx, key);
                *url = QString::fromUtf8(url_cstr);
                JS_FreeCString(ctx, url_cstr);
            }
            JS_FreeValue(ctx, key);

            JSValue pr = JS_GetPropertyUint32(ctx, result, 2);
            if (JS_VALUE_GET_TAG(pr) == JS_TAG_BOOL) {
                *proxy = (bool)JS_ToBool(ctx, pr);
                proxy_set = true;
            }
            JS_FreeValue(ctx, pr);

            JSValue base_res = JS_GetPropertyUint32(ctx, result, 0);
            const char* base_cstr = JS_ToCString(ctx, base_res);
            return_str = QString::fromUtf8(base_cstr);
            JS_FreeCString(ctx, base_cstr);
            JS_FreeValue(ctx, base_res);
        } else {
            const char* res_cstr = JS_ToCString(ctx, result);
            return_str = QString::fromUtf8(res_cstr);
            JS_FreeCString(ctx, res_cstr);
        }

        JS_FreeValue(ctx, result);

        if (!proxy_set) {
            *proxy = return_str.toLower().startsWith("bypass");
        }
        return return_str;
    };

    // Note: Do not free ctx/rt here as the lambda still relies on them. 
    // Manage cleanup depending on where this closure instance is safely destroyed.
    return true;
}

// 5. Convert jsUpdater
bool jsUpdater(JsUpdaterWindow* factory,
               QString* updater_js,
               QString* search,
               QString* archive_name,
               bool* is_newer,
               QStringList* args,
               bool allow_updater,
               bool* keep_running,
               bool button_clicked
) {
    JSRuntime* rt = JS_NewRuntime();
    if (!rt) return false;
    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        return false;
    }

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "search", JS_NewString(ctx, search->toUtf8().constData()));
    JS_SetPropertyStr(ctx, global_obj, "ButtonClicked", JS_NewBool(ctx, button_clicked));
    JS_SetPropertyStr(ctx, global_obj, "UpdaterExists", JS_NewBool(ctx, allow_updater));
    JS_FreeValue(ctx, global_obj);

    if (!jsInit(ctx, updater_js, factory)) {
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return false;
    }

    getString(ctx, "archive_name", archive_name);
    getString(ctx, "search", search);
    getBoolean(ctx, "is_newer", is_newer);
    getBoolean(ctx, "keep_running", keep_running);
    getStringList(ctx, "updater_args", args);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return true;
}

// 6. Convert jsAnnouncementMessage
bool jsAnnouncementMessage(
    JsUpdaterWindow* factory,
    QString* updater_js,
    bool first_start
) {
    JSRuntime* rt = JS_NewRuntime();
    if (!rt) return false;
    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        return false;
    }

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "first_start", JS_NewBool(ctx, first_start));
    JS_FreeValue(ctx, global_obj);

    if (!jsInit(ctx, updater_js, factory)) {
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return false;
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return true;
}
