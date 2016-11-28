#!/bin/bash

set -e

mkdir -p build

slice2cpp PortalInterface.ice
g++ -ggdb -I. -c PortalInterface.cpp Portal.cpp # -std=c+11
g++ -ggdb -o build/portal PortalInterface.o Portal.o -lIce -lIceUtil -lpthread
g++ -ggdb -I. -c PortalInterface.cpp Streamer.cpp
g++ -ggdb -o build/streamer PortalInterface.o Streamer.o -lIce -lIceUtil -lpthread
