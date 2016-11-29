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
    int byteRate = bitRate / 8;
    byteRate *= 4; // @todo: byte rate is weird for x264
    int tickTime = 100; // how often streamer should send data, in milliseconds

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
        sStreamer = new Streamer(portalId, ic, streamEntry,
            listenPort, ffmpegPort, byteRate);

        // open listen port, start ffmpeg
        if (sStreamer->Initialize())
        {
            // start stream
            sStreamer->Run(tickTime);
        }
        else
        {
            LOG_ERROR("Streamer initialization failed");
            return 1;
        }

        // close and cleanup
        sStreamer->Close();
    }

    delete sStreamer;

    ic->destroy();

    exit(0);
    return 0;
}

void exitHandler(int /*signal*/)
{
    printf("Exiting...\n");
    if (sStreamer)
        sStreamer->ForceExit();
}

Streamer::Streamer(std::string const& portalId, Ice::CommunicatorPtr ic,
    StreamEntry const& streamEntry, int listenPort, int ffmpegPort, int byteRate) :
    _streamEntry(streamEntry), _listenPort(listenPort), _ffmpegPort(ffmpegPort),
    _byteRate(byteRate), _listenSocketFd(0), _ffmpegSocketFd(0), _forceExit(false)
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

void Streamer::Run(int tickTime)
{
    long bufferSecs = 10;

    size_t bufferLen = 0;
    // initialize ring buffer, has some quirks
    {
        bufferLen = _byteRate * bufferSecs;
        // bufferLen must be % BUFFER_SIZE
        bufferLen += BUFFER_SIZE - (bufferLen % BUFFER_SIZE);
    }

    printf("ByteRate %d, ring buffer size %lu (%lu secs of buffering)\n",
        _byteRate, bufferLen, bufferSecs);

    size_t bufferPtr = 0;
    char* ringBuffer = new char[bufferLen];

    // ring buffer must be fully initialized before first client is accepted
    for (size_t i = 0; i < bufferLen; i += BUFFER_SIZE)
    {
        ssize_t remaining = BUFFER_SIZE;
        while (remaining > 0)
        {
            if (_forceExit)
                return;

            size_t offset = BUFFER_SIZE - remaining;
            char* buffer = (char*)(ringBuffer + i + offset);
            ssize_t n = read(_ffmpegSocketFd, buffer, remaining);
            remaining -= n;
        }
    }

    printf("Streamer ready\n");

    long lastTickTime = getMSTime();

    while (true)
    {
        if (_forceExit)
            break;

        long timeBeforeTick = getMSTime();
        long diff = timeBeforeTick - lastTickTime;

        int clientSocket = accept4(_listenSocketFd, NULL, NULL, SOCK_NONBLOCK);
        if (clientSocket > 0)
        {
            _clientList.push_back(clientSocket);

            for (int i = 0; i < bufferLen; i += BUFFER_SIZE)
            {
                char* buffer = (char*)(ringBuffer + (bufferPtr + i) % bufferLen);
                if (write(clientSocket, buffer, BUFFER_SIZE) < 0)
                {
                    fprintf(stderr, "Write failure on client accept\n");
                    break;
                }
            }

            printf("New client fd = %d\n", clientSocket);
        }

        if (diff)
        {
            long bytesToSend = long((_byteRate / 1000.0f) * diff);
            printf("byteRate %u, bytesToSend %lu, diff %lu\n",
                _byteRate, bytesToSend, diff);
            for (int i = 0; i < bytesToSend; i += BUFFER_SIZE)
            {
                char* buffer = (char*)(ringBuffer + (bufferPtr + i) % bufferLen);
                ssize_t remaining = BUFFER_SIZE;
                while (remaining > 0)
                {
                    if (_forceExit)
                        return;

                    size_t offset = BUFFER_SIZE - remaining;
                    ssize_t n = read(_ffmpegSocketFd, buffer + offset, remaining);
                    printf("Read from %lu to %lu\n", buffer + offset - ringBuffer, buffer + offset - ringBuffer + n);
                    remaining -= n;
                }

                for (std::list<int>::iterator itr = _clientList.begin(); itr != _clientList.end();)
                {
                    int clientSocket = *itr;
                    if (write(clientSocket, buffer, BUFFER_SIZE) < 0)
                    {
                        printf("Error connecting to fd %d\n", clientSocket);
                        itr = _clientList.erase(itr);
                        continue;
                    }

                    ++itr;
                }
            }
        }

        long timeAfterTick = getMSTime();
        long tickDuration = timeAfterTick - timeBeforeTick;

        lastTickTime = timeBeforeTick;

        if (tickDuration >= tickTime)
            continue;

        long sleepTime = tickTime - tickDuration;
        usleep(sleepTime * 1e3); // takes microsecs as arg
    }

    delete ringBuffer;
}
