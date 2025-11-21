#include "include/api/RPC.h"
#include "include/global/Configs.hpp"
#include <QDebug>

#include <QGrpcHttp2Channel>
#include <qtgrpcnamespace.h>

#include <optional>
#include <qnamespace.h>

namespace API {

    Client::Client(std::function<void(const QString &)> onError, const QString &host, int port) {
        QString address = "http://";
        address += host;          
        address += ":";                         
        address += QString::number(port);
        this->address = address;
        this->onError = std::move(onError);
    }

#define CHECK(method) \
bool is_running = true;                                  \
if (!Configs::dataStore->core_running) {                                               \
    MW_show_log("Cannot invoke method " + QString(method) + ", core is not running");  \
    is_running = false; \
} else {    \
  /*  qDebug() << "Invoke" << QString(method);*/  \
}

#define CHANNEL(X, VAL)                                            \
libcore::LibcoreService::Client client;                             \
auto channel = std::make_shared<QGrpcHttp2Channel>(                 \
    this->address                                                   \
); client.attachChannel(channel);                                   \
std::unique_ptr<QGrpcCallReply> call = client.X(request);           \
        QEventLoop GrpcLoop;                                        \
        QGrpcStatus status;                                         \
        QObject::connect(call.get(),                                \
    &QGrpcCallReply::finished,                                      \
    &GrpcLoop, [&status, &GrpcLoop]( const QGrpcStatus & stat){           \
        status = stat; GrpcLoop.exit();                             \
    },                                                              \
    Qt::SingleShotConnection);                                      \
        GrpcLoop.exec();                                            \
        auto reply = call->read<libcore::VAL>();
            

#define NOT_OK                                                      \
    *rpcOK = false;                                                 \
    onError(QString("LibcoreService error: %1\n").     \
    arg(status.message())                          \
);

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        CHECK("Start")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        CHANNEL(Start, ErrorResp)

        if(status.isOk()) {
            *rpcOK = true;
            return (reply->error());
        } else {
            NOT_OK
            return  status.message();
        }
    }

    QString Client::Stop(bool *rpcOK) {
        CHECK("Stop")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        libcore::EmptyReq request;
        CHANNEL(Stop, ErrorResp)
        
        if(status.isOk()) {
            *rpcOK = true;
            return (reply->error());
        } else {
            NOT_OK
            return status.message();
        }
    }

    std::optional<libcore::QueryStatsResp> Client::QueryStats() {
        CHECK("QueryStats")
        if (!is_running){
            return std::nullopt;
        }
        libcore::EmptyReq request;
        CHANNEL(QueryStats, QueryStatsResp)

        if(status.isOk()) {
            return reply;
        } else {
            return std::nullopt;
        }
    }

    std::optional<libcore::TestResp> Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        CHECK("Test")
        if (!is_running){
            *rpcOK = false;
            return std::nullopt;
        }
        CHANNEL(Test, TestResp)
    
        if(status.isOk()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return std::nullopt;
        }
    }

    void Client::StopTests(bool *rpcOK) {
        CHECK("StopTests")
        
        if (!is_running){
            *rpcOK = false;
            return;
        }
        libcore::EmptyReq request;
        CHANNEL(StopTest, EmptyResp)

        if(status.isOk()) {
            *rpcOK = true;
            return;
        } else {
            NOT_OK
            return;
        }
    }

    std::optional<libcore::QueryURLTestResponse> Client::QueryURLTest(bool *rpcOK)
    {
        CHECK("QueryURLTest")
        if (!is_running){
            *rpcOK = false;
            return std::nullopt;
        }
        libcore::EmptyReq request;
        CHANNEL(QueryURLTest, QueryURLTestResponse)
     
        if(status.isOk()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return std::nullopt;
        }
    }

    QString Client::SetSystemDNS(bool *rpcOK, const bool clear) const {
        CHECK("SetSystemDNS")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        
        libcore::SetSystemDNSRequest request;
        request.setClear(clear);
        libcore::EmptyResp resp;
        CHANNEL(SetSystemDNS, EmptyResp)

        if(status.isOk()) {
            *rpcOK = true;
            return "";
        } else {
            NOT_OK
            return (status.message());
        }
    }

    std::optional<libcore::ListConnectionsResp> Client::ListConnections(bool* rpcOK) const
    {
        CHECK("ListConnections")
        if (!is_running){
            *rpcOK = false;
            return std::nullopt;
        }
    
        libcore::EmptyReq request;
        CHANNEL(ListConnections, ListConnectionsResp)
        if(status.isOk()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            MW_show_log(QString("Failed to list connections: ") + 
                (status.message()));
            return std::nullopt;
        }
    }

    QString Client::CheckConfig(bool* rpcOK, const QString& config) const
    {
        *rpcOK = true;
        return "";
        /*
        CHECK("CheckConfig")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        libcore::LoadConfigReq request;
        request.setCoreConfig(config);
        CHANNEL(CheckConfig, ErrorResp)
        if(status.isOk())
        {
            *rpcOK = true;
            return (reply->error());
        } else
        {
            NOT_OK
            return (status.message());
        }
            */
    }

    bool Client::IsPrivileged(bool* rpcOK) const
    {
        CHECK("IsPrivileged")
        if (!is_running) {
            *rpcOK = false;
            return false;
        }

        libcore::EmptyReq request;
        CHANNEL(IsPrivileged, IsPrivilegedResponse)
        if(status.isOk()) {
            *rpcOK = true;
            return reply->hasPrivilege();
        } else {
            NOT_OK
            return false;
        }
    }

    std::optional<libcore::SpeedTestResponse> Client::SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request)
    {
        CHECK("SpeedTest")
        if (!is_running){
            *rpcOK = false;
            return std::nullopt;
        }
        CHANNEL(SpeedTest, SpeedTestResponse)

        if(status.isOk()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    std::optional<libcore::QuerySpeedTestResponse> Client::QueryCurrentSpeedTests(bool *rpcOK)
    {
        CHECK("QueryCurrentSpeedTests")
        if (!is_running){
            *rpcOK = false;
            return std::nullopt;
        }
        libcore::EmptyReq request;
        CHANNEL(QuerySpeedTest, QuerySpeedTestResponse)

        if(status.isOk()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return std::nullopt;
        }
    }

    std::optional<libcore::QueryCountryTestResponse> Client::QueryCountryTestResults(bool* rpcOK)
    {
        CHECK("QueryCountryTestResults")
        if (!is_running){
            *rpcOK = false;
            return std::nullopt;
        }
        const libcore::EmptyReq request;
        CHANNEL(QueryCountryTest, QueryCountryTestResponse)

        if(status.isOk()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

} // namespace API
