#include <string>
#include <map>

#include <Ice/Ice.h>
#include "PortalInterface.h"

using namespace StreamingService;

class Portal : public PortalInterface, public Ice::Application
{
public:
    Portal();

    // PortalInterface overrides
    void NewStream(StreamEntry const& entry, Ice::Current const& curr) override;
    void CloseStream(StreamEntry const& entry, Ice::Current const& curr) override;

    StreamList GetStreamList(Ice::Current const& curr) override;

    // Ice::Application overrides
    int run(int argc, char** argv) override;

private:
    void UpdateNotifier();

private:
    std::map<std::string, StreamEntry> _streams;
    StreamNotifierInterfacePrx _notifier;
};
