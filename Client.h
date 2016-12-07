#include <string.h>

#include <Ice/Ice.h>
#include "PortalInterface.h"

using namespace StreamingService;

class CLIClient : public Ice::Application
{
public:
    explicit CLIClient(std::string const& portalId);
    ~CLIClient();

    int run(int argc, char* argv[]) override;

private:
    std::string _portalId;
    PortalInterfacePrx _portal;
    StreamList _streams;
};
