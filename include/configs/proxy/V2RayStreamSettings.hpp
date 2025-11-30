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

        #define d_add(X, Y, B) _put(ptr, X, &this->Y, itemType::B)
        
        V2rayStreamSettings() : JsonStore() {
        }
        INIT_MAP_1
            d_add("net", network, string);
            d_add("sec", security, string);
            d_add("pac_enc", packet_encoding, string);
            d_add("path", path, string);
            d_add("host", host, string);
            d_add("sni", sni, string);
            d_add("alpn", alpn, string);
            d_add("cert", certificate, string);
            d_add("insecure", allow_insecure, boolean);
            d_add("headers", headers, string);
            d_add("h_type", header_type, string);
            d_add("method", method, string);
            d_add("ed_name", ws_early_data_name, string);
            d_add("ed_len", ws_early_data_length, integer);
            d_add("xhttp_mode", xhttp_mode, string);
            d_add("xhttp_extra", xhttp_extra, string);
            d_add("utls", utlsFingerprint, string);
            d_add("tls_frag", enable_tls_fragment, boolean);
            d_add("tls_frag_fall_delay", tls_fragment_fallback_delay, string);
            d_add("tls_record_frag", enable_tls_record_fragment, boolean);
            d_add("pbk", reality_pbk, string);
            d_add("sid", reality_sid, string);
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
