#include <string.h>
// #include "PortalInterface.h>
#include "Define.h"

// @todo: remove when PortalInterface included
struct StreamEntry
{
    int x[10];
};

// @todo: remove
class PortalInterface;
typedef PortalInterface* PortalInterfacePrx;
typedef std::vector<StreamEntry> StreamList;

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
