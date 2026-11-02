#include <nekobox/sys/XrayURLTest.hpp>

#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

namespace Configs {
XrayURLTestResult TestXrayURL(const QString &program, QJsonObject config,
                            const QUrl &url, int timeoutMs,
                            const std::atomic<bool> &cancel) {
    XrayURLTestResult result;
    if (cancel.load()) { result.cancelled = true; return result; }
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != "http" && url.scheme() != "https") || timeoutMs <= 0) {
        result.error = "Invalid URL test address or timeout";
        return result;
    }
    QTemporaryDir directory;
    if (!directory.isValid()) {
        result.error = "Cannot create temporary Xray test directory";
        return result;
    }
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) {
        result.error = reservation.errorString();
        return result;
    }
    const auto port = reservation.serverPort();
    auto inbounds = config["inbounds"].toArray();
    if (inbounds.size() != 1 || inbounds[0].toObject()["protocol"] != "socks") {
        result.error = "Xray URL test requires one SOCKS inbound";
        return result;
    }
    auto inbound = inbounds[0].toObject();
    inbound["listen"] = "127.0.0.1";
    inbound["port"] = int(port);
    inbounds[0] = inbound;
    config["inbounds"] = inbounds;
    QFile file(directory.filePath("test.json"));
    const auto bytes = QJsonDocument(config).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        result.error = "Cannot write temporary Xray test config";
        return result;
    }
    file.close();
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    struct Cleanup {
        QProcess &process;
        ~Cleanup() {
            if (process.state() != QProcess::NotRunning) {
                process.kill();
                process.waitForFinished(3000);
            }
        }
    } cleanup{process};
    reservation.close();
    process.start(program, {"run", "-format", "json", "-c", file.fileName()});
    if (!process.waitForStarted(qMin(timeoutMs, 3000))) {
        result.error = "Cannot start Xray URL test: " + process.errorString();
        return result;
    }
    QElapsedTimer startup;
    startup.start();
    bool ready = false;
    while (startup.elapsed() < qMin(timeoutMs, 5000)) {
        if (cancel.load()) { result.cancelled = true; return result; }
        if (process.state() == QProcess::NotRunning) break;
        QTcpSocket probe;
        probe.connectToHost(QHostAddress::LocalHost, port);
        if (probe.waitForConnected(50)) { ready = true; break; }
        process.waitForFinished(20);
    }
    if (!ready) {
        result.error = "Xray test listener did not start: " +
                       QString::fromUtf8(process.readAll()).right(4096);
        return result;
    }

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy, "127.0.0.1", port));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QElapsedTimer elapsed;
    elapsed.start();
    auto *reply = manager.get(request);
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    bool timedOut = false;
    bool receivedHeaders = false;
    QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::metaDataChanged, &loop, [&] {
        // Measure time to response headers, without downloading the body.
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // Qt can publish Location after metaDataChanged. Wait for redirect
        // handling to finish instead of accepting an intermediate 3xx response.
        if (status > 0 && !(status >= 300 && status < 400)) {
            receivedHeaders = true;
            loop.quit();
        }
    });
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (cancel.load() || process.state() == QProcess::NotRunning) loop.quit();
        // Drain the child pipe so verbose core output cannot block it.
        process.readAll();
    });
    poll.start(50);
    deadline.start(timeoutMs);
    loop.exec();
    if (cancel.load()) result.cancelled = true;
    else if (timedOut) result.error = "Xray URL test timed out";
    else if (process.state() == QProcess::NotRunning)
        result.error = "Xray exited during URL test";
    else if (receivedHeaders || (reply->error() == QNetworkReply::NoError &&
                                 reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() > 0))
        result.latencyMs = qMax(1, int(elapsed.elapsed()));
    else result.error = reply->errorString();
    reply->abort();
    delete reply;
    return result;
}
}
