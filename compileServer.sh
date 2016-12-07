#!/bin/bash

set -e

mkdir -p build

CPP_FLAGS="-ggdb -std=c++11 -Wall"
CPP_LIBS="-lIce -lIceUtil -lIceStorm -lpthread -L /usr/lib/x86_64-linux-gnu/c++11/"
slice2cpp PortalInterface.ice
g++ $CPP_FLAGS -I. -c PortalInterface.cpp Portal.cpp
g++ $CPP_FLAGS -o build/portal PortalInterface.o Portal.o $CPP_LIBS
g++ $CPP_FLAGS -I. -c PortalInterface.cpp Streamer.cpp
g++ $CPP_FLAGS -o build/streamer PortalInterface.o Streamer.o $CPP_LIBS
