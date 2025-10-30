#pragma once
#include "include/configs/baseConfig.h"

namespace Configs
{

    inline QStringList tlsFingerprints = {"", "chrome", "firefox", "edge", "safari", "360", "qq", "ios", "android", "random", "randomized"};

    class uTLS : public baseConfig
    {
        public:
        bool enabled = false;
        QString fingerPrint;

        uTLS()
        {
            _add(new configItem("enabled", &enabled, boolean));
            _add(new configItem("fingerprint", &fingerPrint, string));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class ECH : public baseConfig
    {
        public:
        bool enabled = false;
        QStringList config;
        QString config_path;

        ECH()
        {
            _add(new configItem("enabled", &enabled, boolean));
            _add(new configItem("config", &config, stringList));
            _add(new configItem("config_path", &config_path, string));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class Reality : public baseConfig
    {
        public:
        bool enabled = false;
        QString public_key;
        QString short_id;

        Reality()
        {
            _add(new configItem("enabled", &enabled, boolean));
            _add(new configItem("public_key", &public_key, string));
            _add(new configItem("short_id", &short_id, string));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class TLS : public baseConfig
    {
        public:
        bool enabled = false;
        bool disable_sni = false;
        QString server_name;
        bool insecure = false;
        QStringList alpn;
        QString min_version;
        QString max_version;
        QStringList cipher_suites;
        QStringList curve_preferences;
        QString certificate;
        QString certificate_path;
        QStringList certificate_public_key_sha256;
        QString client_certificate;
        QString client_certificate_path;
        QStringList client_key;
        QString client_key_path;
        bool fragment = false;
        QString fragment_fallback_delay;
        bool record_fragment = false;
        std::shared_ptr<ECH> ech = std::make_shared<ECH>();
        std::shared_ptr<uTLS> utls = std::make_shared<uTLS>();
        std::shared_ptr<Reality> reality = std::make_shared<Reality>();

        TLS()
        {
            _add(new configItem("enabled", &enabled, boolean));
            _add(new configItem("disable_sni", &disable_sni, boolean));
            _add(new configItem("server_name", &server_name, string));
            _add(new configItem("insecure", &insecure, boolean));
            _add(new configItem("alpn", &alpn, stringList));
            _add(new configItem("min_version", &min_version, string));
            _add(new configItem("max_version", &max_version, string));
            _add(new configItem("cipher_suites", &cipher_suites, stringList));
            _add(new configItem("curve_preferences", &curve_preferences, stringList));
            _add(new configItem("certificate", &certificate, string));
            _add(new configItem("certificate_path", &certificate_path, string));
            _add(new configItem("certificate_public_key_sha256", &certificate_public_key_sha256, stringList));
            _add(new configItem("client_certificate", &client_certificate, string));
            _add(new configItem("client_certificate_path", &client_certificate_path, string));
            _add(new configItem("client_key", &client_key, stringList));
            _add(new configItem("client_key_path", &client_key_path, string));
            _add(new configItem("fragment", &fragment, boolean));
            _add(new configItem("fragment_fallback_delay", &fragment_fallback_delay, string));
            _add(new configItem("record_fragment", &record_fragment, boolean));
            _add(new configItem("ech", dynamic_cast<JsonStore *>(ech.get()), jsonStore));
            _add(new configItem("utls", dynamic_cast<JsonStore *>(utls.get()), jsonStore));
            _add(new configItem("reality", dynamic_cast<JsonStore *>(reality.get()), jsonStore));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
