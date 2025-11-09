
#include <csignal>

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QTranslator>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QLocalServer>
#include <QThread>
#include <QFileInfo>

#ifdef Q_OS_WIN

#include <3rdparty/WinCommander.hpp>
#include <windows.h>

#include <iostream>
#include "include/sys/windows/MiniDump.h"
#include "include/sys/windows/eventHandler.h"
#include "include/sys/windows/WinVersion.h"
#include <qfontdatabase.h>
#endif


#include "include/global/Configs.hpp"

#include "include/ui/mainwindow_interface.h"

#ifdef Q_OS_LINUX
#include <qfontdatabase.h>
#endif

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

bool isFileAppendable(const QString &filePath) {
    QFile file(filePath);
    // Check if the file exists and is writable in append mode
    if (file.exists()){
        if (file.open(QIODevice::Append)){
            file.close();
            return true;
        }
    } else {
        if (file.open(QIODevice::WriteOnly)){
            file.close();
            return true;
        }
    }
    return false;
}


int main(int argc, char** argv) {
    // Core dump
#ifdef Q_OS_WIN
    Windows_SetCrashHandler();
#endif

    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication a(argc, argv);

#if !defined(Q_OS_MACOS) && (QT_VERSION >= QT_VERSION_CHECK(6,9,0))
    int fontId = -1 ; //QFontDatabase::addApplicationFont(":/font/notoEmoji");

    if (fontId >= 0)
    {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        QFontDatabase::setApplicationEmojiFontFamilies(fontFamilies);
    } else
    {
        qDebug() << "could not load noto font!";
    }
#endif

    // Clean
    QDir::setCurrent(QApplication::applicationDirPath());
    if (QFile::exists("updater.old")) {
        QFile::remove("updater.old");
    }

    // Flags
    Configs::dataStore->argv = QApplication::arguments();
    if (Configs::dataStore->argv.contains("-many")) Configs::dataStore->flag_many = true;
    if (Configs::dataStore->argv.contains("-appdata")) {
        Configs::dataStore->flag_use_appdata = true;
        int appdataIndex = Configs::dataStore->argv.indexOf("-appdata");
        if (Configs::dataStore->argv.size() > appdataIndex + 1 && !Configs::dataStore->argv.at(appdataIndex + 1).startsWith("-")) {
            Configs::dataStore->appdataDir = Configs::dataStore->argv.at(appdataIndex + 1);
        }
    }
    if (Configs::dataStore->argv.contains("-tray")) Configs::dataStore->flag_tray = true;
    if (Configs::dataStore->argv.contains("-debug")) Configs::dataStore->flag_debug = true;
    if (Configs::dataStore->argv.contains("-flag_restart_tun_on")) Configs::dataStore->flag_restart_tun_on = true;
    if (Configs::dataStore->argv.contains("-flag_restart_dns_set")) Configs::dataStore->flag_dns_set = true;
#ifdef NKR_CPP_USE_APPDATA
    Configs::dataStore->flag_use_appdata = true; // Example: Package & MacOS
#endif
#ifdef NKR_CPP_DEBUG
    Configs::dataStore->flag_debug = true;
#endif
    bool use_application_dir = true;
    QApplication::setApplicationName("nekobox");
    
    bool dir_success = true;
    QDir dir;
	// dirs & clean
    auto wd = QDir(QApplication::applicationDirPath());
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
    if (!wd.exists("config")) {
        dir_success &= wd.mkdir("config");
    }
    
	{
		QString wd_abs = wd.absoluteFilePath("config");
		QDir::setCurrent(wd_abs);
		QDir("temp").removeRecursively();
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
    if (!dir.exists(ROUTES_PREFIX_NAME)) {
        dir_success &= dir.mkdir(ROUTES_PREFIX_NAME);
    }
    if (!dir_success) {
        loop_back_2:
        if (use_application_dir){
            Configs::dataStore->flag_use_appdata = true;
            goto loop_back_1;
        }
        QMessageBox::critical(nullptr, "Error", "No permission to write " + wd.absoluteFilePath("config"));
        return 1;
    }

    // migrate the old config file
    if (QFile::exists("groups/nekobox.json")) {
        QFile::rename("groups/nekobox.json", "configs.json");
    } else if (QFile::exists("groups/nekoray.json")) {
        QFile::rename("groups/nekoray.json", "configs.json");
    } else if (QFile::exists("groups/Throne.json")) {
        QFile::rename("groups/Throne.json", "configs.json");
    }
    
    dir_success &= isFileAppendable("configs.json");
    
    if (!dir_success){
        goto loop_back_2;
    }
    
#ifdef Q_OS_LINUX
    QApplication::addLibraryPath(QApplication::applicationDirPath() + "/usr/plugins");
#endif

    // dispatchers
    DS_cores = new QThread;
    DS_cores->start();

// icons
    QIcon::setFallbackSearchPaths(QStringList{
        ":/icon",
    });
    
    // Load dataStore
    Configs::dataStore->fn = "configs.json";
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
        WinCommander::runProcessElevated(QApplication::applicationFilePath(), {}, "", SW_NORMAL, false);
        QApplication::quit();
        return 0;
    }
#endif

    // Datastore & Flags
    if (Configs::dataStore->start_minimal) Configs::dataStore->flag_tray = true;

    // load routing and shortcuts
    Configs::dataStore->routing = std::make_unique<Configs::Routing>();
    Configs::dataStore->routing->fn = ROUTES_PREFIX + "Default";
    isLoaded = Configs::dataStore->routing->Load();
    if (!isLoaded) {
        Configs::dataStore->routing->Save();
    }

    Configs::dataStore->shortcuts = std::make_unique<Configs::Shortcuts>();
    Configs::dataStore->shortcuts->fn = "shortcuts.json";
    isLoaded = Configs::dataStore->shortcuts->Load();
    if (!isLoaded) {
        Configs::dataStore->shortcuts->Save();
    }

    // Translate
    QString locale;
    switch (Configs::dataStore->language) {
        case 1: // English
            break;
        case 2:
            locale = "zh_CN";
            break;
        case 3:
            locale = "fa_IR"; // farsi(iran)
            break;
        case 4:
            locale = "ru_RU"; // Russian
            break;
        default:
            locale = QLocale().name();
    }
    QGuiApplication::tr("QT_LAYOUT_DIRECTION");
    loadTranslate(locale);

    // Check if another instance is running
    QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toBase64(QByteArray::OmitTrailingEquals);
    hashBytes.replace('+', '0').replace('/', '1');
    auto serverName = LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
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

#ifdef Q_OS_LINUX
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#endif

#ifdef Q_OS_WIN
    auto eventFilter = new PowerOffTaskkillFilter(signal_handler);
    a.installNativeEventFilter(eventFilter);
#endif

#ifdef Q_OS_MACOS
    QObject::connect(qApp, &QGuiApplication::commitDataRequest, [&](QSessionManager &manager)
    {
        Q_UNUSED(manager);
        signal_handler(0);
    });
#endif

    UI_InitMainWindow();
    return QApplication::exec();
}
