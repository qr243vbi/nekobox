#ifndef JS_UPDATER
#define JS_UPDATER
#include "message_queue.h"
#include <QString>
#include <QObject>
#include <QVariantMap>
#include <QFile>
#include <nekobox/dataStore/Group.hpp>
#include <QTextStream>
#include <functional>
#include <QStringList>
#include <nekobox/dataStore/HTTPRequestHelper.hpp>

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


bool jsUpdater( JsUpdaterWindow* bQueue,
  QString * updater_js,
  QString * search,
  QString * archive_name,
  bool * is_newer,
  QStringList * args,
  bool allow_updater,
  bool * keep_running,
  bool button_clicked
);

bool jsRouteProfileGetter(
    JsUpdaterWindow * factory,
    QString * updater_js,
    QStringList * list,
    QMap<QString, QString> * names,
    std::function<QString(QString, QString *, bool*)> * func
);

bool jsAnnouncementMessage(
    JsUpdaterWindow * factory,
    QString * updater_js,
    bool first_start
);

std::shared_ptr<const Configs::GroupExtra> jsSubscription(
        JsUpdaterWindow* factory,
        std::shared_ptr<const Configs::GroupExtra> extra);

#endif
