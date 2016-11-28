#include <string.h>

#include <Ice/Ice.h>
#include "PortalInterface.h"

using namespace StreamingService;

class CLIClient
{
public:
    explicit CLIClient(std::string const& portalId, Ice::CommunicatorPtr ic);
    ~CLIClient();

    void Run();

private:
    PortalInterfacePrx _portal;
    StreamList _streams;
};
