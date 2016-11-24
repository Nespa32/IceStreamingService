#include <sstream>
#include <csignal>
#include <Ice/Ice.h>
#include "PortalInterface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define LISTEN_BACKLOG 1

using namespace std;
using namespace StreamingService;

// Function declarations
StreamEntry createEntry(char* args[]);
void portalRegister(int argc, char* argv[]);
void onExit(int signal);
// Global vars
Ice::CommunicatorPtr ic;
PortalInterfacePrx portal;
StreamEntry entry;

int main(int argc, char* argv[])
{
    // Connect to Portal
    portalRegister(argc, argv);

    // Signal Function for onExit call
    signal(SIGINT, onExit);

    // Launch ffmpeg (ex: port 13000)
    pid_t pid;
    pid = fork();

    if (pid == 0)
        execlp("./../ffmpeg.sh", "ffmpeg.sh", "../PopeyeAliBaba_512kb.mp4",
              "tcp://127.0.0.1:13000", "420x320", "400k", NULL);
    // Wait for ffmpeg setup before trying to read
    sleep(10);

    // Sockets for streamer-ffmpeg connection

    int ffmpegSockFD, ffmpegPort = 13000;
    struct sockaddr_in ffmpegAddr;
    struct hostent *server;

    char ffmpegBuffer[256]; //188

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

    // Sockets for client-streamer connection

    int sockFD, port = 11000, newSockFD;
    socklen_t clilen;
    struct sockaddr_in socketAddr, streamerAddr, clientAddr;
    char buffer[256];

    sockFD = socket(AF_INET, SOCK_STREAM, 0);

    bzero((char *) &streamerAddr, sizeof(streamerAddr));

    streamerAddr.sin_family = AF_INET;
    streamerAddr.sin_addr.s_addr = INADDR_ANY;
    streamerAddr.sin_port = htons(port);

    bind(sockFD, (struct sockaddr *)&streamerAddr,
         sizeof(streamerAddr));

    listen(sockFD, 1);

    clilen = sizeof(clientAddr);
    newSockFD = accept(sockFD,
                       (struct sockaddr *)&clientAddr,
                       &clilen);
    // Sending from ffmpeg to client
    while (1)
    {
        bzero (ffmpegBuffer, 256);
        ssize_t n = read (ffmpegSockFD, ffmpegBuffer, 256);
        write (newSockFD, ffmpegBuffer, n);
    }

    close(sockFD);
    close(ffmpegSockFD);

    onExit(0);
    return 0;
}

StreamEntry createEntry(char* args[])
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

void portalRegister(int argc, char* argv[])
{
    ic = Ice::initialize(argc, argv);
    Ice::ObjectPrx base = ic->stringToProxy("Portal:default -p 10000");
    portal = PortalInterfacePrx::checkedCast(base);
    if (!portal)
        throw "Invalid proxy";

    entry = createEntry(argv);
    portal->NewStream(entry);
}

void onExit(int signal)
{
    portal->CloseStream(entry);
    if (ic)
        ic->destroy();

    exit(signal);
}
