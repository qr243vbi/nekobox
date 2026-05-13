#ifdef _WIN32
#include <winsock2.h>
#endif


#include <cpr/proxyauth.h>
#include <cpr/cpr.h>


#include <QString>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMap>

#include <string>

#include <nekobox/global/HTTPRequestHelper.hpp>
#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/global/DeviceDetailsHelper.hpp>
#include <nekobox/ui/mainwindow.h>

static inline void BuildSession(const QString &url, bool sendHwid, cpr::Session& session)
{
    auto s = Configs::dataStore;

    session.SetUrl(cpr::Url{url.toStdString()});
    session.SetTimeout(cpr::Timeout{Configs::dataStore->download_timeout});

    // ------------------------
    // USER AGENT
    // ------------------------
    session.SetHeader({
        {"User-Agent", s->GetUserAgent().toStdString()}
    });

    // ------------------------
    // SSL
    // ------------------------
    if (s->net_insecure) {
        session.SetVerifySsl(cpr::VerifySsl{false});
    }

    // ------------------------
    // PROXY
    // ------------------------
    bool useProxy = s->useProxyForHttpRequest() || s->spmode_system_proxy;
    bool proxyStarted = s->started_id >= 0;

    if (useProxy && proxyStarted)
    {
        QString host = (s->inbound_address == "::")
            ? "127.0.0.1"
            : s->inbound_address;

        std::string proxy =
            host.toStdString() + ":" +
            std::to_string(s->inbound_socks_port);

        session.SetProxies({
            {"http", proxy},
            {"https", proxy},
        });

        if (!s->inbound_username.isEmpty() &&
            !s->inbound_password.isEmpty())
        {
            auto user = s->inbound_username.toStdString();
            auto pass = s->inbound_password.toStdString();
            session.SetProxyAuth({
                {"http", cpr::EncodedAuthentication{user, pass}},   
                {"https", cpr::EncodedAuthentication{user, pass}}
            });
        }
    }

    // ------------------------
    // HWID HEADERS
    // ------------------------
    if (sendHwid)
    {
        auto d = GetDeviceDetails();

        QMap<QString, QString> custom;

        if (!s->sub_custom_hwid_params.isEmpty()) {
            QStringList pairs = s->sub_custom_hwid_params.split(',');
            for (auto &p : pairs) {
                auto t = p.trimmed();
                int eq = t.indexOf('=');
                if (eq > 0) {
                    custom[t.left(eq).toLower()] = t.mid(eq + 1).trimmed();
                }
            }
        }

        auto hwid   = custom.value("hwid", d.hwid);
        auto os     = custom.value("os", d.os);
        auto osv    = custom.value("osversion", d.osVersion);
        auto model  = custom.value("model", d.model);

        cpr::Header headers;

        if (!hwid.isEmpty())  headers["x-hwid"] = hwid.toStdString();
        if (!os.isEmpty())    headers["x-device-os"] = os.toStdString();
        if (!osv.isEmpty())   headers["x-ver-os"] = osv.toStdString();
        if (!model.isEmpty()) headers["x-device-model"] = model.toStdString();

        session.UpdateHeader(headers);
    }
}

namespace Configs_network
{

HTTPResponse NetworkRequestHelper::HttpGet(const QString &url, bool sendHwid)
{
    cpr::Session session; 
    BuildSession(url, sendHwid, session);
    cpr::Response r = session.Get();

    auto resp = HTTPResponse{
        r.error ? QString::fromStdString(r.error.message) : "",
        QByteArray(r.text.data(), r.text.size()),
        {}
    };

    auto & result = resp.header;
    auto & headers = r.header;
    result.reserve(static_cast<int>(headers.size()));

    for (const auto &kv : headers)
    {
        QByteArray key   = QByteArray::fromStdString(kv.first);
        QByteArray value = QByteArray::fromStdString(kv.second);

        result.append(qMakePair(key, value));
    }

    return resp;
}

QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
    for (const auto &p: header) {
        if (QString(p.first).toLower() == name.toLower()) return p.second;
    }
    return "";
}

QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName)
{
    QString result;
    QEventLoop loop;

    QString filePath = Configs::GetBasePath() + "/" + fileName;

    QFuture future = QtConcurrent::run([=, &result, &loop]()
    {
        QDir().mkpath(QFileInfo(filePath).absolutePath());

        auto mw = GetMainWindow();

        const int maxRetries = Configs::dataStore->download_retries;
        cpr::Response r;

        for (int i = 0; i < maxRetries; ++i)
        {
            // =========================
            // NEW SESSION PER TRY (IMPORTANT FIX)
            // =========================
            cpr::Session session;
            BuildSession(url, false, session);

            qint64 existingSize = QFileInfo(filePath).exists()
                ? QFileInfo(filePath).size()
                : 0;

            // =========================
            // RESUME HEADER (RESET EACH TRY)
            // =========================
            if (existingSize > 0)
            {
                session.SetHeader({
                    {"Range", "bytes=" + std::to_string(existingSize) + "-"}
                });
            }

            // =========================
            // FILE OPEN (ONCE PER TRY)
            // =========================
            QFile file(filePath);

            if (!file.open(existingSize > 0
                    ? QIODevice::Append
                    : QIODevice::WriteOnly))
            {
                result = "Could not open file.";
                QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
                return;
            }


        // ------------------------
        // STREAM WRITE CALLBACK
        // ------------------------
        std::function<bool(std::string_view data, intptr_t userdata)> write_callback;
        write_callback = [&](std::string_view data, intptr_t)->bool
            {
                file.write(data.data(), data.size());
                return true;
            };

        // ------------------------
        // PROGRESS CALLBACK
        // ------------------------

        std::function<bool(cpr::cpr_pf_arg_t downloadTotal, cpr::cpr_pf_arg_t downloadNow, cpr::cpr_pf_arg_t uploadTotal, cpr::cpr_pf_arg_t uploadNow, intptr_t userdata)> progress_callback = 


[&](cpr::cpr_off_t total,
                cpr::cpr_off_t now,
                cpr::cpr_off_t uptotal,
                cpr::cpr_off_t upnow, intptr_t )
            {
                if (!mw || mw->isShowRuleSetData())
                    return true;

                runOnUiThread([=]{
                    mw->setDownloadReport(
                        DownloadProgressReport{
                            fileName,
                            (qint64)now,
                            (qint64)total
                        },
                        true
                    );
                    mw->UpdateDataView();
                });

                return true;
            };

            // =========================
            // WRITE CALLBACK
            // =========================
            session.SetWriteCallback(
                write_callback
            );

            // =========================
            // PROGRESS CALLBACK
            // =========================
            session.SetProgressCallback(
                progress_callback
            );

            // =========================
            // EXECUTE
            // =========================
            r = session.Get();

            file.close();

            // =========================
            // SUCCESS
            // =========================
            if (!r.error)
                break;

            // backoff
            int delay = std::min(10000, (1 << i) * 1000);
            QThread::msleep(delay);
        }

        // =========================
        // RESET UI
        // =========================
        runOnUiThread([=]{
            mw->setDownloadReport({}, false);
            mw->UpdateDataView(true);
        });

        // =========================
        // ERROR HANDLING
        // =========================
        if (r.error)
        {
            result = QString::fromStdString(r.error.message);

            qint64 size = QFileInfo(filePath).exists()
                ? QFileInfo(filePath).size()
                : 0;

            result += "\n(fetched: " + QString::number(size) + ")";
        }

        QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
    });

    loop.exec();
    future.waitForFinished();

    return result;
}

/*
QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName)
{
    QString result;
    QEventLoop loop;

    QFuture future = QtConcurrent::run([=, &result, &loop]()
    {
        cpr::Session session;
        BuildSession(url, false, session);

        QString filePath = Configs::GetBasePath() + "/" + fileName;
        QDir().mkpath(QFileInfo(filePath).absolutePath());

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            result = "Could not open file.";
            QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
            return;
        }

        auto mw = GetMainWindow();

        // ------------------------
        // STREAM WRITE CALLBACK
        // ------------------------
        std::function<bool(std::string_view data, intptr_t userdata)> write_callback;
        write_callback = [&](std::string_view data, intptr_t)->bool
            {
                file.write(data.data(), data.size());
                return true;
            };

        // ------------------------
        // PROGRESS CALLBACK
        // ------------------------

std::function<bool(cpr::cpr_pf_arg_t downloadTotal, cpr::cpr_pf_arg_t downloadNow, cpr::cpr_pf_arg_t uploadTotal, cpr::cpr_pf_arg_t uploadNow, intptr_t userdata)> progress_callback = 


[&](cpr::cpr_off_t total,
                cpr::cpr_off_t now,
                cpr::cpr_off_t uptotal,
                cpr::cpr_off_t upnow, intptr_t )
            {
                if (!mw || mw->isShowRuleSetData())
                    return true;

                runOnUiThread([=]{
                    mw->setDownloadReport(
                        DownloadProgressReport{
                            fileName,
                            (qint64)now,
                            (qint64)total
                        },
                        true
                    );
                    mw->UpdateDataView();
                });

                return true;
            };

        session.SetProgressCallback(
            progress_callback
        );

        // ------------------------
        // RETRY LOGIC (25x)
        // ------------------------
        cpr::Response r;

        for (int i = 0; i < 25; ++i)
        {
            r = session.Get();

            if (!r.error)
                break;

            if (
                r.error.code != cpr::ErrorCode::OPERATION_TIMEDOUT )
                break;

            QThread::msleep(500 * (i + 1));
        }

        file.close();

        // reset UI
        runOnUiThread([=]{
            mw->setDownloadReport({}, false);
            mw->UpdateDataView(true);
        });

        if (r.error) {
            result = QString::fromStdString(r.error.message);
        }

        QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
    });

    loop.exec();
    future.waitForFinished();
    return result;
}
*/
}
