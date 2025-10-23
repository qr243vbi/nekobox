#ifndef JS_UPDATER
#define JS_UPDATER

#include <set>
#include <iostream>
#include <quickjs.h>
#include <string>

#ifndef MAINWINDOW_H_DEFINED
#include "global/HTTPRequestHelper.hpp"
#include "configs/ConfigBuilder.hpp"
#include "ui/mainwindow.h"
#include <QString>
#endif

struct QueuePart{
    QString title, message;
    int type;
};

template <typename T>
class BlockingQueue {
public:
    void push(const T &item) {
        QMutexLocker locker(&mutex);
        queue.enqueue(item);
     //   std::cout << " PUSH " << std::endl;
        cond.wakeOne();  // wake a waiting consumer
    }

    T pop() {
        QMutexLocker locker(&mutex);
        while (queue.isEmpty()) {
            cond.wait(&mutex);  // block until data available
        }
        return queue.dequeue();
    }

private:
    QQueue<T> queue;
    QMutex mutex;
    QWaitCondition cond;
};


// Simple C function that adds two numbers
JSValue HttpGet(JSContext *ctx, JSValueConst this_val,
                    int argc, JSValueConst *argv){
    const char * name = nullptr;

    if (argc > 0){
        return JS_UNDEFINED;
    }

    JSValueConst url = argv[0];

    return url;
}



#include <math.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

/* Point Class */

typedef HTTPResponse JSPointData;

static JSClassID js_point_class_id;

static void js_point_finalizer(JSRuntime *rt, JSValue val)
{
    JSPointData *s = (JSPointData*)JS_GetOpaque(val, js_point_class_id);
    /* Note: 's' can be NULL in case JS_SetOpaque() was not called */
    js_free_rt(rt, s);
}

static JSValue js_point_ctor(JSContext *ctx,
                             JSValue new_target,
                             int argc, JSValue *argv)
{
    JSPointData *s;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    const char * data;
    if (argc <= 0 || (data = JS_ToCString(ctx, argv[0])) == nullptr)
        goto fail;
    /* using new_target to get the prototype is necessary when the
     *      class is extended. */
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_point_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    s = (JSPointData*)js_mallocz(ctx, sizeof(*s));
    *s = NetworkRequestHelper::HttpGet(data);
    JS_SetOpaque(obj, s);
    return obj;
    fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue getHeaders(JSContext * ctx,  QList<QPair<QByteArray, QByteArray>>& list){
    JSValue obj = JS_NewObject(ctx);

    for (const auto& pair : list) {
        const QByteArray& key = pair.first;
        const QByteArray& value = pair.second;

        // Convert key and value to JS strings
        JSValue jsValue = JS_NewString(ctx, QString::fromUtf8(value).toUtf8().constData());

        // Set property on the JS object
        JS_SetPropertyStr(ctx, obj, QString::fromUtf8(key).toUtf8().constData(), jsValue);

        // Free atom for the key (internally used)
        JS_FreeValue(ctx, jsValue);
    }

    return obj;
}

static JSValue js_point_get_xy(JSContext *ctx, JSValue this_val, int magic)
{
    JSPointData *s = nullptr;
    if (!(magic < 0 && magic > 2)) {
        s = (JSPointData*)JS_GetOpaque(this_val, js_point_class_id);
    }
    if (magic == 1){
        return JS_NewString(ctx, s->error.toUtf8().constData());
    } else if (magic == 0){
        return JS_NewString(ctx, QString::fromUtf8(s->data).toUtf8().constData());
    } else if (magic == 2){
        return getHeaders(ctx, s->header);
    }
    return JS_UNDEFINED;
}


static void jsObjectToList(JSContext* ctx, JSValue& obj, QList<QPair<QByteArray, QByteArray>>& result) {
    result.clear();

    // Ensure it's an object
    if (!JS_IsObject(obj))
        return;

    // Get property names
    JSPropertyEnum* props;
    uint32_t len;

    if (JS_GetOwnPropertyNames(ctx, &props, &len, obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0)
        return;

    for (uint32_t i = 0; i < len; i++) {
        // Get key name
        JSAtom atom = props[i].atom;
        const char* keyCStr = JS_AtomToCString(ctx, atom);
        QByteArray key(keyCStr);
        JS_FreeCString(ctx, keyCStr);

        // Get value
        JSValue val = JS_GetProperty(ctx, obj, atom);

        const char* valCStr = JS_ToCString(ctx, val);
        if (valCStr) {
            QByteArray value(valCStr);
            result.append(qMakePair(key, value));
            JS_FreeCString(ctx, valCStr);
        }

        JS_FreeValue(ctx, val);  // Free the JSValue
        JS_FreeAtom(ctx, atom);  // Free the key atom
    }

    js_free(ctx, props);  // Free the property array

    return;
}

static JSValue js_point_set_xy(JSContext *ctx, JSValue this_val, JSValue val, int magic)
{
    JSPointData *s = nullptr;
    const char * ch;
    if (!(magic < 0 && magic > 2)) {
        s = (JSPointData*)JS_GetOpaque(this_val, js_point_class_id);
        if (magic == 2){
            jsObjectToList(ctx, val, s->header);
            return val;
        }
        ch = JS_ToCString(ctx, val);
    } else {
        return JS_UNDEFINED;
    }
    if (magic == 1){
        s->error = QString::fromUtf8(ch);
    } else if (magic == 0){
        s->data = QByteArray(ch);
    }
    JS_FreeCString(ctx, ch);
    return val;
}

static JSClassDef js_point_class = {
    "HTTPResponse",
    js_point_finalizer
};

static const JSCFunctionListEntry js_point_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("text", js_point_get_xy, js_point_set_xy, 0),
    JS_CGETSET_MAGIC_DEF("error", js_point_get_xy, js_point_set_xy, 1),
    JS_CGETSET_MAGIC_DEF("headers", js_point_get_xy, js_point_set_xy, 2),
   // JS_CFUNC_DEF("header", 0, js_point_norm),
};

static JSValue js_point_init(JSContext *ctx)
{
    JSValue point_proto, point_class;
    JSRuntime *rt = JS_GetRuntime(ctx);

    /* create the Point class */
    JS_NewClassID(rt, &js_point_class_id);
    JS_NewClass(rt, js_point_class_id, &js_point_class);

    point_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, point_proto, js_point_proto_funcs, countof(js_point_proto_funcs));

    point_class = JS_NewCFunction2(ctx, js_point_ctor, "HTTPResponse", 1, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, point_class, point_proto);
    JS_SetClassProto(ctx, js_point_class_id, point_proto);

    return point_class;
}

// Define a global function that prints a greeting
static JSValue print_int(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Check if an argument is provided
    if (argc > 0) {
        const char * arg;
        for (int i = 1; i < argc; i ++){
            arg = JS_ToCString(ctx, argv[argc-1]);
            if (arg != nullptr){
                fprintf(stderr, "%s\n", arg);
                JS_FreeCString(ctx, arg);
            }
        }
        arg = JS_ToCString(ctx, argv[argc-1]);
        if (arg != nullptr){
            fprintf(stderr, "%s\n", arg);
            JS_FreeCString(ctx, arg);
        }
    } else {
        fprintf(stderr, "\n");
    }

    return JS_UNDEFINED; // Return undefined
}

static std::set<size_t> MainWindowSet;


// Define a global function that shows a log
static JSValue is_window(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Check if an argument is provided
    if (argc > 0) {
        uint64_t win;
        JS_ToBigUint64(ctx, &win, argv[0]);
        bool iswin = false;
        if (MainWindowSet.contains((size_t)win)){
            iswin = true;
        }
        return JS_NewBool(ctx, iswin);
    }

    return JS_UNDEFINED; // Return undefined
}

// Define a global function that shows a log
static JSValue print_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Check if an argument is provided
    if (argc > 1) {
        uint64_t win;
        JS_ToBigUint64(ctx, &win, argv[0]);
        if (MainWindowSet.contains((size_t)win)){
            const char * arg = JS_ToCString(ctx, argv[1]);
            if (arg != nullptr){
                ((BlockingQueue<QueuePart>*)reinterpret_cast<void*>((size_t)win))->push(QueuePart{
                    "",
                    arg,
                    1
                });
                JS_FreeCString(ctx, arg);
            }
        }
    }

    return JS_UNDEFINED; // Return undefined
}

static JSValue show_message(JSContext *ctx,
                            JSValue new_target,
                            int argc, JSValue *argv, bool isWarning)
{
    if (argc > 1){
        const char * title, * message;
        if (argc > 2){
            title = JS_ToCString(ctx, argv[2]);
        } else {
            title = "Updater";
        }
        message = JS_ToCString(ctx, argv[1]);
        uint64_t pointer;
        JS_ToBigUint64(ctx, &pointer, argv[0]);
        if (MainWindowSet.contains((size_t)pointer)){
            if (isWarning){
                ((BlockingQueue<QueuePart>*)reinterpret_cast<void*>((size_t)pointer))->push(QueuePart{
                    title,
                    message,
                    2
                });
      //          window->showMessageBoxWarning(title, message);
            } else {
                ((BlockingQueue<QueuePart>*)reinterpret_cast<void*>((size_t)pointer))->push(QueuePart{
                    title,
                    message,
                    3
                });
        //        window->showMessageBoxInfo(title, message);
            }
        }
    }
    return JS_UNDEFINED;
}

static JSValue show_warning(JSContext *ctx,
                            JSValue new_target,
                            int argc, JSValue *argv)
{
    return show_message(ctx, new_target, argc, argv, true);
}

static JSValue show_info(JSContext *ctx,
                            JSValue new_target,
                            int argc, JSValue *argv)
{
    return show_message(ctx, new_target, argc, argv, false);
}

static JSValue translate_message(JSContext *ctx,
                         JSValue new_target,
                         int argc, JSValue *argv)
{
    if (argc <= 0){
        return JS_UNDEFINED;
    } else {
        return JS_NewString(ctx, QObject::tr(JS_ToCString(ctx, argv[0])).toUtf8().constData());
    };
}

static const JSCFunctionListEntry js_global_funcs[] = {
    JS_CFUNC_DEF("print", 1, print_int),
    JS_CFUNC_DEF("log", 2, print_log),
    JS_CFUNC_DEF("iswindow", 1, is_window),
    JS_CFUNC_DEF("warning", 3, show_warning),
    JS_CFUNC_DEF("info", 3, show_info),
    JS_CFUNC_DEF("translate", 3, translate_message),
};

void getString(JSContext * ctx, /*JSValue & global_obj,*/ std::string name, QString * value){
    JSValue str = JS_Eval(ctx, name.c_str(), name.size(), "updater.js", JS_EVAL_TYPE_GLOBAL);
    if (!JS_IsUndefined(str)){
        const char * strval =  JS_ToCString(ctx, str);
        *value = strval;
        JS_FreeCString(ctx, strval);
        JS_FreeValue(ctx, str);
    }
}

void getBoolean(JSContext * ctx, std::string name, bool * value){
    std::string expr = "(";
    expr = expr + name + ") ? \"A\" : \"B\"";
    QString str = "";
    getString(ctx, expr, &str);
    std::cout << expr << std::endl;
    std::cout << str.toStdString() << std::endl;
    *value = (str == "A");
}


bool jsUpdater(BlockingQueue<QueuePart> * window,
               QString * file,
               QString * search,
               QString * assets_name,
               QString * release_download_url,
               QString * release_url,
               QString * release_note,
               QString * note_pre_release,
               QString * archive_name,
               bool * is_newer
){

    size_t pointer = 0;
    if (window != nullptr ) {
        pointer = reinterpret_cast<size_t>((void*)window);
        MainWindowSet.insert(pointer);
    }

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue result;
    std::string script;
    // Set the Point class in the global object
    JSValue global_obj = JS_GetGlobalObject(ctx);

    JS_SetPropertyStr(ctx, global_obj, "HTTPResponse", js_point_init(ctx));
    JS_SetPropertyStr(ctx, global_obj, "window", JS_NewBigUint64(ctx, pointer));
    JS_SetPropertyStr(ctx, global_obj, "search", JS_NewString(ctx, search->toUtf8().constData()));
    JS_SetPropertyStr(ctx, global_obj, "archive_name", JS_NewString(ctx, "nekobox.zip"));
    JS_SetPropertyFunctionList(ctx, global_obj, js_global_funcs, countof(js_global_funcs));
#ifndef NKR_VERSION
    JS_SetPropertyStr(ctx, global_obj, "NKR_VERSION", JS_NewString(ctx, ""));
#else
    JS_SetPropertyStr(ctx, global_obj, "NKR_VERSION", JS_NewString(ctx, NKR_VERSION));
#endif

    // Run a JavaScript script to test the Point class
//    const char *script = "let p = new HTTPResponse('https://mail.ru'); print('error: '); print(p.error); print('text: '); print(p.text); print(' : ')";



    script = "let configs = ";
    script += Configs::dataStore->ToJsonBytes().toStdString();


    result = JS_Eval(ctx, script.c_str(), script.size(), "updater.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)){
        goto fail;
    }

    script = "function httpget(url){ let p = new HTTPResponse(url); let er = p.error; if(er) { warning(window, translate('Update'), translate('Requesting update error: %1').replace('%1', p.error) ); }; return p; }";
    result = JS_Eval(ctx, script.c_str(), script.size(), "updater.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)){
        goto fail;
    }

    script = file->toUtf8().toStdString();
    result = JS_Eval(ctx, script.c_str(), script.size(), "updater.js", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        fail:
        if (pointer != 0) {
            MainWindowSet.erase(pointer);
        }
        JSValue exception = JS_GetException(ctx);
        const char *error = JS_ToCString(ctx, exception);
        fprintf(stderr, "Exception: %s\n", error);

        ((BlockingQueue<QueuePart>*)reinterpret_cast<void*>(pointer))->push(QueuePart{
            "Update",
            QObject::tr("updater.js throws exception %1, the script is: %2").arg(error, script.c_str()),
            2
        });
        JS_FreeCString(ctx, error);
        JS_FreeValue(ctx, exception);
        goto ret1;
    }

    JS_FreeValue(ctx, result);

    getString(ctx, /*global_obj,*/ "assets_name", assets_name);
    getString(ctx, /*global_obj,*/ "release_download_url", release_download_url);
    getString(ctx, /*global_obj,*/ "release_url", release_url);
    getString(ctx, /*global_obj,*/ "release_note", release_note);
    getString(ctx, /*global_obj,*/ "note_pre_release", note_pre_release);
    getString(ctx, /*global_obj,*/ "archive_name", archive_name);
    getString(ctx, /*global_obj,*/ "search", search);
    getBoolean(ctx, "is_newer", is_newer);
    /*
     assets_name = JS_ToCString(ctx, );
     release_download_url,
     release_url,
     release_note,
     note_pre_release
*/
    // Clean up
    ret1:
    JS_FreeValue(ctx, global_obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    if (pointer != 0) {
        MainWindowSet.erase(pointer);
    }

    return true;
}

#endif
