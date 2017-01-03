#!/bin/bash

# Streamer needs to start an ffmpeg instance
# for the sake of flexibility, it executes this shell script, and
# this shell script starts ffmpeg with user arguments
# it's somewhat ugly, but it beats having to parse ffmpeg options in code
# and it's better than passing everything as an environment variable

# $1 = video file path
# $2 = HLS/DASH end point info in "transport://ip:port/path" format
#    (e.g rtmp://127.0.0.1:8080/hls_app/stream)
ffmpeg -re -i $1 -codec:v libx264 -vprofile baseline -g 30 \
    -codec:a aac -strict -2 \
    -f flv $2
