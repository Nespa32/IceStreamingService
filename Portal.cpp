#include "Portal.h"

#include <IceUtil/IceUtil.h>
#include <IceStorm/IceStorm.h>

using namespace StreamingService;

Portal::Portal()
{
}

void Portal::NewStream(StreamEntry const& entry, const ::Ice::Current&)
{
    _streamRegistry.push_back(entry);
    if (!_streamNotifier)
        printf("No Notifier\n");
    else
        _streamNotifier->Notify("New Stream\n");
}

void Portal::CloseStream(StreamEntry const& entry, const ::Ice::Current&)
{
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

StreamList Portal::GetStreamList(const ::Ice::Current&)
{
    return _streamRegistry;
}

int main(int argc, char* argv[])
{
    Portal app;
    return app.main(argc, argv, "config.pub");
}

int
Portal::run(int argc, char* argv[])
{
    Ice::ObjectAdapterPtr adapter =
        communicator()->createObjectAdapterWithEndpoints("Portal", "default -p 10000");
    Ice::ObjectPtr object = new Portal;
    adapter->add(object, communicator()->stringToIdentity("Portal"));
    adapter->activate();

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

    communicator()->waitForShutdown();

    return 0;
}
