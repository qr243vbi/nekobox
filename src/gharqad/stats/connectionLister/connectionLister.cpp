



#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <nekobox/api/RPC.h>
#include <nekobox/ui/mainwindow_interface.h>
#include <nekobox/stats/connections/connectionLister.hpp>
#include <nekobox/dataStore/ResourceEntity.hpp>
#include <nekobox/configs/ConfigBuilder.hpp>
#include <nekobox/global/GuiUtils.hpp>

namespace Stats
{
    ConnectionLister* connection_lister = new ConnectionLister();

    // The core opens its own connections (to the remote server, for url tests,
    // subscription and ruleset downloads). They show up as nekobox_core /
    // nekobox-core and drown out real app traffic, so they are dropped here.
    static QString normalizedCoreToken(const QString& raw)
    {
        if (raw.isEmpty()) return {};
        QString name = QFileInfo(raw).fileName();
        if (name.isEmpty()) name = raw;
        name = QFileInfo(name).completeBaseName().toLower();
        name.replace(QLatin1Char('-'), QLatin1Char('_'));
        return name;
    }

    static bool isCoreOwnConnection(const QString& process, const QString& outbound)
    {
        static const QString coreBase =
            normalizedCoreToken(QStringLiteral(NKR_CORE_NAME));
        auto matches = [](const QString& token) {
            return token == QLatin1String("nekobox_core") ||
                   token == QLatin1String("nekoboxcore");
        };
        const auto processToken = normalizedCoreToken(process);
        if (matches(processToken) ||
            (!coreBase.isEmpty() && processToken == coreBase)) {
            return true;
        }
        const auto outboundToken = normalizedCoreToken(outbound);
        return matches(outboundToken) ||
               (!coreBase.isEmpty() && outboundToken == coreBase);
    }

    static QString destHost(const QString& dest)
    {
        if (dest.startsWith(QLatin1Char('['))) {
            const int end = dest.indexOf(QLatin1Char(']'));
            if (end > 1) return dest.mid(1, end - 1);
        }
        const int firstColon = dest.indexOf(QLatin1Char(':'));
        const int lastColon = dest.lastIndexOf(QLatin1Char(':'));
        if (firstColon > 0 && firstColon == lastColon) {
            return dest.left(firstColon);
        }
        return dest;
    }

    static QString destPort(const QString& dest)
    {
        if (dest.startsWith(QLatin1Char('['))) {
            const int end = dest.indexOf(QLatin1String("]:"));
            if (end >= 0) return dest.mid(end + 2);
            return {};
        }
        const int firstColon = dest.indexOf(QLatin1Char(':'));
        const int lastColon = dest.lastIndexOf(QLatin1Char(':'));
        if (firstColon > 0 && firstColon == lastColon) {
            return dest.mid(firstColon + 1);
        }
        return {};
    }

    // Windows points the TUN adapter at this resolver, so every app DNS query
    // shows up as a row to 172.19.0.2 (or the configured equivalent) with no
    // process name. They drown the table and freeze the UI on each refresh.
    static bool isTunDnsNoise(const QString& dest, const QString& process,
                              const QString& protocol)
    {
        const auto host = destHost(dest);
        const auto port = destPort(dest);
        const auto dns4 = Configs::getTunDnsAddress();
        const auto dns6 = Configs::getTunDnsAddress6();
        const auto tun4 = Configs::getTunAddress().section(QLatin1Char('/'), 0, 0);
        const auto tun6 = Configs::getTunAddress6().section(QLatin1Char('/'), 0, 0);
        if (!host.isEmpty() &&
            (host.compare(dns4, Qt::CaseInsensitive) == 0 ||
             host.compare(dns6, Qt::CaseInsensitive) == 0)) {
            return true;
        }
        const bool dnsPort = port == QLatin1String("53") ||
                             port == QLatin1String("853");
        if (dnsPort && !host.isEmpty() &&
            (host.compare(tun4, Qt::CaseInsensitive) == 0 ||
             host.compare(tun6, Qt::CaseInsensitive) == 0)) {
            return true;
        }
        if (!process.isEmpty()) return false;
        if (protocol.compare(QLatin1String("dns"), Qt::CaseInsensitive) == 0) {
            return true;
        }
        return dnsPort;
    }

    ConnectionLister::ConnectionLister()
    {
        state = std::make_shared<QSet<QString>>();
    }

    void ConnectionLister::ForceUpdate()
    {
        mu.lock();
        update();
        mu.unlock();
    }


    void ConnectionLister::Loop()
    {
        while (true)
        {
            if (stop) return;
            QThread::msleep(1000);

            if (suspend || !tab_visible || !Configs::dataStore->connection_statistics) continue;

            mu.lock();
            update();
            mu.unlock();
        }
    }

    void ConnectionLister::update()
    {
        bool ok;
        std::optional<libcore::ListConnectionsResp> resp = API::defaultClient->ListConnections(&ok);
        if (!ok)
        {
            return;
        }

        QMap<QString, ConnectionMetadata> toUpdate;
        QMap<QString, ConnectionMetadata> toAdd;
        QSet<QString> newState;
        QList<ConnectionMetadata> sorted;
        QList<ConnectionMetadata> kept;
        auto conns = resp->connections;
        const bool hideCore = Configs::dataStore->hide_core_connections;
        for (auto conn : conns)
        {
            const auto process = QString::fromUtf8(conn.process.c_str());
            const auto outbound = QString::fromUtf8(conn.outbound.c_str());
            const auto dest = QString::fromUtf8(conn.dest.c_str());
            const auto protocol = QString::fromUtf8(conn.protocol.c_str());
            if (hideCore && isCoreOwnConnection(process, outbound)) continue;
            if (isTunDnsNoise(dest, process, protocol)) continue;

            auto c = ConnectionMetadata();
            c.id = QString::fromUtf8(conn.id.c_str());
            c.createdAtMs = conn.created_at;
            c.dest = dest;
            c.upload = conn.upload;
            c.download = conn.download;
            c.domain = QString::fromUtf8(conn.domain.c_str());
            c.network = QString::fromUtf8(conn.network.c_str());
            c.outbound = outbound;
            c.process = process;
            c.protocol = protocol;
            kept.append(c);
        }

        // A busy TUN DNS flood used to push thousands of rows into the table
        // every second. Keep the ones that name an application first.
        constexpr int kMaxDisplayed = 250;
        if (kept.size() > kMaxDisplayed) {
            std::stable_partition(kept.begin(), kept.end(), [](const ConnectionMetadata& c) {
                return !c.process.isEmpty();
            });
            kept.erase(kept.begin() + kMaxDisplayed, kept.end());
        }

        for (const auto& c : kept)
        {
            if (sort == Default)
            {
                if (state->contains(c.id))
                {
                    toUpdate[c.id] = c;
                } else
                {
                    toAdd[c.id] = c;
                }
            } else
            {
                sorted.append(c);
            }
            newState.insert(c.id);
        }

        *state = std::move(newState);

        if (sort == Default)
        {
            runOnUiThread([=,this] {
                auto m = GetMainWindow();
                m->UpdateConnectionList(toUpdate, toAdd);
            });
        } else
        {
            if (sort == ByDownload)
            {
                std::sort(sorted.begin(), sorted.end(), [=,this](const ConnectionMetadata& a, const ConnectionMetadata& b)
                {
                    if (a.download == b.download) return asc ? a.id > b.id : a.id < b.id;
                    return asc ? a.download < b.download : a.download > b.download;
                });
            }
            if (sort == ByUpload)
            {
                std::sort(sorted.begin(), sorted.end(), [=,this](const ConnectionMetadata& a, const ConnectionMetadata& b)
                {
                   if (a.upload == b.upload) return asc ? a.id > b.id : a.id < b.id;
                   return asc ? a.upload < b.upload : a.upload > b.upload;
                });
            }
            if (sort == ByProcess)
            {
                std::sort(sorted.begin(), sorted.end(), [=,this](const ConnectionMetadata& a, const ConnectionMetadata& b)
                {
                    if (a.process == b.process) return asc ? a.id > b.id : a.id < b.id;
                    return asc ? a.process > b.process : a.process < b.process;
                });
            }
            if (sort == ByOutbound)
            {
                std::sort(sorted.begin(), sorted.end(), [=,this](const ConnectionMetadata& a, const ConnectionMetadata& b)
                    {
                        if (a.outbound == b.outbound) return asc ? a.id > b.id : a.id < b.id;
                        return asc ? a.outbound > b.outbound : a.outbound < b.outbound;
                    });
            }
            if (sort == ByProtocol)
            {
                std::sort(sorted.begin(), sorted.end(), [=,this](const ConnectionMetadata& a, const ConnectionMetadata& b)
                    {
                        if (a.protocol == b.protocol) return asc ? a.id > b.id : a.id < b.id;
                        return asc ? a.protocol > b.protocol : a.protocol < b.protocol;
                    });
            }
            runOnUiThread([=,this] {
                auto m = GetMainWindow();
                m->UpdateConnectionListWithRecreate(sorted);
            });
        }
    }

    void ConnectionLister::stopLoop()
    {
        stop = true;
    }

    void ConnectionLister::setSort(const ConnectionSort newSort)
    {
        if (newSort == ByTraffic)
        {
            if (sort == ByDownload && asc)
            {
                sort = ByUpload;
                asc = false;
                return;
            }
            if (sort == ByUpload && asc)
            {
                sort = ByDownload;
                asc = false;
                return;
            }
            if (sort == ByDownload)
            {
                asc = true;
                return;
            }
            if (sort == ByUpload)
            {
                asc = true;
                return;
            }
            sort = ByDownload;
            asc = false;
            return;
        }
        if (sort == newSort) asc = !asc;
        else
        {
            sort = newSort;
            asc = false;
        }
    }

}
