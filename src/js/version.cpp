#include <include/js/version.h>
#if NKR_VERSION == getNkrVersion

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

const char * getVersionString(){
    static const char * VERSION = nullptr;
    if (VERSION == nullptr){
        QString appPath = QCoreApplication::applicationDirPath();
        QString filePath = appPath + "/version.txt";
        QFile file(filePath);
        QString source;
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            source = "1.0.0";
        } else {
            QTextStream in(&file);
            source = in.readAll();
            file.close();
        }
        QByteArray tempByteArray = source.toUtf8();
        VERSION = strdup(tempByteArray.constData());
    }
    return VERSION;
}
#endif
