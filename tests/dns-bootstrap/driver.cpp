#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <iostream>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

// Only the settings read by the extracted pure functions. No application,
// profiles, transport, DNS lookups, settings persistence or core is created.
namespace Configs {
struct Routing { QString remote_dns; } routing;
struct Settings {
  Routing *routing = &Configs::routing;
  int core_box_clash_api = 0;
  QString core_box_clash_listen_addr = "127.0.0.1";
  QString core_box_clash_api_secret;
} settings;
Settings *dataStore = &settings;
}
using Configs::dataStore;

#include "production.inc"

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(QByteArray::fromStdString(line), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return 2;
    auto row = document.object();
    Configs::routing.remote_dns = row["remote"].toString();
    auto original = row["input"].toObject();
    auto imported = row["sanitize"].toBool(true)
                        ? sanitizeSingBoxConfig(original) : original;
    row["imported"] = imported;
    QJsonArray passes;
    auto current = imported;
    for (int pass = 0; pass < row["passes"].toInt(1); ++pass) {
      NormalizeFullConfigDnsForRuntime(current);
      passes.append(current);
    }
    auto repeated = imported;
    NormalizeFullConfigDnsForRuntime(repeated);
    row["repeated_original"] = repeated;
    row["passes"] = passes;
    row["actual"] = current;
    std::cout << QJsonDocument(row).toJson(QJsonDocument::Compact).toStdString() << '\n';
  }
}
