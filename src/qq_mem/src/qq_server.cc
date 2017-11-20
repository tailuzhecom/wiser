#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>

#include "qq.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using qq::HelloRequest;
using qq::HelloReply;

// for AddDocument
using qq::AddDocumentRequest;
using qq::StatusReply;

using qq::QQEngine;

// Logic and data behind the server's behavior.
class QQEngineServiceImpl final : public QQEngine::Service {
    Status SayHello(ServerContext* context, const HelloRequest* request,
            HelloReply* reply) override {
        std::string prefix("Hello ");
        reply->set_message(prefix + request->name());
        return Status::OK;
    }

    Status AddDocument(ServerContext* context, const AddDocumentRequest* request,
            StatusReply* reply) override {
        std::cout << "title" << request->document().title() << std::endl;
        std::cout << "url" << request->document().url() << std::endl;
        std::cout << "body" << request->document().body() << std::endl;

        std::string msg("I am from AddDocument() server.");
        reply->set_message(msg);
        reply->set_ok(true);
        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    QQEngineServiceImpl service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();

    return 0;
}
