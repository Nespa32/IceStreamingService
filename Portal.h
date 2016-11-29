#include <Ice/Ice.h>
#include "PortalInterface.h"

#define override

using namespace StreamingService;

class Portal : public PortalInterface
{
 public:
    Portal();

    void NewStream(StreamEntry const& entry, const ::Ice::Current&) override;
    void CloseStream(StreamEntry const& entry, const ::Ice::Current&) override;

    StreamList GetStreamList(const ::Ice::Current&) override;

 private:
    StreamList _streamRegistry;

};
