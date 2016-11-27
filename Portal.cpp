#include "Portal.h"

using namespace StreamingService;

Portal::Portal()
{
}

void Portal::NewStream(StreamEntry const& entry, const ::Ice::Current&)
{
    _streamRegistry.push_back(entry);
}

void Portal::CloseStream(StreamEntry const& entry, const ::Ice::Current&)
{
    std::vector<StreamEntry>::iterator itr;
    for (itr = _streamRegistry.begin(); itr != _streamRegistry.end(); ++itr)
    {
        StreamEntry const& streamEntry = *itr;
        if (streamEntry.streamName == entry.streamName)
        {
            _streamRegistry.erase(itr);
            break;
        }
    }
}

StreamList Portal::GetStreamList(const ::Ice::Current&)
{
    return _streamRegistry;
}

int main(int argc, char* argv[])
{
    Ice::CommunicatorPtr ic = Ice::initialize(argc, argv);
    Ice::ObjectAdapterPtr adapter =
        ic->createObjectAdapterWithEndpoints("Portal", "default -p 10000");
    Ice::ObjectPtr object = new Portal;
    adapter->add(object, ic->stringToIdentity("Portal"));
    adapter->activate();
    ic->waitForShutdown();
    ic->destroy();

    return 0;
}
