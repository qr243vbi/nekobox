#include <nekobox/sys/XrayURLTest.hpp>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTimer>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    QFile file(args[2]);
    if (!file.open(QIODevice::ReadOnly)) return 2;
    if (qEnvironmentVariableIsSet("NEKOBOX_TEST_CA")) {
        auto ssl = QSslConfiguration::defaultConfiguration();
        ssl.addCaCertificates(QSslCertificate::fromPath(qEnvironmentVariable("NEKOBOX_TEST_CA")));
        QSslConfiguration::setDefaultConfiguration(ssl);
    }
    std::atomic<bool> cancel(false);
    if (args[5].toInt() == 0) cancel.store(true);
    else if (args[5].toInt() > 0)
        QTimer::singleShot(args[5].toInt(), &app, [&] { cancel.store(true); });
    const auto result = Configs::TestXrayURL(args[1], QJsonDocument::fromJson(file.readAll()).object(),
                                            QUrl(args[3]), args[4].toInt(), cancel);
    std::cout << QJsonDocument(QJsonObject{{"latency", result.latencyMs}, {"error", result.error},
                                         {"cancelled", result.cancelled}}).toJson().toStdString();
}
