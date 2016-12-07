#!/bin/bash

set -e

mkdir -p build

CPP_FLAGS="-ggdb -std=c++11 -Wall"
CPP_LIBS="-lIce -lIceUtil -lIceStorm -lpthread -L /usr/lib/x86_64-linux-gnu/c++11/"

slice2cpp PortalInterface.ice

g++ $CPP_FLAGS -I. -c Client.cpp PortalInterface.cpp
g++ $CPP_FLAGS -o build/client Client.o PortalInterface.o $CPP_LIBS

#./build/client
