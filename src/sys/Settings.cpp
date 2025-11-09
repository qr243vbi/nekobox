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
#ifndef SKIP_UPDATER_BUTTON
QString getUpdaterPath(){
    return getResource(
    #ifdef Q_OS_WIN
        "updater.exe"
    #else
        "updater"
    #endif
    );
}
#endif


#ifdef Q_OS_LINUX
#include <QProcessEnvironment>
QString getAppImage(){
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains("APPIMAGE") && env.contains("NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE")){
        QString appimage = env.value("APPIMAGE");
        QString appimageproof = env.value("NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE");
        QFile imageproof(getRootResource(appimageproof));
        if (imageproof.exists()){
            QFile image(appimage);
            if (image.exists()){
                return appimage;
            }
        }
    }
    return "";
}

bool isAppImage(){
    return getAppImage() != "";
}
QString getApplicationPath(){
    QString appimage = getAppImage();
    if (appimage == ""){
        return appimage;
    } else {
        return QCoreApplication::applicationFilePath();
    }
}
#else
QString getApplicationPath(){
    return QCoreApplication::applicationFilePath();
}
#endif

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
