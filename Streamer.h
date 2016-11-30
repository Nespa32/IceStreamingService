#include <string>

#include <Ice/Ice.h>
#include "PortalInterface.h"

using namespace StreamingService;

class Streamer
{
public:
    Streamer(std::string const& portalId, Ice::CommunicatorPtr ic,
        StreamEntry const& streamEntry, int listenPort, int ffmpegPort);

    bool Initialize();
    void Close();
    void Run();

    void ForceExit() { _forceExit = true; }

private:
    // configs
    int const _listenPort;
    int const _ffmpegPort;

    PortalInterfacePrx _portal;
    StreamEntry _streamEntry;
    std::list<int> _clientList;
    int _listenSocketFd;
    int _ffmpegSocketFd;
    bool _forceExit;
};

