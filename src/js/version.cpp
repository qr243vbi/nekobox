#include <include/js/version.h>

#ifdef NKR_DYNAMIC_VERSION

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "include/sys/Settings.h"

const char * getVersionString(){
    static const char * VERSION = nullptr;
    if (VERSION == nullptr){
        QString filePath = getResource("version.txt");
        QFile file(filePath);
        QString source;
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
#ifdef NKR_DEFAULT_VERSION
            source = NKR_DEFAULT_VERSION;
#else
            source = "1.0.0";
#endif
        } else {
            QTextStream in(&file);
            source = in.readAll();
            file.close();
        }
        QByteArray tempByteArray = source.simplified().toUtf8();
        VERSION = strdup(tempByteArray.constData());
    }
    return VERSION;
}

#else

const char * getVersionString(){
    return NKR_VERSION;
}

#endif
