#!/bin/bash

# Streamer needs to start an ffmpeg instance
# for the sake of flexibility, it executes this shell script, and
# this shell script starts ffmpeg with user arguments
# it's somewhat ugly, but it beats having to parse ffmpeg options in code
# and it's better than passing everything as an environment variable

# $1 = video file path
# $2 = end point info in "transport://ip:port" format (e.g tcp://127.0.0.1:9999)
# $3 = video size (e.g 420x320)
# $4 = video bitrate (e.g 400k or 400000)
ffmpeg -re -i $1 -loglevel warning \
    -analyzeduration 500k -probesize 500k -framerate 30 -video_size $3 \
    -codec:v libx264 -preset ultrafast -pix_fmt yuv420p \
    -tune zerolatency -preset ultrafast -b:v $4 -g 30 \
    -codec:a flac -b:a 32k \
    -f mpegts ${2}?listen=1
