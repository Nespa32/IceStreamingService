#include <sstream>
#include <csignal>
#include <Ice/Ice.h>
#include "PortalInterface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define LISTEN_BACKLOG 10
#define BUFFER_SIZE 256

using namespace std;
using namespace StreamingService;


StreamEntry CreateEntry(char* args[]);
void PortalRegister(int argc, char* argv[]);
void OnExit(int signal);
int Connect2ffmpeg(int ffmpegPort);
int BeginListening(int port);
int ParseBitrate(char* bitrate);
long getMStime();


// Global vars
Ice::CommunicatorPtr ic;
PortalInterfacePrx portal;
StreamEntry entry;

int main(int argc, char* argv[])
{
    // Connect to Portal
    PortalRegister(argc, argv);

    // Signal Function for OnExit call
    signal(SIGINT, OnExit);

    // Launch ffmpeg (ex: port 13000)
    pid_t pid;
    pid = fork();

    if (pid == 0)
        execlp("./../ffmpeg.sh", "ffmpeg.sh", "../PopeyeAliBaba_512kb.mp4",
              "tcp://127.0.0.1:13000", "420x320", "400k", NULL);
    // Has to be more than bitrate/8
    int byterate = ParseBitrate(argv[4]) * 2;
    // Wait for ffmpeg setup before trying to read
    sleep(2);
    // Sockets for streamer-ffmpeg connection
    int ffmpegPort = 13000;
    int ffmpegSockFD = Connect2ffmpeg(ffmpegPort);
    // Sockets for client-streamer connection
    int clientPort = 11000;
    struct sockaddr_in clientAddr;
    socklen_t clilen;
    int sockFD = BeginListening(clientPort);
    clilen = sizeof(clientAddr);

    char ffmpegBuffer[BUFFER_SIZE];
    std::list<int> socketList;

    //    int newSocketFD = accept (sockFD, (sockaddr*)&clientAddr, &clilen);

    long const period = 1000; // ms
    long lastMs = getMStime();
    while (1)
    {
        long now = getMStime();
        long diff = now - lastMs;
        long bytesToSend = (diff > 0) ? long(byterate / (1000.0f / diff)) : 0;

        int newSocketFD = accept4(sockFD, (sockaddr*)&clientAddr, &clilen, SOCK_NONBLOCK);
        if (newSocketFD > 0)
        {
            socketList.push_back(newSocketFD);
            printf("New client fd = %d\n", newSocketFD);
        }

        int byteCounter = 0;
        while (byteCounter < bytesToSend)
        {
            byteCounter += BUFFER_SIZE;

            bzero(ffmpegBuffer, BUFFER_SIZE);
            ssize_t n = read(ffmpegSockFD, ffmpegBuffer, BUFFER_SIZE);
            if (n < BUFFER_SIZE)
            {
                printf("read failed, byteCounter %d\n", byteCounter);
                break;
            }

            for (std::list<int>::iterator itr = socketList.begin(); itr != socketList.end();)
            {
                if (write (*itr, ffmpegBuffer, n) < 0)
                {
                    printf("Error connecting to fd %d\n", *itr);
                    itr = socketList.erase(itr);
                }
                else
                    itr++;
            }
        }

        long timeAfterTick = getMStime();
        long updateDiff = timeAfterTick - now;

        long sleepTime = ((updateDiff < period) ? (period - updateDiff) : 0);

        lastMs = now;
        usleep(sleepTime * 1e3); // takes microsecs as arg
    }

    close(sockFD);
    close(ffmpegSockFD);

    OnExit(0);
    return 0;
}

StreamEntry CreateEntry(char* args[])
{
    StreamEntry s;

    s.streamName = args[1];
    s.endpoint = args[2];
    s.videoSize = args[3];
    s.bitRate = atoi(args[4]);

    string str = args[5];
    string buf;
    stringstream ss(str);

    while(ss >> buf)
        s.keyword.push_back(buf);

    return s;
}

void PortalRegister(int argc, char* argv[])
{
    ic = Ice::initialize(argc, argv);
    Ice::ObjectPrx base = ic->stringToProxy("Portal:default -p 10000");
    portal = PortalInterfacePrx::checkedCast(base);
    if (!portal)
        throw "Invalid proxy";

    entry = CreateEntry(argv);
    portal->NewStream(entry);
}

void OnExit(int signal)
{
    portal->CloseStream(entry);
    if (ic)
        ic->destroy();

    exit(signal);

}

int Connect2ffmpeg(int ffmpegPort)
{
    int ffmpegSockFD;
    struct sockaddr_in ffmpegAddr;
    struct hostent *server;

    ffmpegSockFD = socket(AF_INET, SOCK_STREAM, 0);

    server = gethostbyname("localhost");

    bzero((char *) &ffmpegAddr, sizeof(ffmpegAddr));
    ffmpegAddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&ffmpegAddr.sin_addr.s_addr,
          server->h_length);
    ffmpegAddr.sin_port = htons(ffmpegPort);

    connect(ffmpegSockFD, (struct sockaddr *) &ffmpegAddr,
            sizeof(ffmpegAddr));

    return ffmpegSockFD;
}

int BeginListening(int port)
{
    int sockFD;
    struct sockaddr_in socketAddr, streamerAddr, clientAddr;

    sockFD = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    //sockFD = socket(AF_INET, SOCK_STREAM, 0);

    bzero((char *) &streamerAddr, sizeof(streamerAddr));

    streamerAddr.sin_family = AF_INET;
    streamerAddr.sin_addr.s_addr = INADDR_ANY;
    streamerAddr.sin_port = htons(port);

    bind(sockFD, (struct sockaddr *)&streamerAddr,
         sizeof(streamerAddr));

    listen(sockFD, LISTEN_BACKLOG);
    return sockFD;
}

int ParseBitrate(char* bitrate)
{
    int l = strlen(bitrate);
    int mult = 1;
    switch (bitrate[l - 1])
    {
    case 'k':
    case 'K':
        mult = 1e3;
        break;
    case 'm':
    case 'M':
        mult = 1e6;
        break;
    default:
        break;
    }

    int i = atoi(bitrate) * mult;
    return i;
}

long getMStime()
{
    timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1e3 + t.tv_usec / 1e3;
}
