#include <unistd.h>
#include <string>

#include <Ice/Ice.h>
#include "PortalInterface.h"

using namespace StreamingService;

class Streamer : public Ice::Application
{
public:
    Streamer();

    // Ice::Application overrides
    int run(int argc, char** argv) override;

    bool Initialize();
    void Close();
    void Run();

private:
    static void PrintUsage();

private:
    // configs
    std::string _videoFilePath;
    // endpoint info
    std::string _transport;
    std::string _host;
    int _listenPort = 0;
    int _ffmpegPort = 0;
    // support for HLS/DASH
    std::string _hlsHost;
    std::string _dashHost;

    PortalInterfacePrx _portal;
    StreamEntry _streamEntry;
    std::list<int> _clientList;
    int _listenSocketFd = 0;
    int _ffmpegSocketFd = 0;
    pid_t _ffmpegPid = 0;
};

