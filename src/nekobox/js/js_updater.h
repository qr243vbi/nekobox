#ifndef JS_UPDATER
#define JS_UPDATER
#include "message_queue.h"
#include <QString>
#include <QObject>
#include <QVariantMap>
#include <QFile>
#include <QTextStream>
#include <functional>
#include <QStringList>
#include <nekobox/global/HTTPRequestHelper.hpp>

class JsUpdaterWindow : public QObject
{
    Q_OBJECT
public:
    explicit JsUpdaterWindow(); //MainWindow*);
    Q_INVOKABLE void print(const QVariant value);
    Q_INVOKABLE void log(const QVariant value, const QVariant title);
    Q_INVOKABLE void warning(const QVariant message, const QVariant title);
    Q_INVOKABLE void info(const QVariant message, const QVariant title);
    Q_INVOKABLE bool file_exists(const QVariant file);
    Q_INVOKABLE int ask(const QVariant value, const QVariant title, const QVariant map);
    Q_INVOKABLE QString get_jsdelivr_link(const QVariant value);
    Q_INVOKABLE QString translate(const QVariant value);
    Q_INVOKABLE QString get_locale() const;
    Q_INVOKABLE QString download(const QVariant url, const QVariant fileName, const QVariant skipIfExists);
    Q_INVOKABLE QString curdir();
    Q_INVOKABLE void open_url(const QVariant url);
    QMutex mutex;
signals:
    void log_signal(const QString &value, const QString &title);
    void warning_signal(const QString &value, const QString &title);
    void info_signal(const QString &value, const QString &title);
    void download_signal(const QString &url, const QString &asset, QString *ret);
    void ask_signal(const QString &value, const QString &title, const QStringList &map, int * ret);
public slots:
    void unlock();
};

class JsTextWriter : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit JsTextWriter();
    Q_INVOKABLE bool open(const QVariant path);
    Q_INVOKABLE void write(const QVariant text);
    Q_INVOKABLE void close();
    virtual ~JsTextWriter() override;
private:
    QFile file;
    QTextStream stream;
};

class JsHTTPRequest : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ get_text)
    Q_PROPERTY(QString error READ get_error)
    Q_PROPERTY(QVariantMap header READ get_header)

public:
    Q_INVOKABLE explicit JsHTTPRequest(const QString& url);

    // Q_INVOKABLE methods for JavaScript access
    Q_INVOKABLE QString get_text() const;
    Q_INVOKABLE QString get_error() const;
    Q_INVOKABLE QVariantMap get_header() const;

    // Virtual destructor is a good practice for QObject subclasses
    virtual ~JsHTTPRequest() override;

private:
    void init(const QString& url);
private:
    QString m_text;
    QString m_error;
    QVariantMap m_header;
};


bool jsUpdater( JsUpdaterWindow* bQueue,
  QString * updater_js,
  QString * search,
  QString * archive_name,
  bool * is_newer,
  QStringList * args,
  bool allow_updater,
  bool * keep_running
);

bool jsRouteProfileGetter(
    JsUpdaterWindow * factory,
    QString * updater_js,
    QStringList * list,
    QMap<QString, QString> * names,
    std::function<QString(QString, QString *, bool*)> * func
);

#endif
