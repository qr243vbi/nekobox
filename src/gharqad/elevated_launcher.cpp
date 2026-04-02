#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QProcess>
#include <QJsonArray>
#include <qcontainerfwd.h>
#include <QFile>
#include <QSettings>
#include <QDir>
#include <qcoreapplication.h>
#include <QRandomGenerator>
#include <QProcessEnvironment>

QString handleCommand(const QString& cmd, const QString &program) {
    auto obj = QJsonValue::fromJson(cmd.toUtf8());
    QProcess process;
    QStringList args;
    QString ret = "";


    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList envs = env.toStringList();
    
    auto val = obj["args"];
    if (val.isArray()){
        for (QVariant var : val.toArray().toVariantList()){
            args << var.toString();
        }
    }

    val = obj["envs"];
    if (val.isArray()){
        for (QVariant var : val.toArray().toVariantList()){
            envs << var.toString();
        }
    }

    if (args.length() > 0) {
        process.setArguments(args);
    }
    if (envs.length() > 0) {
        process.setEnvironment(envs);
    }
    if (!program.isEmpty()){
        if (QFile(program).exists()){
            process.setProgram(program);
            process.startDetached();
            ret = "ok";
        }
    }

    return ret;
}

void unregister_task(QString task){

}

void register_task(QString task){

}

QString generateRandomString(int length)
{
    const QString chars = "abcdefghijklmnopqrstuvwxyz";
    QString result;

    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        result.append(chars.at(index));
    }

    return result;
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    QLocalServer server;

    QSettings settings(QString(QCoreApplication::applicationDirPath() + QDir::separator() + "elevated_launcher.ini"), QSettings::IniFormat);

    if (argc > 2){
        QString command = argv[1];
        if (command == "register"){ 
            QString task = settings.value("task_name", "").toString();
            if (!task.isEmpty()){
                unregister_task(task);
            }
            register_task(task);
            task = generateRandomString(15);
            settings.setValue("socket", generateRandomString(28));
            settings.setValue("program", QString(argv[2]));
            settings.setValue("task_name", task);
            settings.sync();
        }
        
        return 1;
    }

    QString socketName = settings.value("socket").toString();
    QString program = settings.value("program").toString();

    if (socketName.isEmpty()){
        return 1;
    }
    
    // Remove existing socket (important on Linux/macOS)
    QLocalServer::removeServer(socketName);

    if (!server.listen(socketName)) {
        qCritical() << "Unable to start server:" << server.errorString();
        return 1;
    }

    qDebug() << "Server listening on" << socketName;

    QObject::connect(&server, &QLocalServer::newConnection, [&, program]() {
        QLocalSocket *client = server.nextPendingConnection();

        QObject::connect(client, &QLocalSocket::readyRead, [client, program]() {
            QByteArray data = client->readAll();
            QString command = QString::fromUtf8(data).trimmed();

            qDebug() << "Received:" << command;

            QString response = handleCommand(command, program);

            client->write(response.toUtf8() + "\n");
            client->flush();
        });

        QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });

    return a.exec();
}
