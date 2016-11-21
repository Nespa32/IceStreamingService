#!/bin/bash

set -e

slice2cpp PortalInterface.ice
g++ -I. -c PortalInterface.cpp Portal.cpp # -std=c+11
g++ -o portal PortalInterface.o Portal.o -lIce -lIceUtil -lpthread
g++ -I. -c PortalInterface.cpp Streamer.cpp
g++ -o streamer PortalInterface.o Streamer.o -lIce -lIceUtil -lpthread
