#include <Ice/Ice.h>
#include "PortalInterface.h"

#define override

using namespace StreamingService;

class Portal : public PortalInterface, public Ice::Application
{
public:
    explicit Portal();

    void NewStream(StreamEntry const& entry, const ::Ice::Current&) override;
    void CloseStream(StreamEntry const& entry, const ::Ice::Current&) override;

    StreamList GetStreamList(const ::Ice::Current&) override { return _streamRegistry; }
    int run(int, char*[]) override;

private:
    void UpdateNotifier();

private:
    StreamList _streamRegistry;
    StreamNotifierInterfacePrx _streamNotifier;

};
