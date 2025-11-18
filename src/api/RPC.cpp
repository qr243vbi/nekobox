#include "include/api/RPC.h"
#include <grpcpp/client_context.h>
#include <grpcpp/grpcpp.h>
#include "core/server/gen/libcore.grpc.pb.h"
#include "include/global/Configs.hpp"
#include <QDebug>

namespace API {

    Client::Client(std::function<void(const QString &)> onError, const QString &host, int port) {
        QString address = host;          
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
qDebug() << "Invoke" << QString(method);  \
}

#define CHANNEL                         \
auto channel = grpc::CreateChannel(address.toStdString(), grpc::InsecureChannelCredentials()); \
auto stub = libcore::LibcoreService::NewStub(channel); \
grpc::ClientContext context;

#define NOT_OK      \
    *rpcOK = false; \
    onError(QString("LibcoreService error with code %2: %1\n").  \
    arg(QString::fromStdString(status.error_message()),          \
    QString::number(status.error_code()) )          \
);

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        CHECK("Start")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        CHANNEL
        libcore::ErrorResp reply;
        auto status = stub->Start(&context, request, &reply);

        if(status.ok()) {
            *rpcOK = true;
            return QString::fromStdString(reply.error());
        } else {
            NOT_OK
            return "";
        }
    }

    QString Client::Stop(bool *rpcOK) {
        CHECK("Stop")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        libcore::EmptyReq request;
        libcore::ErrorResp reply;
        CHANNEL
        auto status = stub->Stop(&context, request, &reply);

        if(status.ok()) {
            *rpcOK = true;
            return QString::fromStdString(reply.error());
        } else {
            NOT_OK
            return "";
        }
    }

    libcore::QueryStatsResp Client::QueryStats() {
        CHECK("QueryStats")
        libcore::EmptyReq request;
        libcore::QueryStatsResp reply;
        if (!is_running){
            return {};
        }
        CHANNEL
        auto status = stub->QueryStats(&context, request, &reply);

        if(status.ok()) {
            return reply;
        } else {
            return {};
        }
    }

    libcore::TestResp Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        CHECK("Test")
        if (!is_running){
            *rpcOK = false;
            return {};
        }
        libcore::TestResp reply;
        CHANNEL
        auto status = stub->Test(&context, request, &reply);

        if(status.ok()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    void Client::StopTests(bool *rpcOK) {
        CHECK("StopTests")
        if (!is_running){
            *rpcOK = false;
            return;
        }
        libcore::EmptyReq request;
        libcore::EmptyResp resp;
        CHANNEL
        auto status = stub->StopTest(&context, request, &resp);

        if(status.ok()) {
            *rpcOK = true;
            return;
        } else {
            NOT_OK
            return;
        }
    }

    libcore::QueryURLTestResponse Client::QueryURLTest(bool *rpcOK)
    {
        CHECK("QueryURLTest")
        if (!is_running){
            *rpcOK = false;
            return {};
        }
        libcore::EmptyReq req;
        libcore::QueryURLTestResponse resp;
        CHANNEL
        auto status = stub->QueryURLTest(
            &context, req, &resp);

        if(status.ok()) {
            *rpcOK = true;
            return resp;
        } else {
            NOT_OK
            return resp;
        }
    }

    QString Client::SetSystemDNS(bool *rpcOK, const bool clear) const {
        CHECK("SetSystemDNS")
        if (!is_running){
            *rpcOK = false;
            return "";
        }
        libcore::SetSystemDNSRequest request;
        request.set_clear(clear);
        libcore::EmptyResp resp;
        CHANNEL

        auto status = stub->SetSystemDNS(
            &context, request, &resp);
        if(status.ok()) {
            *rpcOK = true;
            return "";
        } else {
            NOT_OK
            return QString::fromStdString(status.error_message());
        }
    }

    libcore::ListConnectionsResp Client::ListConnections(bool* rpcOK) const
    {
        CHECK("ListConnections")
        if (!is_running){
            *rpcOK = false;
            return {};
        }
        libcore::EmptyReq req;
        libcore::ListConnectionsResp reply;
        CHANNEL
        auto status = stub->ListConnections(&context, req, &reply);
        if(status.ok()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            MW_show_log(QString("Failed to list connections: ") + 
                QString::fromStdString(status.error_message()));
            return {};
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
        libcore::ErrorResp reply;
        request.set_core_config(config.toStdString());
        CHANNEL
        auto status = stub->CheckConfig(&context, request, &reply);

        if(status.ok())
        {
            *rpcOK = true;
            return QString::fromStdString(reply.error());
        } else
        {
            NOT_OK
            return QString::fromStdString(status.error_message());
        }

    }

    bool Client::IsPrivileged(bool* rpcOK) const
    {
        CHECK("IsPrivileged")

        if (!is_running){
            *rpcOK = false;
            return false;
        }
        libcore::EmptyReq req;
        libcore::IsPrivilegedResponse reply;
        CHANNEL
        auto status = stub->IsPrivileged(&context, req, &reply);

        if(status.ok())
        {
            *rpcOK = true;
            return reply.has_privilege();
        } else
        {
            NOT_OK
            return false;
        }
    }

    libcore::SpeedTestResponse Client::SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request)
    {
        CHECK("SpeedTest")
        if (!is_running){
            *rpcOK = false;
            return {};
        }
        libcore::SpeedTestResponse reply;
        CHANNEL
        auto status = stub->SpeedTest(&context, request, &reply);

        if(status.ok()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    libcore::QuerySpeedTestResponse Client::QueryCurrentSpeedTests(bool *rpcOK)
    {
        CHECK("QueryCurrentSpeedTests")
        if (!is_running){
            *rpcOK = false;
            return {};
        }
        const libcore::EmptyReq request;
        libcore::QuerySpeedTestResponse reply;
        CHANNEL
        auto status = stub->QuerySpeedTest(&context, 
            request, &reply);

        if(status.ok()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    libcore::QueryCountryTestResponse Client::QueryCountryTestResults(bool* rpcOK)
    {
        CHECK("QueryCountryTestResults")
        if (!is_running){
            *rpcOK = false;
            return {};
        }
        const libcore::EmptyReq request;
        libcore::QueryCountryTestResponse reply;
        CHANNEL
        auto status = stub->QueryCountryTest(
            &context, request, &reply);

        if(status.ok()) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

} // namespace API
