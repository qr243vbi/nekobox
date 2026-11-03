
#include <nekobox/sys/XrayURLTest.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#endif

#include <cpr/cpr.h>

#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <string>
#include <string_view>
#include <QJsonArray>

namespace Configs {

    namespace {

        // ------------------------------------------------------------
        // Native socket abstraction
        // ------------------------------------------------------------

        #ifdef _WIN32

        using NativeSocket = SOCKET;
        using SocketLength = int;

        constexpr NativeSocket InvalidSocket = INVALID_SOCKET;

        bool InitializeSockets()
        {
            static const bool initialized = [] {
                WSADATA data{};
                return WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }();

            return initialized;
        }

        void CloseSocket(NativeSocket socket)
        {
            if (socket != InvalidSocket)
                closesocket(socket);
        }

        #else

        using NativeSocket = int;
        using SocketLength = socklen_t;

        constexpr NativeSocket InvalidSocket = -1;

        bool InitializeSockets()
        {
            return true;
        }

        void CloseSocket(NativeSocket socket)
        {
            if (socket != InvalidSocket)
                close(socket);
        }

        #endif

        struct SocketGuard {
            NativeSocket socket = InvalidSocket;

            ~SocketGuard()
            {
                CloseSocket(socket);
            }

            SocketGuard(const SocketGuard &) = delete;
            SocketGuard &operator=(const SocketGuard &) = delete;

            SocketGuard() = default;
        };

        // ------------------------------------------------------------
        // Allocate an available loopback port
        // ------------------------------------------------------------

        quint16 ReserveLocalPort()
        {
            if (!InitializeSockets())
                return 0;

            SocketGuard guard;

            guard.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            if (guard.socket == InvalidSocket)
                return 0;

            sockaddr_in address{};

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = 0;

            if (bind(
                guard.socket,
                reinterpret_cast<sockaddr *>(&address),
                     sizeof(address)) != 0) {
                return 0;
                     }

                     SocketLength length = sizeof(address);

                     if (getsockname(
                         guard.socket,
                         reinterpret_cast<sockaddr *>(&address),
                                     &length) != 0) {
                         return 0;
                                     }

                                     return ntohs(address.sin_port);
        }

        // ------------------------------------------------------------
        // SOCKS5 readiness check
        //
        // A TCP connection alone is insufficient: another process
        // could have acquired the port between allocation and startup.
        //
        // Perform a SOCKS5 greeting and validate the response instead.
        // ------------------------------------------------------------

        bool CheckSocksListener(quint16 port)
        {
            if (!InitializeSockets())
                return false;

            SocketGuard guard;

            guard.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            if (guard.socket == InvalidSocket)
                return false;

            #ifdef _WIN32

            DWORD timeout = 200;

            setsockopt(
                guard.socket,
                SOL_SOCKET,
                SO_RCVTIMEO,
                reinterpret_cast<const char *>(&timeout),
                       sizeof(timeout));

            setsockopt(
                guard.socket,
                SOL_SOCKET,
                SO_SNDTIMEO,
                reinterpret_cast<const char *>(&timeout),
                       sizeof(timeout));

            #else

            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = 200000;

            setsockopt(
                guard.socket,
                SOL_SOCKET,
                SO_RCVTIMEO,
                &timeout,
                sizeof(timeout));

            setsockopt(
                guard.socket,
                SOL_SOCKET,
                SO_SNDTIMEO,
                &timeout,
                sizeof(timeout));

            #endif

            sockaddr_in address{};

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(port);

            if (connect(
                guard.socket,
                reinterpret_cast<sockaddr *>(&address),
                        sizeof(address)) != 0) {
                return false;
                        }

                        // SOCKS5 greeting:
                        // Version 5, one authentication method, no authentication.
                        const unsigned char greeting[] = {
                            0x05, 0x01, 0x00
                        };

                        #ifdef _WIN32

                        const int sent = send(
                            guard.socket,
                            reinterpret_cast<const char *>(greeting),
                                              sizeof(greeting),
                                              0);

                        #else

                        const int sent = static_cast<int>(send(
                            guard.socket,
                            greeting,
                            sizeof(greeting),
                                                               MSG_NOSIGNAL));

                        #endif

                        if (sent != sizeof(greeting))
                            return false;

            unsigned char response[2]{};

            const int received = recv(
                guard.socket,
                reinterpret_cast<char *>(response),
                                      sizeof(response),
                                      MSG_WAITALL);

            return received == sizeof(response) &&
            response[0] == 0x05 &&
            response[1] == 0x00;
        }

        // ------------------------------------------------------------
        // HTTP response header handling
        // ------------------------------------------------------------

        struct HeaderState {
            long status = 0;
            QString location;
            bool complete = false;
        };

        bool IsRedirect(long status)
        {
            switch (status) {
                case 301:
                case 302:
                case 303:
                case 307:
                case 308:
                    return true;
                default:
                    return false;
            }
        }

        bool ProcessHeaderLine(
            HeaderState &headers,
            std::string_view line)
        {
            const QString value = QString::fromUtf8(
                line.data(),
                                                    static_cast<qsizetype>(line.size()));

            // HTTP response status line.
            if (value.startsWith("HTTP/")) {
                const auto parts = value.split(
                    ' ',
                    Qt::SkipEmptyParts);

                if (parts.size() >= 2) {
                    bool ok = false;

                    const long status = parts[1].toLong(&ok);

                    if (ok) {
                        headers.status = status;
                        headers.location.clear();
                        headers.complete = false;
                    }
                }

                return true;
            }

            // End of response headers.
            if (value == "\r\n" || value == "\n") {

                // Ignore informational responses and wait for the
                // actual final response.
                if (headers.status >= 100 &&
                    headers.status < 200 &&
                    headers.status != 101) {
                    return true;
                    }

                    headers.complete = true;

                // Abort intentionally: we only need the headers.
                return false;
            }

            // Location header.
            const qsizetype separator = value.indexOf(':');

            if (separator > 0) {
                const QString name = value.left(separator).trimmed();

                if (name.compare(
                    "Location",
                    Qt::CaseInsensitive) == 0) {
                    headers.location =
                    value.mid(separator + 1).trimmed();
                    }
            }

            return true;
        }

        QString ResponseError(const cpr::Response &response)
        {
            if (!response.error.message.empty()) {
                return QString::fromStdString(
                    response.error.message);
            }

            return QStringLiteral("HTTP request failed");
        }

    } // namespace

    // ============================================================
    // Main implementation
    // ============================================================

    XrayURLTestResult TestXrayURL(
        const QString &program,
        QJsonObject config,
        const QUrl &url,
        int timeoutMs,
        const std::atomic<bool> &cancel)
    {
        XrayURLTestResult result;

        if (cancel.load()) {
            result.cancelled = true;
            return result;
        }

        // --------------------------------------------------------
        // Validate URL
        // --------------------------------------------------------

        if (!url.isValid() ||
            url.host().isEmpty() ||
            (url.scheme() != "http" &&
            url.scheme() != "https") ||
            timeoutMs <= 0) {
            result.error = "Invalid URL test address or timeout";
        return result;
            }

            // --------------------------------------------------------
            // Temporary directory
            // --------------------------------------------------------

            QTemporaryDir directory;

            if (!directory.isValid()) {
                result.error =
                "Cannot create temporary Xray test directory";
            return result;
            }

            // --------------------------------------------------------
            // Allocate local SOCKS port
            // --------------------------------------------------------

            const quint16 port = ReserveLocalPort();

            if (port == 0) {
                result.error =
                "Cannot allocate local SOCKS port";
            return result;
            }

            // --------------------------------------------------------
            // Configure SOCKS inbound
            // --------------------------------------------------------

            auto inbounds = config["inbounds"].toArray();

            if (inbounds.size() != 1 ||
                inbounds[0].toObject()["protocol"] != "socks") {
                result.error =
                "Xray URL test requires one SOCKS inbound";
            return result;
                }

                auto inbound = inbounds[0].toObject();

                inbound["listen"] = "127.0.0.1";
                inbound["port"] = int(port);

                inbounds[0] = inbound;
                config["inbounds"] = inbounds;

                // --------------------------------------------------------
                // Write temporary Xray configuration
                // --------------------------------------------------------

                QFile file(directory.filePath("test.json"));

                const auto bytes =
                QJsonDocument(config).toJson(QJsonDocument::Compact);

                if (!file.open(QIODevice::WriteOnly) ||
                    file.write(bytes) != bytes.size()) {
                    result.error =
                    "Cannot write temporary Xray test config";
                return result;
                    }

                    file.close();

                    // --------------------------------------------------------
                    // Start Xray
                    // --------------------------------------------------------

                    QProcess process;

                    process.setStandardOutputFile(QProcess::nullDevice());
                    process.setStandardErrorFile(QProcess::nullDevice());

                    struct ProcessCleanup {
                        QProcess &process;

                        ~ProcessCleanup()
                        {
                            if (process.state() != QProcess::NotRunning) {
                                process.kill();
                                process.waitForFinished(3000);
                            }
                        }
                    } cleanup{process};

                    process.start(program, {
                        "run",
                        "-format", "json",
                        "-c", file.fileName()
                    });

                    if (!process.waitForStarted(qMin(timeoutMs, 3000))) {
                        result.error =
                        "Cannot start Xray URL test: " +
                        process.errorString();
                        return result;
                    }

                    // --------------------------------------------------------
                    // Wait for SOCKS5 listener
                    // --------------------------------------------------------

                    QElapsedTimer startup;
                    startup.start();

                    bool ready = false;

                    while (startup.elapsed() < qMin(timeoutMs, 5000)) {

                        if (cancel.load()) {
                            result.cancelled = true;
                            return result;
                        }

                        if (process.state() == QProcess::NotRunning)
                            break;

                        if (CheckSocksListener(port)) {
                            ready = true;
                            break;
                        }

                        QThread::msleep(50);
                    }

                    if (!ready) {
                        if (cancel.load()) {
                            result.cancelled = true;
                            return result;
                        }

                        result.error = "Xray test listener did not start";
                        return result;
                    }

                    // --------------------------------------------------------
                    // HTTP request through Xray SOCKS5
                    // --------------------------------------------------------

                    const QString proxy =
                    QStringLiteral("socks5h://127.0.0.1:%1").arg(port);

                    QElapsedTimer elapsed;
                    elapsed.start();

                    QUrl currentUrl = url;

                    constexpr int maxRedirects = 10;

                    for (int redirect = 0;
                         redirect <= maxRedirects;
        ++redirect) {

                        if (cancel.load()) {
                            result.cancelled = true;
                            return result;
                        }

                        if (process.state() == QProcess::NotRunning) {
                            result.error =
                            "Xray exited during URL test";
                    return result;
                        }

                        const int remaining =
                        timeoutMs - static_cast<int>(elapsed.elapsed());

                        if (remaining <= 0) {
                            result.error = "Xray URL test timed out";
                            return result;
                        }

                        HeaderState headers;

                        cpr::Session session;

                        session.SetUrl(cpr::Url{
                            currentUrl.toString(
                                QUrl::FullyEncoded).toStdString()
                        });

                        session.SetProxies(cpr::Proxies{
                            {"http", proxy.toStdString()},
                                           {"https", proxy.toStdString()}
                        });

                        // Handle redirects ourselves.
                        session.SetRedirect(cpr::Redirect{false});

                        session.SetTimeout(cpr::Timeout{remaining});

                        session.SetConnectTimeout(
                            cpr::ConnectTimeout{qMin(remaining, 3000)}
                        );

                        session.SetVerifySsl(cpr::VerifySsl{true});

                        // Stop once the final response headers are received.
                        session.SetHeaderCallback(
                            cpr::HeaderCallback{
                                [&headers](
                                    std::string_view line,
                                    intptr_t) -> bool {
                                        return ProcessHeaderLine(headers, line);
                                    }
                            }
                        );

                        // Cancellation and child process monitoring.
                        session.SetProgressCallback(
                            cpr::ProgressCallback{
                                [&cancel, &process](
                                    cpr::cpr_off_t,
                                    cpr::cpr_off_t,
                                    cpr::cpr_off_t,
                                    cpr::cpr_off_t,
                                    intptr_t) -> bool {

                                        return !cancel.load() &&
                                        process.state() != QProcess::NotRunning;
                                    }
                            }
                        );

                        // The callback intentionally aborts after headers.
                        // Therefore, a callback-abort error is expected when
                        // headers.complete is true.
                        const cpr::Response response = session.Get();

                        if (cancel.load()) {
                            result.cancelled = true;
                            return result;
                        }

                        if (process.state() == QProcess::NotRunning) {
                            result.error =
                            "Xray exited during URL test";
                                           return result;
                        }

                        if (!headers.complete) {

                            if (elapsed.elapsed() >= timeoutMs) {
                                result.error = "Xray URL test timed out";
                            } else {
                                result.error = ResponseError(response);
                            }

                            return result;
                        }

                        // ----------------------------------------------------
                        // Redirect handling
                        // ----------------------------------------------------

                        if (IsRedirect(headers.status) &&
                            !headers.location.isEmpty()) {

                            if (redirect == maxRedirects) {
                                result.error = "Too many HTTP redirects";
                                return result;
                            }

                            const QUrl nextUrl =
                            currentUrl.resolved(
                                QUrl(headers.location));

                            if (!nextUrl.isValid() ||
                                nextUrl.host().isEmpty() ||
                                (nextUrl.scheme() != "http" &&
                                nextUrl.scheme() != "https")) {
                                result.error =
                                "Invalid HTTP redirect URL";
                            return result;
                                }

                                // NoLessSafeRedirectPolicy:
                                // HTTPS -> HTTP is not allowed.
                                if (currentUrl.scheme() == "https" &&
                                    nextUrl.scheme() != "https") {
                                    result.error =
                                    "Unsafe HTTPS downgrade redirect";
                                return result;
                                    }

                                    currentUrl = nextUrl;
                                    continue;
                            }

                            // ----------------------------------------------------
                            // Successful final response
                            // ----------------------------------------------------

                            result.latencyMs = qMax(
                                1,
                                static_cast<int>(elapsed.elapsed())
                            );

                            return result;
        }

        result.error = "Too many HTTP redirects";
        return result;
    }

} // namespace Configs
