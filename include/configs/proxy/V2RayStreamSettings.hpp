#pragma once

#include "AbstractBean.hpp"
#include "Preset.hpp"

namespace Configs {
    class V2rayStreamSettings : public JsonStore {
    public:
        QString network = "tcp";
        QString security = "";
        QString packet_encoding = "";

        QString path = "";
        QString host = "";
        QString method = "";
        QString headers = "";
        QString header_type = "";

        QString sni = "";
        QString alpn = "";
        QString certificate = "";
        QString utlsFingerprint = "";
        bool enable_tls_fragment = false;
        QString tls_fragment_fallback_delay;
        bool enable_tls_record_fragment = false;
        bool allow_insecure = false;
        // ws early data
        QString ws_early_data_name = "";
        int ws_early_data_length = 0;
        // xhttp
        QString xhttp_mode = "auto";
        QString xhttp_extra = "";
        // reality
        QString reality_pbk = "";
        QString reality_sid = "";
                
        V2rayStreamSettings() : JsonStore() {
        }
        INIT_MAP_1
            ADD_MAP("net", network, string);
            ADD_MAP("sec", security, string);
            ADD_MAP("pac_enc", packet_encoding, string);
            ADD_MAP("path", path, string);
            ADD_MAP("host", host, string);
            ADD_MAP("sni", sni, string);
            ADD_MAP("alpn", alpn, string);
            ADD_MAP("cert", certificate, string);
            ADD_MAP("insecure", allow_insecure, boolean);
            ADD_MAP("headers", headers, string);
            ADD_MAP("h_type", header_type, string);
            ADD_MAP("method", method, string);
            ADD_MAP("ed_name", ws_early_data_name, string);
            ADD_MAP("ed_len", ws_early_data_length, integer);
            ADD_MAP("xhttp_mode", xhttp_mode, string);
            ADD_MAP("xhttp_extra", xhttp_extra, string);
            ADD_MAP("utls", utlsFingerprint, string);
            ADD_MAP("tls_frag", enable_tls_fragment, boolean);
            ADD_MAP("tls_frag_fall_delay", tls_fragment_fallback_delay, string);
            ADD_MAP("tls_record_frag", enable_tls_record_fragment, boolean);
            ADD_MAP("pbk", reality_pbk, string);
            ADD_MAP("sid", reality_sid, string);
        STOP_MAP

        void BuildStreamSettingsSingBox(QJsonObject *outbound);

        QMap<QString, QString> GetHeaderPairs(bool* ok) {
            bool inQuote = false;
            QString curr;
            QStringList list;
            for (const auto &ch: headers) {
                if (inQuote) {
                    if (ch == '"') {
                        inQuote = false;
                        list << curr;
                        curr = "";
                        continue;
                    } else {
                        curr += ch;
                        continue;
                    }
                }
                if (ch == '"') {
                    inQuote = true;
                    continue;
                }
                if (ch == ' ') {
                    if (!curr.isEmpty()) {
                        list << curr;
                        curr = "";
                    }
                    continue;
                }
                if (ch == '=') {
                    if (!curr.isEmpty()) {
                        list << curr;
                        curr = "";
                    }
                    continue;
                }
                curr+=ch;
            }
            if (!curr.isEmpty()) list<<curr;

            if (list.size()%2 == 1) {
                *ok = false;
                return {};
            }
            QMap<QString,QString> res;
            for (int i = 0; i < list.size(); i+=2) {
                res[list[i]] = list[i + 1];
            }
            *ok = true;
            return res;
        }
    };

    inline V2rayStreamSettings *GetStreamSettings(AbstractBean *bean) {
        if (bean == nullptr) return nullptr;
        auto stream_item = bean->_get("stream");
        if (stream_item != nullptr) {
            auto stream_store = *(JsonStore **) stream_item->getPtr(bean);
            auto stream = (Configs::V2rayStreamSettings *) stream_store;
            return stream;
        }
        return nullptr;
    }
} // namespace Configs
