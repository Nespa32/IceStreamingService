#include "Portal.h"

#include <IceUtil/IceUtil.h>
#include <IceStorm/IceStorm.h>

using namespace StreamingService;

int main(int argc, char* argv[])
{
    Portal app;
    return app.main(argc, argv, "config.pub");
}

Portal::Portal() { }

void Portal::NewStream(StreamEntry const& entry, const ::Ice::Current&)
{
    UpdateNotifier();

    _streamRegistry.push_back(entry);
    _streamNotifier->Notify("New Stream\n");
}

void Portal::CloseStream(StreamEntry const& entry, const ::Ice::Current&)
{
    UpdateNotifier();

    std::vector<StreamEntry>::iterator itr;
    for (itr = _streamRegistry.begin(); itr != _streamRegistry.end(); ++itr)
    {
        StreamEntry const& streamEntry = *itr;
        if (streamEntry.streamName == entry.streamName)
        {
            _streamNotifier->Notify("Deleted Stream\n");
            _streamRegistry.erase(itr);
            break;
        }
    }
}

int Portal::run(int argc, char* argv[])
{
    Ice::ObjectAdapterPtr adapter =
        communicator()->createObjectAdapterWithEndpoints("Portal", "default -p 10000");
    Ice::ObjectPtr object = new Portal;
    adapter->add(object, communicator()->stringToIdentity("Portal"));
    adapter->activate();

    communicator()->waitForShutdown();
    return 0;
}

void Portal::UpdateNotifier()
{
    if (_streamNotifier)
        return;

    IceStorm::TopicManagerPrx manager =
        IceStorm::TopicManagerPrx::checkedCast(communicator()->propertyToProxy("TopicManager.Proxy"));
    IceStorm::TopicPrx topic;
    try
    {
        topic = manager->retrieve("stream");
    }
    catch(const IceStorm::NoSuchTopic&)
    {
        topic = manager->create("stream");
    }

    Ice::ObjectPrx publisher = topic->getPublisher();
    _streamNotifier = StreamNotifierInterfacePrx::uncheckedCast(publisher);
}
