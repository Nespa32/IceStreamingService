#include "Portal.h"
#include "Util.h"

#include <IceUtil/IceUtil.h>
#include <IceStorm/IceStorm.h>

using namespace StreamingService;

int main(int argc, char* argv[])
{
    Portal app;
    return app.main(argc, argv, "config.pub");
}

Portal::Portal() { }

void Portal::NewStream(StreamEntry const& entry, Ice::Current const& /*curr*/)
{
    UpdateNotifier();

    std::string const& name = entry.streamName;
    auto itr = _streams.find(name);
    if (itr == _streams.end())
        _streams[name] = entry;
    else
    {
        LOG_ERROR("stream with name %s already exists", name.c_str());
        return;
    }

    _notifier->NotifyStreamAdded(entry);
}

void Portal::CloseStream(StreamEntry const& entry, Ice::Current const& /*curr*/)
{
    UpdateNotifier();

    std::string const& name = entry.streamName;
    auto itr = _streams.find(name);
    if (itr != _streams.end())
        _streams.erase(itr);
    else
    {
        LOG_ERROR("stream %s not found", name.c_str());
        return;
    }

    _notifier->NotifyStreamRemoved(entry);
}

StreamList Portal::GetStreamList(Ice::Current const& /*curr*/)
{
    StreamList streamList;
    for (auto const& itr : _streams)
    {
        StreamEntry const& entry = itr.second;
        streamList.push_back(entry);
    }

    return streamList;
}

int Portal::run(int argc, char* argv[])
{
    Ice::ObjectAdapterPtr adapter =
        communicator()->createObjectAdapterWithEndpoints("Portal", "default -p 10000");
    Ice::ObjectPtr object = new Portal;
    adapter->add(object, communicator()->stringToIdentity("Portal"));
    adapter->activate();

    UpdateNotifier();

    LOG_INFO("Portal up and running");

    communicator()->waitForShutdown();
    return 0;
}

void Portal::UpdateNotifier()
{
    if (_notifier)
        return;

    IceStorm::TopicManagerPrx manager =
        IceStorm::TopicManagerPrx::checkedCast(communicator()->propertyToProxy("TopicManager.Proxy"));
    IceStorm::TopicPrx topic;
    try
    {
        topic = manager->retrieve("stream");
    }
    catch (IceStorm::NoSuchTopic const&)
    {
        topic = manager->create("stream");
    }

    Ice::ObjectPrx publisher = topic->getPublisher();
    _notifier = StreamNotifierInterfacePrx::uncheckedCast(publisher);
}
