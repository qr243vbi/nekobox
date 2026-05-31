#include <nekobox/global/HTTPRequestHelperGui.hpp>
#include <cpr/proxyauth.h>
#include <cpr/cpr.h>


#include <QString>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMap>

#include <string>

#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/global/DeviceDetailsHelper.hpp>
#include <nekobox/ui/mainwindow.h>

QString Configs_network::NetworkRequestHelperGui::DownloadAsset(const QString &url, const QString &fileName)
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