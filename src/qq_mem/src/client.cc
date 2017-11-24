#include "qq_client.h"
#include "index_creator.h"


int main(int argc, char** argv) {
    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint (in this case,
    // localhost at port 50051). We indicate that the channel isn't authenticated
    // (use of InsecureChannelCredentials()).
    QQEngineClient qqengine(grpc::CreateChannel(
                "localhost:50051", grpc::InsecureChannelCredentials()));


    std::string reply = qqengine.AddDocument(
            "my title",
            "my url",
            "my body");
    std::cout << "Greeter received: " << reply << std::endl;

    reply = qqengine.Search("body");
    std::cout << "Search: " << reply << std::endl;

    reply = qqengine.Search("body");
    std::cout << "Search: " << reply << std::endl;

    std::cout << "Latttttttttttttttttttttttttttttttttttttttttttttt" << std::endl;
    IndexCreator index_creator("src/testdata/tokenized_wiki_abstract_line_doc", qqengine);
    index_creator.DoIndex();
    std::cout << "Lat2222222222222222222222222" << std::endl;

    return 0;
}
