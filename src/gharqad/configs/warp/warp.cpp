#include <QJsonArray>
#include <QObject>
#include <nekobox/configs/warp/warp.hpp>
#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/dataStore/HTTPRequestHelper.hpp>
#include <QEventLoop>
#include <QTimer>

namespace Configs_network {
std::shared_ptr<warpConfig> genWarpConfig(QString *error, QString privateKey,
                                          QString publicKey) {
  std::shared_ptr<warpConfig> config = std::make_shared<warpConfig>();

  auto OSStr = getOSString();
  QJsonObject payload = {
      {"key", publicKey},
      {"install_id", ""},
      {"warp_enabled", true},
      {"tos", QDateTime::currentDateTimeUtc().toString(
                  "yyyy-MM-ddTHH:mm:ss.000+00:00")},
      {"type", OSStr},
      {"locale", "en_US"},
  };

  auto rawRequest = QJsonObject2QString(payload, false);


  HTTPResponse response;

  QEventLoop loop;

    // Use a single-shot timer to simulate a background network task
    QTimer::singleShot(2000, [&loop, &response, rawRequest]() {
      #ifdef DEBUG_MODE  
      qDebug() << "Warp task executing asynchronously in background!";
      #endif
        QMap<QString, QString> headers;
        headers["User-Agent"] = "WARP for Android";
        headers["Content-Type"] = "application/json";
        response = NetworkRequestHelper::HttpPost(warpApiURL, false, headers, rawRequest.toUtf8());
        
        // When your lambda's logic finishes, tell the event loop to exit
        loop.quit(); 
    });

    // Pause execution right here. The function will hold while the 
    // background loop keeps the UI fully responsive.
    loop.exec(); 
  
  
  QString rawResponse;
  if (response.error.isEmpty()) {
    rawResponse = QString(response.data);
    auto jsonResp = QString2QJsonObject(rawResponse)["config"].toObject();
    if (!jsonResp.contains("peers") || !jsonResp["peers"].isArray() ||
        jsonResp["peers"].toArray().empty()) {
      goto error_resp;
    }

    auto peerObj = jsonResp["peers"].toArray()[0].toObject();
    if (peerObj.isEmpty()) {
      goto error_resp;
    }

    config->privateKey = std::move(privateKey);
    if (auto pubKey = peerObj["public_key"].toString(); !pubKey.isEmpty()) {
      config->publicKey = pubKey;
    } else {      
      goto error_resp;
    }

    config->endpoint = peerObj["endpoint"].toObject()["host"].toString();
    if (config->endpoint.isEmpty()) {
      config->endpoint = "engage.cloudflareclient.com:2408";
    }

    auto ifcAddrObj = jsonResp["interface"].toObject()["addresses"].toObject();
    if (ifcAddrObj.isEmpty()) {
      goto error_resp;
    }
    config->ipv4Address = ifcAddrObj["v4"].toString();
    config->ipv6Address = ifcAddrObj["v6"].toString();
  } else {
    *error = response.error;
  }

  return config;

  error_resp: 

  *error = QObject::tr("Received invalid response: %1\nRequest payload was: %2").arg(rawResponse, rawRequest);
  return config;

}
} // namespace Configs_network