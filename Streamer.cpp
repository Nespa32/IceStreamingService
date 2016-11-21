#include <sstream>
#include <csignal>
#include <Ice/Ice.h>
#include "PortalInterface.h"

using namespace std;
using namespace StreamingService;

void onExit(int signal);
StreamEntry createEntry(char* args[]);

Ice::CommunicatorPtr ic;
PortalInterfacePrx portal;
StreamEntry entry;

int main(int argc, char* argv[])
{
    int status = 0;
    ic = Ice::initialize(argc, argv);
    Ice::ObjectPrx base = ic->stringToProxy("Portal:default -p 10000");
    portal = PortalInterfacePrx::checkedCast(base);
    if (!portal)
        throw "Invalid proxy";

    entry = createEntry(argv);
    signal(SIGINT, onExit);

    while (1)
    {
    }

    onExit(0);
    return status;
}

StreamEntry createEntry(char* args[])
{
    StreamEntry s;

    s.streamName = args[1];
    s.endpoint = args[2];
    s.videoSize = args[3];
    s.bitRate = atoi(args[4]);

    string str = args[5];
    string buf;
    stringstream ss(str);

    while(ss >> buf)
        s.keyword.push_back(buf);

    return s;
}

void onExit(int signal)
{
    portal->CloseStream(entry);
    if (ic)
        ic->destroy();
}
