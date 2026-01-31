#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <csignal>

#include <QCryptographicHash>
#include <QDir>
#include <QTranslator>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QLocalServer>
#include <QThread>
#include <QFileInfo>
#include <QApplication>
#ifdef Q_OS_WIN
#include <3rdparty/WinCommander.hpp>
#include <windows.h>

#include <iostream>
#include "nekobox/sys/windows/MiniDump.h"
#include "nekobox/sys/windows/eventHandler.h"
#include "nekobox/sys/windows/WinVersion.h"
#include <qfontdatabase.h>
#endif

#include "nekobox/sys/Settings.h"
#include "nekobox/dataStore/ResourceEntity.hpp"

#include <nekobox/ui/mainwindow_interface.h>
#include <nekobox/global/GuiUtils.hpp>

QVariantMap ruleSetMap;
QWidget *mainwindow;

#ifdef Q_OS_UNIX
#include <nekobox/sys/linux/LinuxCap.h>
#include <qfontdatabase.h>
#endif
#define disable_run_admin windows_no_admin

void signal_handler(int signum) {
    if (GetMainWindow()) {
        GetMainWindow()->prepare_exit();
        qApp->quit();
    }
}

QTranslator* trans = nullptr;
QTranslator* trans_qt = nullptr;

void loadTranslate(const QString& locale) {
    QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Apply");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Yes");
    QT_TRANSLATE_NOOP("QPlatformTheme", "No");
    QT_TRANSLATE_NOOP("QPlatformTheme", "OK");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Defaults");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Restore Defaults");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Discard");
    
    QT_TRANSLATE_NOOP("QPlatformTheme", "Undo");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Redo");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Cut");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Copy");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Paste");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Delete");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Select All");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Stop");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Clear");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Copy Link Location");
    if (trans != nullptr) {
        trans->deleteLater();
    }
    if (trans_qt != nullptr) {
        trans_qt->deleteLater();
    }
    //
    trans = new QTranslator;
    trans_qt = new QTranslator;
    QLocale::setDefault(QLocale(locale));
    //
    if (trans->load(":/translations/" + locale + ".qm")) {
        QCoreApplication::installTranslator(trans);
    }
}

#define LOCAL_SERVER_PREFIX "nekobox-"

int main(int argc, char** argv) {
    // Core dump
#ifdef Q_OS_WIN
    Windows_SetCrashHandler();
#endif
#ifdef Q_OS_LINUX
    Unix_SetCrashHandler();
#endif

    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication a(argc, argv);

    // Flags
    Configs::dataStore->argv = QApplication::arguments();
    if (Configs::dataStore->argv.contains("-many")) Configs::dataStore->flag_many = true;
    if (Configs::dataStore->argv.contains("-appdata")) {
        Configs::dataStore->flag_use_appdata = true;
        int appdataIndex = Configs::dataStore->argv.lastIndexOf("-appdata") + 1;
        if (Configs::dataStore->argv.size() > appdataIndex && !Configs::dataStore->argv.at(appdataIndex).startsWith("-")) {
            Configs::dataStore->appdataDir = Configs::dataStore->argv.at(appdataIndex);
        }
    }
    
    if (Configs::dataStore->argv.contains("-tray")) Configs::dataStore->flag_tray = true;
    if (Configs::dataStore->argv.contains("-debug")) Configs::dataStore->flag_debug = true;
    if (Configs::dataStore->argv.contains("-flag_restart_tun_on")) Configs::dataStore->flag_restart_tun_on = true;
    if (Configs::dataStore->argv.contains("-flag_restart_dns_set")) Configs::dataStore->flag_dns_set = true;
#ifdef NKR_CPP_DEBUG
    Configs::dataStore->flag_debug = true;
#endif
    bool use_application_dir = true;
    QApplication::setApplicationName("nekobox");
    
    bool dir_success = true;
    QDir dir;
	// dirs & clean
    auto wd = QDir(root_directory);
    if (Configs::dataStore->flag_use_appdata) {
        if (!Configs::dataStore->appdataDir.isEmpty()) {
            wd.setPath(Configs::dataStore->appdataDir);
        } else {
			loop_back_1:
            wd.setPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        }
        use_application_dir = false;
    }
    dir_success = true;
    
    if (!wd.exists()) wd.mkpath(wd.absolutePath());
    if (!wd.exists("settings")) {
        dir_success &= wd.mkdir("settings");
    }
    
	{
		QString wd_abs = wd.absoluteFilePath("settings");
		QDir::setCurrent(wd_abs);
		MoveDirToTrash("temp");
		dir = QDir(wd_abs);
	
		QFileInfo fileInfo(wd_abs);
		dir_success &= fileInfo.exists();
		dir_success &= fileInfo.isDir();  
		if (!dir_success){
			goto loop_back_2;
		}
	}
        // Dir
    if (!dir.exists("profiles")) {
        dir_success &= dir.mkdir("profiles");
    }
    if (!dir.exists("groups")) {
        dir_success &= dir.mkdir("groups");
    }
    if (!dir.exists("resources")) {
        dir_success &= dir.mkdir("resources");
    }
    if (!dir.exists("route_profiles")) {
        dir_success &= dir.mkdir("route_profiles");
    }
    if (!dir_success) {
        loop_back_2:
        if (use_application_dir){
            Configs::dataStore->flag_use_appdata = true;
            goto loop_back_1;
        }
        QMessageBox::critical(nullptr, "Error", "No permission to write " + wd.absoluteFilePath("settings"));
        return 1;
    }

    
    dir_success &= isFileAppendable("nekobox.cfg");
    
    if (!dir_success){
        goto loop_back_2;
    }
    
    Configs::resourceManager->Load();
    bool supported = Configs::resourceManager->symlinks_supported = createSymlink(getApplicationPath(), "resources/qr243vbi.lnk.lnk");
    if (supported){
        QFile::remove("resources/qr243vbi.lnk.lnk");
    };

    
#ifdef Q_OS_UNIX
    QApplication::addLibraryPath(root_directory + "/usr/plugins");
#endif

    
    // dispatchers
    DS_cores = new QThread;
    DS_cores->start();

// icons
    QIcon::setFallbackSearchPaths(QStringList{
        ":/icon",
    });
    
    // Load dataStore
    Configs::dataStore->fn = "nekobox.cfg";
    auto isLoaded = Configs::dataStore->Load();
    if (!isLoaded) {
        Configs::dataStore->Save();
    }
    

    // icon for no theme
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName("breeze");
    }

#ifdef Q_OS_WIN
    if (Configs::dataStore->windows_set_admin && !Configs::IsAdmin() && !Configs::dataStore->disable_run_admin)
    {
        Configs::dataStore->windows_set_admin = false; // so that if permission denied, we will run as user on the next run
        Configs::dataStore->Save();
        WinCommander::runProcessElevated(getApplicationPath(), {}, "", SW_NORMAL, false);
        QApplication::quit();
        return 0;
    }
#endif

    // Datastore & Flags
    if (Configs::dataStore->start_minimal) Configs::dataStore->flag_tray = true;

    // load routing and shortcuts
    Configs::dataStore->routing = std::make_unique<Configs::Routing>();
    Configs::dataStore->routing->fn = "default_route_profile.cfg";
    isLoaded = Configs::dataStore->routing->Load();
    if (!isLoaded) {
        Configs::dataStore->routing->Save();
    }

    Configs::dataStore->shortcuts = std::make_unique<Configs::Shortcuts>();
    Configs::dataStore->shortcuts->fn = "shortcuts.cfg";
    isLoaded = Configs::dataStore->shortcuts->Load();
    if (!isLoaded) {
        Configs::dataStore->shortcuts->Save();
    }

    // Translate
    QString locale = getLocale();
    QGuiApplication::tr("QT_LAYOUT_DIRECTION");
    if (locale == "") locale = QLocale().name();
    loadTranslate(locale);

    // Check if another instance is running
    QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toBase64(QByteArray::OmitTrailingEquals);
    hashBytes.replace('+', '0').replace('/', '1');
    serverName = LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
    qDebug() << "server name: " << serverName;
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(250))
    {
        qDebug() << "Another instance is running, let's wake it up and quit";
        socket.disconnectFromServer();
        return 0;
    }

    // QLocalServer
    QLocalServer server(qApp);
    server.setSocketOptions(QLocalServer::WorldAccessOption);
    if (!server.listen(serverName)) {
        qWarning() << "Failed to start QLocalServer! Error:" << server.errorString();
        return 1;
    }
    QObject::connect(&server, &QLocalServer::newConnection, qApp, [&] {
        auto s = server.nextPendingConnection();
        qDebug() << "Another instance tried to wake us up on " << serverName << s;
        s->close();
        // raise main window
        MW_dialog_message("", "Raise");
    });
    QObject::connect(qApp, &QApplication::aboutToQuit, [&]
    {
        server.close();
        QLocalServer::removeServer(serverName);
    });

#ifdef Q_OS_UNIX
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#endif

#ifdef Q_OS_WIN
    auto eventFilter = new PowerOffTaskkillFilter(signal_handler);
    a.installNativeEventFilter(eventFilter);
#endif

    UI_InitMainWindow();
    return QApplication::exec();
}
