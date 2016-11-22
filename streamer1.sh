#!/bin/bash

set -e

./build/streamer PopeyeAliBaba tcp://127.0.0.1:13000 480x270 400k a,b,c,d \
    ../PopeyeAliBaba_512kb.mp4 tcp://127.0.0.1:12000 480x270 400k
