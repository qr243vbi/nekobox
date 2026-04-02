#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDateTime>
#include <QDebug>

QString handleCommand(const QString& cmd) {
    if (cmd == "time") {
        return QDateTime::currentDateTime().toString();
    } else if (cmd == "hello") {
        return "Hello from Qt server!";
    } else {
        return "ERROR: Command not allowed";
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    QLocalServer server;

    QString socketName = "MyQtSocket";

    // Remove existing socket (important on Linux/macOS)
    QLocalServer::removeServer(socketName);

    if (!server.listen(socketName)) {
        qCritical() << "Unable to start server:" << server.errorString();
        return 1;
    }

    qDebug() << "Server listening on" << socketName;

    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket *client = server.nextPendingConnection();

        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            QByteArray data = client->readAll();
            QString command = QString::fromUtf8(data).trimmed();

            qDebug() << "Received:" << command;

            QString response = handleCommand(command);

            client->write(response.toUtf8() + "\n");
            client->flush();
        });

        QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });

    return a.exec();
}
