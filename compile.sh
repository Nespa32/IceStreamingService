#!/bin/bash

set -e

mkdir -p build
cd build

CPP_FLAGS="-ggdb -std=c++11 -Wall"
CPP_LIBS="-lIce -lIceUtil -lIceStorm -lpthread -L /usr/lib/x86_64-linux-gnu/c++11/"
SRC_DIR="../src"

slice2cpp ${SRC_DIR}/PortalInterface.ice

g++ $CPP_FLAGS -I. -c PortalInterface.cpp
g++ $CPP_FLAGS -I. -c ${SRC_DIR}/Portal.cpp
g++ $CPP_FLAGS -I. -c ${SRC_DIR}/Streamer.cpp
g++ $CPP_FLAGS -I. -c ${SRC_DIR}/Client.cpp
g++ $CPP_FLAGS -o portal PortalInterface.o Portal.o $CPP_LIBS
g++ $CPP_FLAGS -o streamer PortalInterface.o Streamer.o $CPP_LIBS
g++ $CPP_FLAGS -o client Client.o PortalInterface.o $CPP_LIBS

rm -f PortalInterface.cpp
rm -f PortalInterface.h
