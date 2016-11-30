#include <sstream>
#include <csignal>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "Streamer.h"
#include "Util.h"

#define LISTEN_BACKLOG 10
#define BUFFER_SIZE 256

using namespace StreamingService;

static Streamer* sStreamer = nullptr;

void exitHandler(int signal);

int main(int argc, char* argv[])
{
    Ice::CommunicatorPtr ic = Ice::initialize(argc, argv);

    std::string const portalId = "Portal:default -p 10000";

    char* bitRateStr = argv[4];
    int bitRate = atoi(bitRateStr);
    // parse bitRate
    {
        char c = bitRateStr[strlen(bitRateStr) - 1];
        if (c == 'k' || c == 'K')
            bitRate *= 1e3;
        else if (c == 'm' || c == 'M')
            bitRate *= 1e6;
        else if (c == 'g' || c == 'G')
            bitRate *= 1e9;
    }

    int listenPort = 11000;
    int ffmpegPort = 13000;

    StreamEntry streamEntry;
    streamEntry.streamName = argv[1];
    streamEntry.endpoint = argv[2];
    streamEntry.videoSize = argv[3];
    streamEntry.bitRate = bitRate;
    // fill stream keywords
    {
        std::string s = argv[5];
        std::string t;
        std::stringstream ss(s);
        while (ss >> t)
            streamEntry.keyword.push_back(t);
    }

    // catch Ctrl-C, need to remove stream from portal
    signal(SIGINT, exitHandler);

    int exitCode = 0;

    // actual stream logic
    {
        sStreamer = new Streamer(portalId, ic, streamEntry, listenPort, ffmpegPort);

        // open listen port, start ffmpeg
        if (sStreamer->Initialize())
        {
            // start stream
            sStreamer->Run();
        }
        else
        {
            LOG_ERROR("Streamer initialization failed");
            exitCode = 1;
        }

        // close and cleanup
        sStreamer->Close();
    }

    delete sStreamer;

    ic->destroy();

    return exitCode;
}

void exitHandler(int /*signal*/)
{
    printf("Exiting...\n");
    if (sStreamer)
        sStreamer->ForceExit();
}

Streamer::Streamer(std::string const& portalId, Ice::CommunicatorPtr ic,
    StreamEntry const& streamEntry, int listenPort, int ffmpegPort) :
    _listenPort(listenPort), _ffmpegPort(ffmpegPort),
     _streamEntry(streamEntry), _listenSocketFd(0), _ffmpegSocketFd(0),
    _forceExit(false)
{
   Ice::ObjectPrx base = ic->stringToProxy(portalId);

    _portal = PortalInterfacePrx::checkedCast(base);
}

bool Streamer::Initialize()
{
    if (!_portal)
    {
        LOG_ERROR("No portal");
        return false;
    }

    // start ffmpeg, wait for open port
    {
        if (fork() == 0)
        {
            // @todo: pass arguments, get rid of shell script
            execlp("./ffmpeg.sh", "ffmpeg.sh", "../PopeyeAliBaba_512kb.mp4",
                "tcp://127.0.0.1:13000", "420x320", "400k", nullptr);
        }

        _ffmpegSocketFd = socket(AF_INET, SOCK_STREAM, 0);
        hostent* server = gethostbyname("localhost");

        sockaddr_in addr;
        bzero((char*)&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        bcopy((char*)server->h_addr, (char*)&addr.sin_addr.s_addr, server->h_length);
        addr.sin_port = htons(_ffmpegPort);

        while (true)
        {
            if (_forceExit)
            {
                LOG_ERROR("Exiting early...");
                return false;
            }

            // @todo: socket won't connect if ffmpeg had an early exit, handle that
            int error = connect(_ffmpegSocketFd, (sockaddr*)&addr, sizeof(addr));
            if (error >= 0)
                break; // no error, finally have a valid socket

            usleep(500 * 1e3); // 500ms sleep
        }
    }

    // open listen port
    {
        _listenSocketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (_listenSocketFd < 0)
        {
            LOG_ERROR("Failed to initialize listen socket");
            return false;
        }

        sockaddr_in addr;
        bzero((char*)&addr, sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(_listenPort);

        if (bind(_listenSocketFd, (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            LOG_ERROR("Failed to bind listen socket");
            return false;
        }

        if (listen(_listenSocketFd, LISTEN_BACKLOG) < 0)
        {
            LOG_ERROR("Failed to open listen socket");
            return false;
        }
    }

    _portal->NewStream(_streamEntry);
    return true;
}

void Streamer::Close()
{
    while (!_clientList.empty())
    {
        int clientSocket = _clientList.front();
        _clientList.pop_front();
        close(clientSocket);
    }

    if (_listenSocketFd > 0)
    {
        shutdown(_listenSocketFd, SHUT_RDWR);
        close(_listenSocketFd);
    }

    if (_ffmpegSocketFd > 0)
    {
        shutdown(_ffmpegSocketFd, SHUT_RDWR);
        close(_ffmpegSocketFd);
    }

    if (_portal)
        _portal->CloseStream(_streamEntry);
}

void Streamer::Run()
{
    printf("Streamer ready\n");

    long const sleepTime = 20; // 20ms sleep time per cycle
    long const tickTimer = 30; // 30ms for sending data per cycle

    while (true)
    {
        // periodically accept new clients
        int clientSocket = accept4(_listenSocketFd, NULL, NULL, SOCK_NONBLOCK);
        if (clientSocket > 0)
        {
            _clientList.push_back(clientSocket);
            printf("New client fd = %d\n", clientSocket);
        }

        usleep(sleepTime * 1e3); // wait a bit so there's some data to send

        long timeBeforeTick = getMSTime();

        // read from ffmpeg and send data
        // ffmpeg will produce data to be read at the right video play speed
        while (true)
        {
            char buffer[BUFFER_SIZE];
            ssize_t remaining = BUFFER_SIZE;
            while (remaining > 0)
            {
                if (_forceExit)
                    return;

                size_t offset = BUFFER_SIZE - remaining;
                ssize_t n = read(_ffmpegSocketFd, buffer + offset, remaining);
                if (n < 0)
                {
                    LOG_ERROR("ffmpeg socket read failed");
                    return;
                }

                remaining -= n;
            }

            // send data to all clients, remove clients with invalid/closed sockets
            _clientList.remove_if([buffer](int clientSocket)
            {
                if (write(clientSocket, buffer, BUFFER_SIZE) < 0)
                {
                    LOG_ERROR("Write error for client socket %d", clientSocket);
                    return true;
                }

                return false;
            });

            // break out of send cycle and accept new clients if a tick has passed
            long now = getMSTime();
            if (now - timeBeforeTick > tickTimer)
                break;
        }
    }
}
