#include <string.h>

#include "Define.h"
#include "PortalInterface.h"

using namespace StreamingService;

class CLIClient
{
public:
    explicit CLIClient(std::string const& portalId, Ice::CommunicatorPtr ic);
    ~CLIClient();

    void run();

private:
    PortalInterfacePrx _portal;
    StreamList _streams;
};
