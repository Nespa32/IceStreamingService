#!/bin/bash

set -e

mkdir -p build

g++ -Wall -I. -c Client.cpp PortalInterface.cpp
g++ -Wall -o build/client Client.o PortalInterface.o -lIce -lIceUtil -lpthread

./build/client
