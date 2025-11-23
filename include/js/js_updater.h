#ifndef JS_UPDATER
#define JS_UPDATER
#include "message_queue.h"
#include <QString>
#include <QObject>
#include <QVariantMap>
#include <functional>
#include <include/global/HTTPRequestHelper.hpp>

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
    Q_INVOKABLE QString get_jsdelivr_link(const QVariant value);
    Q_INVOKABLE QString translate(const QVariant value);
signals:
    void log_signal(const QString &value, const QString &title);
    void warning_signal(const QString &value, const QString &title);
    void info_signal(const QString &value, const QString &title);
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
  QString * assets_name,
  QString * release_download_url,
  QString * release_url,
  QString * release_note,
  QString * note_pre_release,
  QString * archive_name,
  bool * is_newer);

bool jsRouteProfileGetter(
    JsUpdaterWindow * factory,
    QString * updater_js,
    QStringList * list,
    std::function<QString(QString)> * func
);

#endif
