#include "qq_client.h"


int main(int argc, char** argv) {
    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint (in this case,
    // localhost at port 50051). We indicate that the channel isn't authenticated
    // (use of InsecureChannelCredentials()).
    QQEngineClient qqengine(grpc::CreateChannel(
                "localhost:50051", grpc::InsecureChannelCredentials()));

    std::string reply = qqengine.AddDocument();
    std::cout << "Greeter received: " << reply << std::endl;

    reply = qqengine.Search();
    std::cout << "Search: " << reply << std::endl;

    return 0;
}
