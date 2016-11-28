#!/bin/bash

ffmpeg -i $1 -loglevel warning \
-analyzeduration 500k -probesize 500k -framerate 30 -video_size $3 \
-codec:v libx264 -preset ultrafast -pix_fmt yuv420p \
-tune zerolatency -preset ultrafast -b:v $4 -g 30 \
-codec:a flac -b:a 32k \
-f mpegts ${2}?listen=1 2>&1 > ffmpeg.log
