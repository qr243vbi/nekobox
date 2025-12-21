#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include "nekobox/api/RPC.h"
#include "nekobox/dataStore/Configs.hpp"
#include <QDebug>
#include <optional>
#include <qnamespace.h>

//Thrift libraries
#include <thrift/protocol/TBinaryProtocol.h>             
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TSocket.h>                    
#include <thrift/transport/TBufferTransports.h>          
#include <thrift/transport/TTransportUtils.h>
#include <gen-cpp/LibcoreService.h>
#include <gen-cpp/libcore_types.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

struct Status{
    bool ok = false;
    std::string what = "";
    bool isOk(){ return ok; }
    QString message() { return QString::fromStdString(what); }
};

namespace API {

    Client::Client(std::function<void(const QString &)> onError, const QString &host, int port) {                         
        this->port = (port);
        this->domain = host.toStdString();
        this->onError = std::move(onError);
    }

#define CHECK(method) \
bool is_running = true;                                  \
if (!Configs::dataStore->core_running) {                                               \
    MW_show_log("Cannot invoke method " + QString(method) + ", core is not running");  \
    is_running = false; \
} else {    \
}



#define CHANNEL(X, VAL)                                                                 \
std::shared_ptr<TTransport> socketAA(new TSocket(domain, port));                     \
std::shared_ptr<TTransport> transportAA(new TBufferedTransport(socketAA));                  \
std::shared_ptr<TProtocol> protocolAA(new TBinaryProtocol(transportAA));                    \
libcore::LibcoreServiceClient client(protocolAA);                                         \
Status status;                                                                          \
std::optional<libcore::VAL> reply = std::nullopt;                                       \
try{                                                                                    \
    transportAA->open();                                                                  \
    status.ok = true;                                                                   \
    libcore::VAL resp;                                                                  \
    client.X(resp, request);                                                            \
    transportAA->close();                                                                 \
    reply = std::make_optional(resp);                                                   \
} catch (TException e){                                                                 \
    status.ok = false;                                                                  \
    qDebug() << "HI CRUEL WORLD";                                                       \
    status.what = e.what();                                                             \
    qDebug() << status.what;                                                            \
}

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
            return QString::fromStdString(reply->error);
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
            return QString::fromStdString(reply->error);
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
        request.clear = (clear);
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
        CHECK("CheckConfig")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        libcore::LoadConfigReq request;
        request.core_config = (config.toStdString());
        CHANNEL(CheckConfig, ErrorResp)
        if(status.isOk())
        {
            *rpcOK = true;
            return QString::fromStdString(reply->error);
        } else
        {
            NOT_OK
            return (status.message());
        }
    }

    bool Client::IsPrivileged(bool* rpcOK) const
    {
        CHECK("IsPrivileged")
        if (!is_running) {
            *rpcOK = false;
            return false;
        }

        libcore::EmptyReq request;
        request.start = true;
        CHANNEL(IsPrivileged, IsPrivilegedResponse)
        if(status.isOk()) {
            *rpcOK = true;
            return reply->has_privilege;
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
