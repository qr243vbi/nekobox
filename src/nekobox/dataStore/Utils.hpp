#pragma once

#include "3rdparty/qv2ray/wrapper.hpp"
#include <functional>
#include <memory>
#include <QObject>
#include <QString>
#include <QUrlQuery>
#include <QJsonArray>
#include <QVariantMap>
#include <QDebug>
#include <QFile>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif
//


#ifndef ADD_MAP

#define ITEM_TYPE(X) itemType::type_##X 
#define MAP_BODY \
static ConfJsMapStat ptr ; \
static bool init = false;               \
if (init) return ptr;  

#define DECL_MAP(X)  ConfJsMap X::_map() {   \
MAP_BODY


#define INIT_MAP_1 virtual ConfJsMap _map() override {   \
MAP_BODY

#define INIT_MAP INIT_MAP_1              \
ptr = AbstractBean::_map();   

#define STOP_MAP \
init = true;     \
return ptr;      \
}

#define ADD_MAP(X, Y, B) _put(ptr, X, &this->Y, ITEM_TYPE(B))
#endif

#ifndef NKR_VERSION
inline QString software_version;
const char * getSoftwareVersion();
#define NKR_VERSION getSoftwareVersion()
#define NKR_DYNAMIC_VERSION dynamic
#endif

inline QString software_build_date;
inline QString software_name;
inline QString software_core_name;

#define root_directory  QApplication::applicationDirPath()
#define software_path   QApplication::applicationFilePath()

// MainWindow functions
inline std::function<void(QString)> MW_show_log;
inline std::function<void(QString, QString)> MW_dialog_message;

// Dispatchers

class QThread;
inline QThread *DS_cores;

// Timers

class QTimer;
inline QTimer *TM_auto_update_subsctiption;
inline std::function<void(int)> TM_auto_update_subsctiption_Reset_Minute;

// String

#define FIRST_OR_SECOND(a, b) a.isEmpty() ? b : a

inline const QString UNICODE_LRO = QString::fromUtf8(QByteArray::fromHex("E280AD"));

QString SubStrBefore(QString str, const QString &sub);

QString SubStrAfter(QString str, const QString &sub);

QString QStringList2Command(const QStringList &list);

QStringList SplitLines(const QString &_string);

QStringList SplitLinesSkipSharp(const QString &_string, int maxLine = 0);

// Base64

QByteArray DecodeB64IfValid(const QString &input, QByteArray::Base64Options options = QByteArray::Base64Option::Base64Encoding);

// URL

class QUrlQuery;

#define GetQuery(url) QUrlQuery((url).query(QUrl::ComponentFormattingOption::FullyDecoded));

QString GetQueryValue(const QUrlQuery &q, const QString &key, const QString &def = "");

int GetQueryIntValue(const QUrlQuery &q, const QString &key, int def = 0);
QStringList GetQueryListValue(const QUrlQuery &q, const QString &key);

QString GetRandomString(int randomStringLength);

void MoveDirToTrash(const QString &path);

quint64 GetRandomUint64();

QJsonArray QListInt2QJsonArray(const QList<int> &list);

QJsonArray QListStr2QJsonArray(const QList<QString> &list);

QList<int> QJsonArray2QListInt(const QJsonArray &arr);

QJsonObject QMapString2QJsonObject(const QMap<QString,QString> &mp);

#define QJSONARRAY_ADD(arr, add) \
    for (const auto &a: (add)) { \
        (arr) += a;              \
    }
#define QJSONOBJECT_COPY(src, dst, key) \
    if (src.contains(key)) dst[key] = src[key];
#define QJSONOBJECT_COPY2(src, dst, src_key, dst_key) \
    if (src.contains(src_key)) dst[dst_key] = src[src_key];

QList<QString> QJsonArray2QListString(const QJsonArray &arr);

QJsonArray QString2QJsonArray(const QString& str);

// Files

QByteArray ReadFile(const QString &path);
QByteArray ReadFile( QFile &path);

QString ReadFileText(const QString &path);
QString ReadFileText( QFile &path);

bool WriteFileText(const QString &path, const QString &text);
bool WriteFileText(QFile &file, const QString &notes);

bool WriteFile(const QString &path, const QByteArray &text);
bool WriteFile(QFile &file, const QByteArray &notes);
// Validators

bool IsIpAddress(const QString &str);

bool IsIpAddressV4(const QString &str);

bool IsIpAddressV6(const QString &str);


// [2001:4860:4860::8888] -> 2001:4860:4860::8888
QString UnwrapIPV6Host(QString &str);

// [2001:4860:4860::8888] or 2001:4860:4860::8888 -> [2001:4860:4860::8888]
QString WrapIPV6Host(QString &str);

QString DisplayAddress(QString serverAddress, int serverPort);

QString DisplayDest(const QString& dest, QString domain);

// Format & Misc

int MkPort();

QString DisplayTime(long long time, int formatType = 0);

QString ReadableSize(const qint64 &size);

bool InRange(unsigned x, unsigned low, unsigned high);

bool IsValidPort(int port);

void runOnNewThread(const std::function<void()> &callback);

void runOnThread(const std::function<void()> &callback, QObject *parent);

void AddQueryString(QUrlQuery & query, const QString & name, const QString & value);

void AddQueryStringList( QUrlQuery & query, const QString & name, const QStringList & value);

void AddQueryMap( QUrlQuery & query, const QString &  name, const QVariantMap & value);

void AddQueryInt( QUrlQuery & query, const QString &  name, int value);

void AddQueryNatural( QUrlQuery & query, const QString & name, int value);

QStringList GetQueryListValue(const QUrlQuery &q, const QString &key);

QVariantMap QString2QMap(const QString &key);

QString QMap2QString(const QVariantMap &map);

QVariantMap GetQueryMapValue(const QUrlQuery &q, const QString &key);
