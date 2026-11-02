#pragma once

#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <atomic>

namespace Configs {
struct XrayURLTestResult {
    int latencyMs = -1;
    QString error;
    bool cancelled = false;
};

// Runs synchronously on a worker thread. Owns its process, port and config;
// never reloads or stops the user's active proxy.
XrayURLTestResult TestXrayURL(const QString &program, QJsonObject config,
                            const QUrl &url, int timeoutMs,
                            const std::atomic<bool> &cancel);
}
