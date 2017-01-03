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

// udp include
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "Client.h"
#include "Util.h"

#include <IceStorm/IceStorm.h>
#include <IceUtil/IceUtil.h>

#define BUFFER_SIZE 4136
//256

using namespace StreamingService;

int main(int argc, char** argv)
{
    CLIClient app;
    return app.main(argc, argv, "config.client");
}

CLIClient::CLIClient() { }

CLIClient::~CLIClient() { }

int CLIClient::run(int argc, char* argv[])
{
    // connect to Portal, fetch stream list
    {
        Ice::ObjectPrx base = communicator()->propertyToProxy("Portal.Proxy");
        PortalInterfacePrx portal = PortalInterfacePrx::checkedCast(base);

        // can't run client without an active Portal
        if (!portal)
        {
            LOG_ERROR("portal not found");
            return 1;
        }

        auto streamList = portal->GetStreamList();
        for (StreamEntry const& entry : streamList)
            _streams[entry.streamName] = entry;
    }

    IceStorm::TopicPrx topic;
    Ice::ObjectPrx subscriber;
    // setup stream subscriber
    {
        IceStorm::TopicManagerPrx manager =
            IceStorm::TopicManagerPrx::checkedCast(communicator()->propertyToProxy("TopicManager.Proxy"));

        try
        {
            topic = manager->retrieve("stream");
        }
        catch (IceStorm::NoSuchTopic const&)
        {
            LOG_ERROR("failed to find IceStorm topic");
            return 1;
        }

        Ice::ObjectAdapterPtr adapter = communicator()->createObjectAdapter("Notifier.Subscriber");
        Ice::Identity subId;
        subId.name = IceUtil::generateUUID();
        subscriber = adapter->add(new StreamNotifier(*this), subId);

        adapter->activate();

        try
        {
            topic->subscribeAndGetPublisher(IceStorm::QoS(), subscriber);
        }
        catch (IceStorm::AlreadySubscribed const&)
        {
            LOG_ERROR("notifier already subscribed");
            return 1;
        }
    }

    // run command loop
    RunCommands();

    topic->unsubscribe(subscriber);
    return 0;
}

void CLIClient::StreamAdded(StreamEntry const& entry)
{
    std::string const& name = entry.streamName;
    auto itr = _streams.find(name);
    if (itr == _streams.end())
    {
        LOG_INFO("[INFO] Stream added: '%s'", name.c_str());
        _streams[name] = entry;
    }
    else
        LOG_ERROR("stream %s already exists", name.c_str());
}

void CLIClient::StreamRemoved(StreamEntry const& entry)
{
    std::string const& name = entry.streamName;
    auto itr = _streams.find(name);
    if (itr != _streams.end())
    {
        LOG_INFO("[INFO] Stream removed: '%s'", name.c_str());
        _streams.erase(itr);
    }
    else
        LOG_ERROR("stream %s not found", name.c_str());
}

void CLIClient::RunCommands()
{
    LOG_INFO("Connected, press help for a command list");

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
            LOG_INFO("All commands can be preceded by 'stream'");
            LOG_INFO("help                - print this message");
            LOG_INFO("list [opt]          - list all streams");
            LOG_INFO(" --detail           - shows stream endpoint/keywords");
            LOG_INFO("search $keywords    - list for streams with matching keywords");
            LOG_INFO("play $stream_name   - play stream with matching name");
            LOG_INFO("exit/quit           - quits the cli");
        }
        else if (command == "list")
        {
            std::string opt;
            std::getline(iss, opt, ' ');
            bool inDetail = opt == "--detail";

            LOG_INFO("There are %zu streams active", _streams.size());
            for (auto const& itr : _streams)
            {
                StreamEntry const& entry = itr.second;
                LOG_INFO("- name: %s video size: %s bit rate: %s",
                    entry.streamName.c_str(), entry.videoSize.c_str(),
                    entry.bitRate.c_str());

                if (inDetail)
                {
                    LOG_INFO("EndPoint: %s", entry.endpoint.c_str());
                    for (std::string const& entryKeyword : entry.keyword)
                        LOG_INFO("Keyword: %s", entryKeyword.c_str());
                }
            }
        }
        else if (command == "search")
        {
            std::map<std::string, StreamEntry const*> matches;
            std::string keyword;
            while (std::getline(iss, keyword, ' '))
            {
                for (auto const& itr : _streams)
                {
                    StreamEntry const& entry = itr.second;
                    for (std::string const& entryKeyword : entry.keyword)
                    {
                        if (entryKeyword.find(keyword) != std::string::npos)
                            matches[entry.streamName] = &entry;
                    }
                }
            }

            LOG_INFO("There are %zu streams matches", matches.size());
            for (auto const& itr : matches)
            {
                StreamEntry const& entry = *itr.second;
                LOG_INFO("- name: %s video size: %s bit rate: %s",
                    entry.streamName.c_str(), entry.videoSize.c_str(),
                    entry.bitRate.c_str());
            }
        }
        else if (command == "play")
        {
            // Some stuff for udp
            int udpSocket;
            sockaddr_in udpAddr;
            int isTcp = 1;
            socklen_t len;
            //
            std::string streamName;
            std::getline(iss, streamName);

            auto itr = _streams.find(streamName);
            if (itr != _streams.end())
            {
                StreamEntry const& entryToPlay = itr->second;
                { // Check if the transport is udp
                char* transport = strdup(entryToPlay.endpoint.c_str());
                strtok (transport,":/");
                char* port = strtok (NULL, ":/");
                char* ip = strtok (NULL, ":/");
                if (strcmp(transport, "udp") == 0) // UDP
                {
                    isTcp = 0;
                    // new port/socket for ffplay to connect to
                    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
                    bzero((char*)&udpAddr, sizeof(udpAddr));
                    udpAddr.sin_family = AF_INET;
                    udpAddr.sin_addr.s_addr = INADDR_ANY;
                    udpAddr.sin_port = 0;
                    if (bind((udpSocket), (sockaddr*)&udpAddr, sizeof(udpAddr)) < 0)
                    {
                        LOG_INFO("Failed to bind to ffplay udp socket");
                    }
                    else
                    {
                        len = sizeof(udpAddr);
                        getsockname(udpSocket, (sockaddr*)&udpAddr, (socklen_t*)&len);
                    }

                    // client/server first message
                    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
                    struct sockaddr_in streamerAddr;
                    streamerAddr.sin_family = AF_INET;
                    streamerAddr.sin_port = htons(atoi(port));
                    streamerAddr.sin_addr.s_addr = inet_addr(ip);
                    streamerAddr.sin_port = htons(9600);
                    streamerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                    memset(streamerAddr.sin_zero, '\0', sizeof streamerAddr.sin_zero);

                    char str[20] ;
                    sprintf(str, "%d", ntohs(udpAddr.sin_port));
                    int err = -1;
                    while (err == -1)
                    {
                        err = sendto(clientSocket, str, sizeof(str), 0, (struct sockaddr*)&streamerAddr, sizeof(streamerAddr));
                    }
                }
                }
                // launch ffplay instance
                if (fork() == 0)
                {
                    if (isTcp)
                    {
                        // but redirect ffplay output to /dev/null
                        int fd = open("/dev/null", O_WRONLY);
                        dup2(fd, STDOUT_FILENO);
                        dup2(fd, STDERR_FILENO);
                        close(fd);

                        execlp("ffplay", "ffplay", entryToPlay.endpoint.c_str(), NULL);
                    }
                    else // udp
                    {
                        char buf[BUFFER_SIZE];
                        int ffplaySockFD = socket(AF_INET, SOCK_STREAM, 0);
                        sockaddr_in ffplayAddr;
                        struct hostent *server;

                        server = gethostbyname("localhost");

                        bzero((char *) &ffplayAddr, sizeof(ffplayAddr));
                        ffplayAddr.sin_family = AF_INET;
                        bcopy((char *)server->h_addr,
                              (char *)&ffplayAddr.sin_addr.s_addr,
                              server->h_length);
                        ffplayAddr.sin_port = 0;

                        if (bind(ffplaySockFD, (sockaddr*)&ffplayAddr, sizeof(ffplayAddr)) < 0)
                        {
                            LOG_ERROR("Failed to bind\n");
                        }
                        else
                        {
                            socklen_t len = sizeof(ffplayAddr);
                            getsockname(ffplaySockFD, (sockaddr*)&ffplayAddr, (socklen_t*)&len);
                            printf("FFplay to port %d\n", ntohs(ffplayAddr.sin_port));
                        }
                        if(listen(ffplaySockFD, 2) < 0)
                        {
                            LOG_ERROR("Failed to listen\n");
                        }
                        socklen_t len = sizeof(ffplayAddr);
                        int newFD;
                        while((newFD = accept(ffplaySockFD, (struct sockaddr *) &ffplayAddr,
                                              (socklen_t*)&len)) < 0 )
                        {
                            LOG_ERROR("Failed to connect to ffplay, fd = %d\n", newFD);
                        }
                        LOG_INFO("Connected to ffplay, fd = %d\n", newFD);

                        bzero(buf,BUFFER_SIZE);
                        while (1)
                        {
                            ssize_t n = recvfrom(udpSocket, buf, BUFFER_SIZE, 0, (struct sockaddr *)&udpAddr, (socklen_t*)&len);
                            if (n <= 0)
                                break;
                            write (newFD, buf, n);
                            bzero(buf,BUFFER_SIZE);
                        }
                    }
                }
            }
            else
            {
                LOG_INFO("Stream '%s' not found", streamName.c_str());
            }
        }
        else if (command == "quit" || command == "exit")
        {
            LOG_INFO("Exiting...");
            return;
        }
        else
        {
            // ignore empty commands
            if (!command.empty())
                LOG_INFO("Unrecognised command '%s'", command.c_str());
        }
    }
}
