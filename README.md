Ice Streaming Service

After running 'make', a local build/ folder is setup with 3 usable binaries
(portal, streamer and client) and respective configurations.
Before any of them can be run, icebox has to be up and running in order IceStorm
to function.

To start icebox, run:
'icebox --Ice.Config=config.icebox'

Now that icebox is running, Portal can be started
./portal

This runs the portal server, which will register streams and make metadata available
to clients.
Since the portal is up, we can also run a client now.
./client

The client is CLI-based and has a few commands available:
- "help                - print this message"
- "list [opt]          - list all streams"
- " --detail           - shows stream endpoint/keywords"
- "search $keywords    - list for streams with matching keywords"
- "play $stream_name   - play stream with matching name"
- "exit/quit           - quits the cli"

Of course, this isn't of much use since there will be no streams available.
To start a stream:
./streamer $video_file $stream_name [options]

Streamer has various options:
- '--transport $trans' sets endpoint transport protocol, tcp by default
- '--host $host' sets endpoint host, localhost by default
- '--port $port' specifies listen port, 9600 by default
- '--ffmpeg_port $port' sets port for ffmpeg instance, 9601 by default
- '--video_size $size' specifies video size, 480x270 by default
- '--bit_rate $rate' sets video bit rate, 400k by default
- '--keywords $key1,$key2...,$keyn' adds search keywords to stream

Streamer can be additionally configured by changing streamer_ffmpeg.sh, which is a
shell script that wraps up ffmpeg (which Streamer makes use of).
