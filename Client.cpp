#include <stdio.h>
#include <string>
#include <sstream>
#include <istream>
#include <map>

// for fork/exec/open
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Client.h"
#include "Util.h"

#include <IceStorm/IceStorm.h>
#include <IceUtil/IceUtil.h>

using namespace StreamingService;

class StreamNotifier : public StreamNotifierInterface
{
public:
    void Notify(const std::string& notification, const Ice::Current&) override
    {
        std::cout << notification << '\n';
    }
};

int main(int argc, char** argv)
{
    std::string const portalId = "Portal:default -p 10000";
    CLIClient app(portalId);
    return app.main(argc, argv, "config.sub");
}

CLIClient::CLIClient(std::string const& portalId)
{
    _portalId = portalId;
}

CLIClient::~CLIClient() { }

int CLIClient::run(int argc, char* argv[])
{
    Ice::ObjectPrx base = communicator()->stringToProxy(_portalId);
    _portal = PortalInterfacePrx::checkedCast(base);
    //IceStorm stuff
    Ice::StringSeq args = Ice::argsToStringSeq(argc, argv);
    args = communicator()->getProperties()->parseCommandLineOptions("Notifier", args);
    Ice::stringSeqToArgs(args, argc, argv);
    IceStorm::TopicManagerPrx manager =
        IceStorm::TopicManagerPrx::checkedCast(
                                               communicator()->propertyToProxy("TopicManager.Proxy"));
    IceStorm::TopicPrx topic;
    topic = manager->retrieve("stream");
    Ice::ObjectAdapterPtr adapter = communicator()->createObjectAdapter("Notifier.Subscriber");
    Ice::Identity subId;
    subId.name = IceUtil::generateUUID();
    Ice::ObjectPrx subscriber = adapter->add(new StreamNotifier, subId);
    adapter->activate();
    IceStorm::QoS qos;
    topic->subscribeAndGetPublisher(qos, subscriber);
    //IceStorm stuff

    if (!_portal)
    {
        printf("CLIClient::run - portal not found\n");
        return 0;
    }

    _streams = _portal->GetStreamList();

    printf("Connected, press help for a command list\n");

    while (true)
    {
        std::istringstream iss;
        std::string command;

        // fetch line stream/command
        {
            std::string line;
            std::getline(std::cin, line);
            iss.str(line); // initialize stream

            std::getline(iss, command, ' ');
            // commands are of the form "stream <cmd>" or "<cmd>"
            // ignore starting "stream" string
            if (command == "stream")
                std::getline(iss, command, ' ');
        }

        if (command == "help")
        {
            printf("All commands can be preceded by 'stream'\n");
            printf("help                - print this message\n");
            printf("list                - list all streams\n");
            printf("search $keywords    - list for streams with matching keywords\n");
            printf("play $stream_name   - play stream with matching name\n");
        }
        else if (command == "list")
        {
            printf("There are %zu streams active\n", _streams.size());
            for (size_t i = 0; i < _streams.size(); ++i)
            {
                StreamEntry const& entry = _streams[i];
                printf("- name: %s video size: %s bit rate: %d\n",
                    entry.streamName.c_str(), entry.videoSize.c_str(), entry.bitRate);
            }
        }
        else if (command == "search")
        {
            std::map<std::string, StreamEntry const*> matches;
            std::string keyword;
            while (std::getline(iss, keyword, ' '))
            {
                for (StreamList::const_iterator itr = _streams.begin(); itr != _streams.end(); ++itr)
                {
                    StreamEntry const& entry = *itr;
                    for (size_t i = 0; i < entry.keyword.size(); ++i)
                    {
                        std::string const& entryKeyword = entry.keyword[i];
                        if (entryKeyword.find(keyword) != std::string::npos)
                            matches[entry.streamName] = &entry;
                    }
                }
            }

            printf("There are %zu streams matches\n", _streams.size());
            for (std::map<std::string, StreamEntry const*>::const_iterator itr = matches.begin(); itr != matches.end(); ++itr)
            {
                StreamEntry const& entry = *itr->second;
                printf("- name: %s video size: %s bit rate: %d\n",
                    entry.streamName.c_str(), entry.videoSize.c_str(), entry.bitRate);
            }
        }
        else if (command == "play")
        {
            std::string streamName;
            std::getline(iss, streamName);

            StreamEntry const* entryToPlay = nullptr;
            for (StreamList::const_iterator itr = _streams.begin(); itr != _streams.end(); ++itr)
            {
                StreamEntry const& entry = *itr;
                if (streamName == entry.streamName)
                {
                    entryToPlay = &entry;
                    break;
                }
            }

            if (entryToPlay)
            {
                // launch ffplay instance
                if (fork() == 0)
                {
                    // but redirect ffplay output to /dev/null
                    int fd = open("/dev/null", O_WRONLY);
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);

                    execlp("ffplay", "ffplay", entryToPlay->endpoint.c_str(), NULL);
                }
            }
            else
            {
                printf("Stream '%s' not found\n", streamName.c_str());
            }
        }
        else
        {
            // ignore empty commands
            if (!command.empty())
                printf("Unrecognised command '%s'\n", command.c_str());
        }
    }
    topic->unsubscribe(subscriber);
    return 0;
}
