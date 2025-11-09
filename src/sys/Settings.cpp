#include "include/sys/Settings.h"
QSettings getSettings(){
    return QSettings(CONFIG_INI_PATH, QSettings::IniFormat);
}

QString getResourcesDir(){
    QSettings settings = getSettings();
    QString str = settings.value("resources_path", "").toString();
    if (str == ""){
        str = getRootResource("public");
    }
    return str;
};

QString getResource(QString str){
    QString dir = getResourcesDir();
    dir += "/";
    dir += str;
    QFile file(dir);
    if (file.exists()){
        return dir;
    } else {
        return getRootResource(str);
    }
}

QString getRootResource(QString str){
    QString dir = QCoreApplication::applicationDirPath();
    dir += "/";
    dir += str;
    return dir;
}

QString getUpdaterPath(){
    return getResource(
    #ifdef Q_OS_WIN
        "updater.exe"
    #else
        "updater"
    #endif
    );
}

QString getCorePath(){
    QSettings settings = getSettings();
    QString core_path = getRootResource(
    #ifdef Q_OS_WIN
        "nekobox_core.exe"
    #else
        "nekobox_core"
    #endif
    );
    return settings.value("core_path", core_path).toString();
}
