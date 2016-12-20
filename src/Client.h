#include <string>
#include <map>

#include <Ice/Ice.h>
#include "PortalInterface.h"

using namespace StreamingService;

class CLIClient : public Ice::Application
{
public:
    CLIClient();
    ~CLIClient();

    // Ice::Application overrides
    int run(int argc, char** argv) override;

    void StreamAdded(StreamEntry const& entry);
    void StreamRemoved(StreamEntry const& entry);

private:
    void RunCommands();

private:
    std::map<std::string, StreamEntry> _streams;
};

class StreamNotifier : public StreamNotifierInterface
{
public:
    StreamNotifier(CLIClient& client) : _client(client) { }

    void NotifyStreamAdded(StreamEntry const& entry, Ice::Current const& curr) override
    {
        _client.StreamAdded(entry);
    }

    void NotifyStreamRemoved(StreamEntry const& entry, Ice::Current const& curr) override
    {
        _client.StreamRemoved(entry);
    }

private:
    CLIClient& _client;
};


