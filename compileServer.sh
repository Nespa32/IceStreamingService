#!/bin/bash

set -e

mkdir -p build

slice2cpp PortalInterface.ice
g++ -I. -c PortalInterface.cpp Portal.cpp # -std=c+11
g++ -o build/portal PortalInterface.o Portal.o -lIce -lIceUtil -lpthread
g++ -I. -c PortalInterface.cpp Streamer.cpp
g++ -o build/streamer PortalInterface.o Streamer.o -lIce -lIceUtil -lpthread
