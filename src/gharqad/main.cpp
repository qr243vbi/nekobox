

#include <csignal>

#include <QCryptographicHash>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTranslator>
// #include <QLocalSocket>
// #include <QLocalServer>
#include <QApplication>
#include <QFileInfo>
#include <QThread>
#include <iostream>
#include <memory>
#include <nekobox/dataStore/DatabaseRocksDB.hpp>
#include <nekobox/stats/traffic/TrafficLooper.hpp>
#include <nekobox/sys/Settings.h>
#include <qfontdatabase.h>
#include <qnamespace.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TPipe.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransport.h>

#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

#ifdef Q_OS_WIN
#include <nekobox/sys/windows/MiniDump.h>
#include <nekobox/sys/windows/WinCommander.hpp>
#include <nekobox/sys/windows/WinVersion.h>
#include <nekobox/sys/windows/eventHandler.h>
#include <windows.h>
#endif

#ifndef NKR_SOFTWARE_KEYS
#define CHECK_STARTUP_ACCESS_M
#else
#include <nekobox/ui/security_addon.h>
#endif

#include <nekobox/dataStore/ResourceEntity.hpp>
#include <nekobox/sys/Settings.h>

#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/ui/mainwindow_interface.h>

QVariantMap ruleSetMap;
QWidget *mainwindow;

MainWindow *GetMainWindow() { return (MainWindow *)mainwindow; }

#ifdef Q_OS_UNIX
#include <nekobox/sys/linux/LinuxCap.h>

QDBusPendingReply<> OrgFreedesktopPortalRequestInterface::Close() {
  QList<QVariant> argumentList;
  return asyncCallWithArgumentList(QStringLiteral("Close"), argumentList);
}

#endif
#define disable_run_admin windows_no_admin

void signal_handler(int signum) {
  auto mainwindow = GetMainWindow();
  if (mainwindow != nullptr) {
    mainwindow->on_menu_exit_triggered(true);
    qApp->quit();
  }
}

QTranslator *trans = nullptr;
// QTranslator* trans_qt = nullptr;

namespace Stats {
std::unique_ptr<DatabaseLogger> databaseLogger =
    std::make_unique<DatabaseLogger>();
}

namespace Configs {
std::shared_ptr<DatabaseManager> databaseManager;
class ResourceManager *resourceManager;
} // namespace Configs

namespace Preset {
namespace SingBox {
QMap<QString, QString> OutboundTypes;
}
} // namespace Preset

#ifdef Q_OS_UNIX
static QString getSocketPath(const QString &serverName) {
  return QDir::tempPath() + QDir::separator() + serverName + ".sock";
}
#endif

static std::shared_ptr<apache::thrift::transport::TTransport>
getLocalTransport(const QString &serverName) {

#ifdef Q_OS_UNIX
  QString filename = getSocketPath(serverName);
#endif
  return std::shared_ptr<apache::thrift::transport::TTransport>(
      new apache::thrift::transport::TPipe((
#ifdef Q_OS_UNIX
                                               filename
#else
                                               serverName
#endif
                                               )
                                               .toStdString()));
}

static std::shared_ptr<apache::thrift::transport::TServerTransport>
getLocalServerTransport(const QString &serverName) {

#ifdef Q_OS_UNIX
  QString filename = getSocketPath(serverName);
  {
    QFile file(filename);
    if (file.exists()) {
      file.remove();
    }
  }
#endif
  return std::shared_ptr<apache::thrift::transport::TServerTransport>(
      new apache::thrift::transport::TServerSocket((
#ifdef Q_OS_UNIX
                                                       filename
#else
                                                       serverName
#endif
                                                       )
                                                       .toStdString()));
}

static bool wakeExistingInstance(const QString &serverName) {
  try {
    auto transport = (getLocalTransport(serverName));
    transport->open();

    libcore::InstanceServiceClient client(
        std::make_shared<apache::thrift::protocol::TBinaryProtocol>(transport));

    client.wakeUp();

    transport->close();

    qWarning() << "Another instance is running, let's wake it up and quit";
    return true; // instance exists
  } catch (...) {
    return false; // no server running
  }
}

static void loadTranslate(QString locale) {
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
  //   if (trans_qt != nullptr) {
  //       trans_qt->deleteLater();
  //   }
  //

  // QLocale().name();
  trans = new QTranslator;
  //   trans_qt = new QTranslator;
  QLocale::setDefault(QLocale(locale));
  //
  if (trans->load(getLangResource(locale))) {
    QCoreApplication::installTranslator(trans);
  }

  Preset::SingBox::OutboundTypes = {
      {"socks", "Socks"},
      {"http", "HTTP"},
      {"mieru", "Mieru"},
      {"shadowsocks", "Shadowsocks"},
      {"chain", QObject::tr("Chain Proxy")},
      {"vmess", "VMess"},
      {"trojan", "Trojan"},
      {"vless", "VLESS"},
      {"hysteria", "Hysteria 1"},
      {"hysteria2", "Hysteria 2"},
      {"tuic", "TUIC"},
      {"anytls", "AnyTLS"},
      {"shadowtls", "ShadowTLS"},
      {"wireguard", "Wireguard"},
      {"tailscale", "Tailscale"},
      {"ssh", "SSH"},
      {"tor", "Tor"},
      {"naive", "Naive"},
      {"trusttunnel", "TrustTunnel"},
      {"juicity", "Juicity"},
      {"custom", QObject::tr("Custom")},
      {"extracore", QObject::tr("Extra Core")},
  };
}

#define LOCAL_SERVER_PREFIX "nekobox-"

int main(int argc, char **argv) {
  auto unicodepath = boost::dll::program_location();
  // Core dump
#ifdef Q_OS_WIN
  Windows_SetCrashHandler();
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    printf("WSAStartup failed: %d\n", result);
    return 1;
  }
  root_directory = QString::fromWCharArray(unicodepath.parent_path().c_str());
  software_path = QString::fromWCharArray(unicodepath.c_str());
#else
  Unix_SetCrashHandler();
  root_directory = QString::fromUtf8(unicodepath.parent_path().c_str());
  software_path = QString::fromUtf8(unicodepath.c_str());
#endif

#ifdef Q_OS_UNIX
  {
    QString qpath = root_directory;
#ifdef DEBUG_MODE
    qDebug() << qpath + "/usr/plugins";
#endif
    QApplication::addLibraryPath(qpath + "/usr/plugins");
  }
#endif

  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication::setQuitOnLastWindowClosed(false);
  QApplication a(argc, argv);

  // Flags
  Configs::dataStore->argv = QApplication::arguments();
  if (Configs::dataStore->argv.contains("-many"))
    Configs::dataStore->flag_many = true;
  if (Configs::dataStore->argv.contains("-appdata")) {
    Configs::dataStore->flag_use_appdata = true;
    int appdataIndex = Configs::dataStore->argv.lastIndexOf("-appdata") + 1;
    if (Configs::dataStore->argv.size() > appdataIndex &&
        !Configs::dataStore->argv.at(appdataIndex).startsWith("-")) {
      Configs::dataStore->appdataDir =
          Configs::dataStore->argv.at(appdataIndex);
    }
  }

  if (Configs::dataStore->argv.contains("-tray"))
    Configs::dataStore->flag_tray = true;
  if (Configs::dataStore->argv.contains("-debug"))
    Configs::dataStore->flag_debug = true;
  if (Configs::dataStore->argv.contains("-flag_restart_tun_on"))
    Configs::dataStore->flag_restart_tun_on = true;
  if (Configs::dataStore->argv.contains("-flag_restart_dns_set"))
    Configs::dataStore->flag_dns_set = true;
#ifdef NKR_CPP_DEBUG
  Configs::dataStore->flag_debug = true;
#endif
  bool use_application_dir = true;
  QApplication::setApplicationName("nekobox");

  bool dir_success = true;
  QDir dir;
  // dirs & clean
  auto wd = QDir(root_directory);
#ifdef Q_OS_UNIX
  {
    QString imagepath = getAppImage();
    if (imagepath != "") {
      QFileInfo fileinfo(imagepath);
      wd.setPath(fileinfo.absolutePath());
    }
  }
#endif
  if (Configs::dataStore->flag_use_appdata) {
    if (!Configs::dataStore->appdataDir.isEmpty()) {
      wd.setPath(Configs::dataStore->appdataDir);
    } else {
    loop_back_1:
      wd.setPath(
          QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    }
    use_application_dir = false;
  }
  dir_success = true;

  if (!wd.exists())
    wd.mkpath(wd.absolutePath());
  if (!wd.exists("settings")) {
    dir_success &= wd.mkdir("settings");
  }

  {
    QString wd_abs = wd.absoluteFilePath("settings");

#ifdef Q_OS_UNIX
    prepare_directory_for_shared_access(wd_abs.toStdString());
#endif
    QDir::setCurrent(wd_abs);
    MoveDirToTrash("temp");
    dir = QDir(wd_abs);

    QFileInfo fileInfo(wd_abs);
    dir_success &= fileInfo.exists();
    dir_success &= fileInfo.isDir();
    if (!dir_success) {
      goto loop_back_2;
    }
  }
  // Dir
  dir_success &= dir.mkdir("temp");
  if (!dir.exists("resources")) {
    dir_success &= dir.mkdir("resources");
  }
  if (!dir_success) {
  loop_back_2:
    if (use_application_dir) {
      Configs::dataStore->flag_use_appdata = true;
      goto loop_back_1;
    }
    QMessageBox::critical(nullptr, "Error",
                          "No permission to write " +
                              wd.absoluteFilePath("settings"));
    return 1;
  }
  dir_success &= isFileAppendable("window.ini");

  if (!dir_success) {
    goto loop_back_2;
  }

  // Check if another instance is running
  QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(),
                                                  QCryptographicHash::Md5)
                             .toBase64(QByteArray::OmitTrailingEquals);
  hashBytes.replace('+', '0').replace('/', '1');
  serverName = LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
#ifdef DEBUG_MODE
  qDebug() << "server name: " << serverName;
#endif
  /*
  QLocalSocket socket;
  socket.connectToServer(serverName);
  if (socket.waitForConnected(250))
  {
      #ifdef DEBUG_MODE
      qDebug() << "Another instance is running, let's wake it up and quit";
      #endif
      socket.disconnectFromServer();
      return 0;
  }
*/
  if (wakeExistingInstance(serverName)) {
    return 0;
  }

  Configs::databaseManager = std::make_shared<Configs::FileDatabaseManager>();
  Configs::resourceManager = new class Configs::ResourceManager();

  CHECK_STARTUP_ACCESS_M
  Stats::databaseLogger->Load();
  Stats::databaseLogger->initialize();
  Stats::trafficLooper->initialize();

  Configs::windowSettings->Load();
  Configs::resourceManager->Load();
  bool supported = Configs::resourceManager->symlinks_supported =
      createSymlink(getApplicationPath(), "resources/qr243vbi.lnk.lnk");
  if (supported) {
    QFile::remove("resources/qr243vbi.lnk.lnk");
    if (Configs::windowSettings->no_symlinks) {
      Configs::resourceManager->symlinks_supported = false;
    }
  };

  // dispatchers
  DS_cores = new QThread;
  DS_cores->start();

  // icons
  QIcon::setFallbackSearchPaths(QStringList{
      ":/icon",
  });

  // Load dataStore
  auto isLoaded = Configs::dataStore->Load();
  if (!isLoaded) {
    Configs::dataStore->Save();
  }

  // icon for no theme
  if (QIcon::themeName().isEmpty()) {
    QIcon::setThemeName("breeze");
  }

#ifdef Q_OS_WIN
  if (Configs::dataStore->windows_set_admin && !Configs::IsAdmin() &&
      !Configs::dataStore->disable_run_admin) {
    Configs::dataStore->windows_set_admin =
        false; // so that if permission denied, we will run as user on the next
               // run
    Configs::dataStore->Save();
    WinCommander::runProcessElevated(getApplicationPath(), {}, "", SW_NORMAL,
                                     false);
    QApplication::quit();
    return 0;
  }
#endif

  // Datastore & Flags
  if (Configs::dataStore->start_minimal)
    Configs::dataStore->flag_tray = true;

  // load routing and shortcuts
  Configs::dataStore->routing = std::make_unique<Configs::Routing>();
  // Configs::dataStore->routing->fn = "default_route_profile.cfg";
  isLoaded = Configs::dataStore->routing->Load();
  if (!isLoaded) {
    Configs::dataStore->routing->Save();
  }

  Configs::windowSettings->shortcuts =
      std::make_unique<class Configs::Shortcuts>();

  isLoaded = Configs::windowSettings->shortcuts->Load();
  Configs::windowSettings->shortcuts->Save();

  // Translate
  QString locale = getLocale();
#ifdef DEBUG_MODE
  qDebug() << "Language is: " << locale;
#endif
  QGuiApplication::tr("QT_LAYOUT_DIRECTION");
  loadTranslate(locale);

  // QLocalServer
  auto handler = std::make_shared<API::InstanceHandler>();
  QObject::connect(handler.get(), &API::InstanceHandler::wokeUp, qApp, [&] {
#ifdef DEBUG_MODE
    qDebug() << "Wake received via Thrift";
#endif

    MainWindow *window = GetMainWindow();
    if (window) {
      do {
        ToggleWindow(window);
      } while (window->isHidden());
    }
  });
  auto transport = getLocalServerTransport(serverName);
  auto processor = std::make_shared<libcore::InstanceServiceProcessor>(handler);
  auto server = std::make_shared<apache::thrift::server::TThreadedServer>(
      processor, transport,
      std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
      std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>());

  QFuture future = QtConcurrent::run([server] { server->serve(); });

  /*
  QLocalServer server(qApp);
  server.setSocketOptions(QLocalServer::WorldAccessOption);
  if (!server.listen(serverName)) {
      qWarning() << "Failed to start QLocalServer! Error:" <<
  server.errorString(); return 1;
  }
  QObject::connect(&server, &QLocalServer::newConnection, qApp, [&] {
      auto s = server.nextPendingConnection();
      #ifdef DEBUG_MODE
      qDebug() << "Another instance tried to wake us up on " << serverName << s;
      #endif
      s->close();
      // raise main window
      MainWindow * window = GetMainWindow();
      if (window != nullptr){
          do {
              ToggleWindow(window);
          } while (window->isHidden());
      }
  });
  QObject::connect(qApp, &QApplication::aboutToQuit, [&]
  {
      server.close();
      QLocalServer::removeServer(serverName);
  });
  */

#ifdef Q_OS_UNIX
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
#endif

#ifdef Q_OS_WIN
  auto eventFilter = new PowerOffTaskkillFilter(signal_handler);
  a.installNativeEventFilter(eventFilter);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  // Load the color emoji font (Twemoji COLR format — required for Qt 6.9+
  // colored emoji)
  int emojiFontId = QFontDatabase::addApplicationFont(
      getResource("emoji.ttf", {}, ":/fonts/TwemojiCOLR.ttf"));
  if (emojiFontId >= 0) {
    QStringList fontFamilies =
        QFontDatabase::applicationFontFamilies(emojiFontId);
    QFontDatabase::setApplicationEmojiFontFamilies(fontFamilies);
  } else {
    qDebug() << "could not load emoji font!";
  }
#else
  // Fallback for older Qt
  {
    int emojiFontId = QFontDatabase::addApplicationFont(
        getResource("emoji.ttf", {}, ":/fonts/NotoEmoji.ttf"));
    if (emojiFontId != -1) {
      QStringList families =
          QFontDatabase::applicationFontFamilies(emojiFontId);
      if (!families.isEmpty()) {
        QFont appFont = QApplication::font();
        QStringList fallbackFamilies;
        fallbackFamilies << appFont.family();
        fallbackFamilies << families.first();
        appFont.setFamilies(fallbackFamilies);
        QApplication::setFont(appFont);
      }
    }
  }
#endif

  UI_InitMainWindow();
  auto ret = QApplication::exec();
  server->stop();
  future.waitForFinished();
#ifdef Q_OS_UNIX
  QFile file(getSocketPath(serverName));

  if (file.exists()) {
    file.remove();
  };
#endif
  return ret;
}
