#include "nekobox/dataStore/Utils.hpp"

#include "3rdparty/base64.h"
#include "3rdparty/QThreadCreateThread.hpp"
#include <QDir>
#include <QFileInfo>
#include <random>

#include <QApplication>
#include <QUrlQuery>
#include <QTcpServer>
#include <QTimer>
#include <QMessageBox>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDateTime>
#include <QTimer>
#include <functional>
#include <QLocale>

QString defStr(const QString & value, const QString def){
    if (value.isEmpty()){
        return def;
    } else {
        return value;
    }
}

void runOnNewThread(const std::function<void()> &callback) {
    createQThread(callback)->start();
}

void runOnThread(const std::function<void()> &callback, QObject *parent) {
    auto *timer = new QTimer();
    auto thread = dynamic_cast<QThread *>(parent);
    if (thread == nullptr) {
        timer->moveToThread(parent->thread());
    } else {
        timer->moveToThread(thread);
    }
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}

QStringList SplitLines(const QString &_string) {
    return _string.split(QRegularExpression("[\r\n]"), Qt::SplitBehaviorFlags::SkipEmptyParts);
}

QStringList SplitLinesSkipSharp(const QString &_string, int maxLine) {
    auto lines = SplitLines(_string);
    QStringList newLines;
    int i = 0;
    for (const auto &line: lines) {
        if (line.trimmed().startsWith("#")) continue;
        newLines << line;
        if (maxLine > 0 && ++i >= maxLine) break;
    }
    return newLines;
}

QByteArray DecodeB64IfValid(const QString &input, QByteArray::Base64Options options) {
    Qt515Base64::Base64Options newOptions = Qt515Base64::Base64Option::AbortOnBase64DecodingErrors;
    if (options.testFlag(QByteArray::Base64UrlEncoding)) newOptions |= Qt515Base64::Base64Option::Base64UrlEncoding;
    if (options.testFlag(QByteArray::OmitTrailingEquals)) newOptions |= Qt515Base64::Base64Option::OmitTrailingEquals;
    auto result = Qt515Base64::QByteArray_fromBase64Encoding(input.toUtf8(), newOptions);
    if (result) {
        return result.decoded;
    }
    return {};
}

QString QStringList2Command(const QStringList &list) {
    QStringList new_list;
    for (auto str: list) {
        auto q = "\"" + str.replace("\"", "\\\"") + "\"";
        new_list << q;
    }
    return new_list.join(" ");
}

QString GetQueryValue(const QUrlQuery &q, const QString &key, const QString &def) {
    auto a = q.queryItemValue(key);
    if (a.isEmpty()) {
        return def;
    }
    return a;
}

void MoveDirToTrash(const QString &path){
    QDir dir(path);
    if (dir.exists()){
        if (!QFile::moveToTrash(path)){
            dir.removeRecursively();
        }
    }
};

QString GetRandomString(int randomStringLength) {
    static std::random_device rd;
    static std::mt19937 mt(rd());

    static const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    static std::uniform_int_distribution<int> dist(0, possibleCharacters.length() - 1);

    QString randomString;
    for (int i = 0; i < randomStringLength; ++i) {
        QChar nextChar = possibleCharacters.at(dist(mt));
        randomString.append(nextChar);
    }
    return randomString;
}

quint64 GetRandomUint64() {
    static std::random_device rd;
    static std::mt19937 mt(rd());
    static std::uniform_int_distribution<quint64> dist;
    return dist(mt);
}


QJsonArray QListStr2QJsonArray(const QList<QString> &list) {
    QVariantList list2;
    bool isEmpty = true;
    for (auto &item: list) {
        if (item.trimmed().isEmpty()) continue;
        list2.append(item);
        isEmpty = false;
    }

    if (isEmpty) return {};
    else return QJsonArray::fromVariantList(list2);
}

QJsonArray QListInt2QJsonArray(const QList<int> &list) {
    QVariantList list2;
    for (auto &item: list)
        list2.append(item);
    return QJsonArray::fromVariantList(list2);
}

QList<int> QJsonArray2QListInt(const QJsonArray &arr) {
    QList<int> list2;
    for (auto item: arr)
        list2.append(item.toInt());
    return list2;
}

std::vector<std::string> QListStr2VectorStr(const QStringList &list){
    std::vector<std::string> vec;
    for (QString str : list){
        vec.push_back(str.toStdString());
    }
    return vec;
}

QStringList VectorStr2QListStr(const std::vector<std::string> &list){
    QStringList vec;
    for (std::string str : list){
        vec.append(QString::fromStdString(str));
    }
    return vec;
}


std::vector<int> QListInt2VectorInt(const QList<int> &list){
    std::vector<int> vec;
    for (int str : list){
        vec.push_back(str);
    }
    return vec;
}

QList<int> VectorInt2QListInt(const std::vector<int> &list){
    QList<int> vec;
    for (int str : list){
        vec.append(str);
    }
    return vec;
}

QList<QString> QJsonArray2QListStr(const QJsonArray &arr) {
    QList<QString> list2;
    for (auto item: arr)
        list2.append(item.toString());
    return list2;
}

QJsonArray QString2QJsonArray(const QString& str) {
    auto doc = QJsonDocument::fromJson(str.toUtf8());
    if (doc.isArray()) {
        return doc.array();
    }
    return {};
}

QJsonObject QMapString2QJsonObject(const QMap<QString,QString> &mp) {
    QJsonObject res;
    for (const auto &key: mp.keys()) {
        res.insert(key, mp[key]);
    }

    return res;
}

QByteArray ReadFile(const QString &path) {
    QFile file(path);
    return ReadFile(file);
}

QByteArray ReadFile( QFile &file) {
    QByteArray array;
    if (file.open(QFile::ReadOnly)){
        array = file.readAll();
    }
    file.close();
    return array;
}

QString ReadFileText(const QString &path) {
    return QString::fromUtf8(ReadFile(path));
}

QString ReadFileText( QFile &path) {
    return QString::fromUtf8(ReadFile(path));
}

bool WriteFileText(const QString &path, const QString &notes){
    QFile file(path);
    return WriteFileText(file, notes);
}

bool WriteFile(const QString &path, const QByteArray &notes){
    QFile file(path);
    return WriteFile(file, notes);
}

bool WriteFile(QFile &file, const QByteArray &notes){
    QDir dir = QFileInfo(file).absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {  
            return false;
        }
    }
    bool ret = false;
    if (file.open(QIODevice::WriteOnly)){
        ret = file.write(notes) > -1;
        file.close();
    } 
    return ret;
}

bool WriteFileText(QFile &file, const QString &notes){
    QByteArray array = notes.toUtf8();
    return WriteFile(file, array);
}

int MkPort() {
    QTcpServer s;
    s.listen();
    auto port = s.serverPort();
    s.close();
    return port;
}

QString ReadableSize(const qint64 &size) {
    double sizeAsDouble = size;
    static QStringList measures;
    if (measures.isEmpty())
        measures << "B"
                 << "KiB"
                 << "MiB"
                 << "GiB"
                 << "TiB"
                 << "PiB"
                 << "EiB"
                 << "ZiB"
                 << "YiB";
    QStringListIterator it(measures);
    QString measure(it.next());
    while (sizeAsDouble >= 1024.0 && it.hasNext()) {
        measure = it.next();
        sizeAsDouble /= 1024.0;
    }
    return QString::fromLatin1("%1 %2").arg(sizeAsDouble, 0, 'f', 2).arg(measure);
}

bool IsIpAddress(const QString &str) {
    auto address = QHostAddress(str);
    if (address.protocol() == QAbstractSocket::IPv4Protocol || address.protocol() == QAbstractSocket::IPv6Protocol)
        return true;
    return false;
}

bool IsIpAddressV4(const QString &str) {
    auto address = QHostAddress(str);
    if (address.protocol() == QAbstractSocket::IPv4Protocol)
        return true;
    return false;
}

bool IsIpAddressV6(const QString &str) {
    auto address = QHostAddress(str);
    if (address.protocol() == QAbstractSocket::IPv6Protocol)
        return true;
    return false;
}

QString DisplayTime(long long time, int formatType) {
    QDateTime t;
    t.setMSecsSinceEpoch(time * 1000);
    return QLocale().toString(t, QLocale::FormatType(formatType));
}


const char * getSoftwareVersion(){
    static const char * VERSION_STATIC = nullptr;
    if (VERSION_STATIC == nullptr){
        VERSION_STATIC = 	
#ifdef _MSC_VER
	_strdup
#else
	strdup
#endif
		(software_version.toUtf8().constData());
    }
    return VERSION_STATIC;
}

int GetQueryIntValue(const QUrlQuery &q, const QString &key, int def){
    QString str = GetQueryValue(q, key);
    if (!str.isEmpty()){
        return str.toInt();
    } else {
        return def;
    }
};

QString SubStrBefore(QString str, const QString &sub) {
    if (!str.contains(sub)) return str;
    return str.left(str.indexOf(sub));
}

QString SubStrAfter(QString str, const QString &sub) {
    if (!str.contains(sub)) return str;
    return str.right(str.length() - str.indexOf(sub) - sub.length());
}


// [2001:4860:4860::8888] -> 2001:4860:4860::8888
QString UnwrapIPV6Host(QString &str) {
    return str.replace("[", "").replace("]", "");
}

// [2001:4860:4860::8888] or 2001:4860:4860::8888 -> [2001:4860:4860::8888]
QString WrapIPV6Host(QString &str) {
    if (!IsIpAddressV6(str)) return str;
    return "[" + UnwrapIPV6Host(str) + "]";
}

QString DisplayAddress(QString serverAddress, int serverPort) {
    if (serverAddress.isEmpty() && serverPort == 0) return {};
    return WrapIPV6Host(serverAddress) + ":" + QString::number(serverPort);
}

QString DisplayDest(const QString& dest, QString domain)
{
    if (domain.isEmpty() || dest.split(":").first() == domain) return dest;
    return dest + " (" + domain + ")";
}

bool InRange(unsigned x, unsigned low, unsigned high) {
    return (low <= x && x <= high);
}

bool IsValidPort(int port) {
    return InRange(port, 1, 65535);
}

void AddQueryString( QUrlQuery & query, const QString& name, const QString & value){
    if (!value.isEmpty()){
        query.addQueryItem(name, value);
    }
}

void AddQueryStringList( QUrlQuery & query, const QString& name, const QStringList & value){
    if (!value.isEmpty()){
        AddQueryString(query, name,
            QString::fromUtf8(QJsonDocument(QListStr2QJsonArray(value)).toJson())
        );
    }
}

void AddQueryMap( QUrlQuery & query, const QString& name, const QVariantMap & value){
    if (!value.isEmpty()){
        AddQueryString(query, name,
            QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(value)).toJson())
        );
    }
}

void AddQueryInt( QUrlQuery & query, const QString& name, int value){
    query.addQueryItem(name, QString::number(value));
}


void AddQueryNatural( QUrlQuery & query, const QString& name,int value){
    if (value > 0){
        AddQueryInt(query, name, value);
    }
}

QStringList GetQueryListValue(const QUrlQuery &q, const QString &key){
    return QJsonArray2QListStr(QString2QJsonArray(GetQueryValue(q, key)));
}

QVariantMap QString2QMap(const QString &key){
    QJsonDocument doc = QJsonDocument::fromJson(key.toUtf8());

    QVariantMap map;
    if (!doc.isNull() && doc.isObject()) {
        map = doc.object().toVariantMap();
    }
    return map;
}

QVariantMap GetQueryMapValue(const QUrlQuery &q, const QString &key){
    return QString2QMap(GetQueryValue(q, key, "{}"));
}

QString QMap2QString(const QVariantMap &map) {
    QJsonObject jsonObject = QJsonObject::fromVariantMap(map);
    QJsonDocument jsonDoc(jsonObject);
    return jsonDoc.toJson(QJsonDocument::Indented);  // Use Compact for a minified string
}

QString QJsonType2QString(QJsonValue::Type type){
    switch (type){
        case QJsonValue::Type::String:
            return "String";
        case QJsonValue::Type::Array:
            return "Array";
        case QJsonValue::Type::Bool:
            return "Bool";
        case QJsonValue::Type::Double:
            return "Double";
        case QJsonValue::Type::Null:
            return "Null";
        case QJsonValue::Type::Object:
            return "Object";
        case QJsonValue::Type::Undefined:
        default:
            break;
    };
    return "Undefined";
}


