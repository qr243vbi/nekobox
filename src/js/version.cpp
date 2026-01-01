#include <nekobox/js/version.h>

#ifdef NKR_DYNAMIC_VERSION

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <nekobox/sys/Settings.h>
#define _CRT_NONSTDC_NO_WARNINGS

const char * getVersionString(){
    static const char * VERSION = nullptr;
    if (VERSION == nullptr){
        auto filePath = getGlobal();
        QString source;
            source = filePath.value("software_version",
#ifdef NKR_DEFAULT_VERSION
                                    NKR_DEFAULT_VERSION
#else
                                    "1.0.0"
#endif
            ).toString();

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
