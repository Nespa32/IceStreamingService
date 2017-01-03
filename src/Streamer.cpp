#include <sstream>
#include <csignal>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "Streamer.h"
#include "Util.h"

#define LISTEN_BACKLOG 10
#define BUFFER_SIZE 4136

using namespace StreamingService;

void exitHandler(int signal);

// need a global to handle Ctrl-C interrupts
bool early_exit = false;

int main(int argc, char** argv)
{
    // catch Ctrl-C, need to remove stream from portal
    signal(SIGINT, exitHandler);

    Streamer app;
    return app.main(argc, argv, "config.streamer");
}

void exitHandler(int /*signal*/)
{
    LOG_INFO("Exiting...");
    early_exit = true;
}

Streamer::Streamer() : Ice::Application(Ice::NoSignalHandling) { }

int Streamer::run(int argc, char** argv)
{
    if (argc < 3)
    {
        PrintUsage();
        return 1;
    }

    IceInternal::Application::_signalPolicy = Ice::NoSignalHandling;

    _videoFilePath = argv[1];
    std::string streamName = argv[2];
    _transport = "tcp";
    _host = "localhost";
    _listenPort = 9600;
    _ffmpegPort = 9601;
    std::string videoSize = "480x270";
    std::string bitRate = "400k";
    std::string keywords; // actually a list with csv values

    // parse command line options
    for (int i = 3; i < argc; ++i)
    {
        std::string option = argv[i];

        // all options have a following arg
        if (i + 1 >= argc)
        {
            LOG_INFO("Missing argument after option %s", option.c_str());
            return 1;
        }

        std::string arg = argv[i + 1];

        ++i; // consume arg

        if (option == "--transport")
            _transport = arg;
        else if (option == "--host")
            _host = arg;
        else if (option == "--port")
            _listenPort = atoi(arg.c_str());
        else if (option == "--ffmpeg_port")
            _ffmpegPort = atoi(arg.c_str());
        else if (option == "--video_size")
            videoSize = arg;
        else if (option == "--bit_rate")
            bitRate = arg;
        else if (option == "--keywords")
            keywords = arg;
        else if (option == "--hls")
            _hlsHost = arg;
        else if (option == "--dash")
            _dashHost = arg;
        else
            LOG_INFO("Unrecognized option '%s', skipping", option.c_str());
    }

    // switch to HTTP mode
    if (!_hlsHost.empty() || !_dashHost.empty())
        _transport = "http";

    // setup stream entry
    // endpoint format: transport://host:port
    std::string endpoint = _transport +
        "://" + _host +
        ":" + std::to_string(_listenPort);

    if (!_hlsHost.empty())
    {
        // format new endpoint
        // http://nginx_host:port/hls/stream.m3u8
        endpoint += "/hls/" + streamName + ".m3u8";
    }
    else if (!_dashHost.empty())
    {
        // format new endpoint
        // http://nginx_host:port/dash/stream.m3u8
        endpoint += "/dash/" + streamName + "/index.mpd";
    }

    _streamEntry.streamName = streamName;
    _streamEntry.endpoint = endpoint;
    _streamEntry.videoSize = videoSize;
    _streamEntry.bitRate = bitRate;
    // fill stream keywords
    {
        std::string t;
        std::stringstream ss(keywords);
        while (std::getline(ss, t, ','))
            _streamEntry.keyword.push_back(t);
    }

    int exitCode = 0;
    if (_transport != "tcp")
        _isTcp = false;
    // actual stream logic
    {
        // open listen port, start ffmpeg
        if (Initialize())
        {
            // start stream
            Run();
        }
        else
        {
            LOG_ERROR("Streamer initialization failed");
            exitCode = 1;
        }
    }

    // close and cleanup
    Close();
    return exitCode;
}

bool Streamer::Initialize()
{
    Ice::ObjectPrx base = communicator()->propertyToProxy("Portal.Proxy");
    _portal = PortalInterfacePrx::checkedCast(base);

    if (!_portal)
    {
        LOG_ERROR("failed to find portal");
        return false;
    }

    // open listen port
    // only in regular case, since for HLS/DASH nginx takes care of streaming
    if (_hlsHost.empty() && _dashHost.empty())
    {
        LOG_INFO("Setting up listen socket...");

        if (_isTcp)
            _listenSocketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        else
            _listenSocketFd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

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

        if (_isTcp && listen(_listenSocketFd, LISTEN_BACKLOG) < 0)
        {
            LOG_ERROR("Failed to open listen socket");
            return false;
        }

        int setVal = 1;
        setsockopt(_listenSocketFd, SOL_SOCKET, SO_REUSEADDR, &setVal, sizeof(int));
        if (!_isTcp)
            setsockopt(_listenSocketFd, SOL_SOCKET, IP_RECVERR,
               (const void *)&setVal , sizeof(int));

    }

    // handle ffmpeg start
    if (!_hlsHost.empty() || !_dashHost.empty())
    {
        // HLS/DASH case
        std::string const& endpoint = (!_hlsHost.empty()) ? _hlsHost : _dashHost;

        if (!_hlsHost.empty())
            LOG_INFO("Starting HLS stream on %s", endpoint.c_str());
        else if (!_dashHost.empty())
            LOG_INFO("Streaming DASH stream on %s", endpoint.c_str());

        _ffmpegPid = fork();
        if (_ffmpegPid == 0)
        {
            // for the sake of flexibility, a shell script is used
            // it's better than coding all ffmpeg arguments
            // arguments used
            // $1 = video file path
            // $2 = HLS/DASH end point info in "transport://ip:port/path" format
            //    (e.g rtmp://127.0.0.1:8080/hls_app/stream)
            execlp("./streamer_ffmpeg_hls_dash.sh", "streamer_ffmpeg_hls_dash.sh",
                _videoFilePath.c_str(),             // $1
                endpoint.c_str(),                   // $2
                nullptr);
        }
    }
    else
    {
        // regular case, wait for open port
        // ffmpeg necessarily starts on localhost, only port can change
        std::string ffmpegHost = "127.0.0.1";

        // need to setup a seperate endpoint for ffmpeg, since the port will differ

        std::string endpoint = std::string("tcp") +
            "://" + ffmpegHost +
            ":" + std::to_string(_ffmpegPort);

        LOG_INFO("Starting and connecting to ffmpeg...");

        _ffmpegPid = fork();
        if (_ffmpegPid == 0)
        {
            // for the sake of flexibility, a shell script is used
            // it's better than coding all ffmpeg arguments
            // arguments used:
            // $1 = video file path
            // $2 = end point info in "transport://ip:port" format (e.g tcp://127.0.0.1:999$
            // $3 = video size (e.g 420x320)
            // $4 = video bitrate (e.g 400k or 400000)
            execlp("./streamer_ffmpeg.sh", "streamer_ffmpeg.sh",
                _videoFilePath.c_str(),             // $1
                endpoint.c_str(),                   // $2
                _streamEntry.videoSize.c_str(),     // $3
                _streamEntry.bitRate.c_str(),       // $4
                nullptr);
        }
        _ffmpegSocketFd = socket(AF_INET, SOCK_STREAM, 0);

        hostent* server = gethostbyname(ffmpegHost.c_str());

        sockaddr_in addr;
        bzero((char*)&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        bcopy((char*)server->h_addr, (char*)&addr.sin_addr.s_addr, server->h_length);
        addr.sin_port = htons(_ffmpegPort);

        while (true)
        {
            if (early_exit)
            {
                LOG_INFO("Exiting early...");
                return false;
            }

            // @todo: socket won't connect if ffmpeg had an early exit, handle that
            int error = connect(_ffmpegSocketFd, (sockaddr*)&addr, sizeof(addr));
            if (error >= 0)
                break; // no error, finally have a valid socket

            usleep(500 * 1e3); // 500ms sleep
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

    if (_ffmpegPid > 0)
    {
        LOG_INFO("Sending SIGTERM to ffmpeg...");
        kill(_ffmpegPid, SIGTERM);

        LOG_INFO("Waiting on ffmpeg to exit...");
        wait(NULL);
    }
}

void Streamer::Run()
{
    LOG_INFO("Streamer ready");

    // HLS/DASH case, nginx is acting as a stream server and we've got nothing to do
    // just sleep until ffmpeg exits
    if (!_hlsHost.empty() || !_dashHost.empty())
    {
        while (!early_exit)
            usleep(100 * 1e3);

        return;
    }

    long const sleepTime = 20; // 20ms sleep time per cycle
    long const tickTimer = 30; // 30ms for sending data per cycle

    while (true)
    {
        // periodically accept new clients
        if (_isTcp) // tcp
        {
            int clientSocket = accept4(_listenSocketFd, NULL, NULL, SOCK_NONBLOCK);
            if (clientSocket > 0)
            {
                _clientList.push_back(clientSocket);
                LOG_INFO("Accepted new client, fd %d", clientSocket);
            }
        }

        else // udp
        {
            struct sockaddr_in clientaddr;
            socklen_t clientlen = sizeof(clientaddr);
            char buffer[BUFFER_SIZE];
            int n = recvfrom(_listenSocketFd, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr *) &clientaddr, &clientlen);
            clientaddr.sin_port = htons(atoi(buffer));
            //clientaddr.sin_family = AF_INET;
            //clientaddr.sin_addr.s_addr = INADDR_ANY;
            if (n != -1 && Streamer::IsNewClient(clientaddr))
            {
                LOG_INFO("Pushing new Client port %d", htons(clientaddr.sin_port));
                _clientUdpList.push_back(clientaddr);
            }
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
                if (early_exit)
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
            if (_isTcp)
            {
                _clientList.remove_if([buffer](int clientSocket)
                                      {
                                          if (write(clientSocket, buffer, BUFFER_SIZE) < 0)
                                          {
                                              LOG_INFO("Removing client fd %d from client list", clientSocket);
                                              return true;
                                          }

                                          return false;
                                      });
            }
            else
            {
                _clientUdpList.remove_if([buffer, this](struct sockaddr_in clientaddr) {
                        int clientlen = sizeof(clientaddr);
                        if (sendto(_listenSocketFd, buffer, BUFFER_SIZE, 0,
                                   (struct sockaddr *) &clientaddr, clientlen) < 0)
                            {
                                //LOG_INFO("Removing client fd %d from client list", clientSocket);
                                LOG_INFO("Failed sent to port %d, removing", ntohs(clientaddr.sin_port));
                                return true;
                            }
                        return false;
                    });
            }

            // break out of send cycle and accept new clients if a tick has passed
            long now = getMSTime();
            if (now - timeBeforeTick > tickTimer)
                break;
        }
    }
}

void Streamer::PrintUsage()
{
    LOG_INFO("Usage: ./streamer $video_file $stream_name [options]");
    LOG_INFO("Options:");
    LOG_INFO("'--transport $trans' sets endpoint transport protocol, tcp by default");
    LOG_INFO("'--host $host' sets endpoint host, localhost by default");
    LOG_INFO("'--port $port' specifies listen port, 9600 by default");
    LOG_INFO("'--ffmpeg_port $port' sets port for ffmpeg instance, 9601 by default");
    LOG_INFO("'--video_size $size' specifies video size, 480x270 by default");
    LOG_INFO("'--bit_rate $rate' sets video bit rate, 400k by default");
    LOG_INFO("'--keywords $key1,$key2...,$keyn' adds search keywords to stream");
    LOG_INFO("'--hls $nginx_host'");
    LOG_INFO("'--dash $nginx_host'");
}

bool Streamer::IsNewClient(sockaddr_in clientaddr)
{
    int clientPort = ntohs(clientaddr.sin_port);
    char *clientIp = inet_ntoa(clientaddr.sin_addr);
    for (sockaddr_in& addr : _clientUdpList)
    {
        int port = ntohs(addr.sin_port);
        char *ip = inet_ntoa(addr.sin_addr);
        if (port == clientPort && strcmp(clientIp, ip) == 0)
            return false;
    }
    return true;
}
