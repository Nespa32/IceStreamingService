
SRC_DIR=src
BUILD_DIR=build
CONFIG_DIR=config

CPP_FLAGS=-ggdb -std=c++11 -Wall
CPP_INCLUDES=-I $(BUILD_DIR)/src
CPP_LIBS=-lIce -lIceUtil -lIceStorm -lpthread -L /usr/lib/x86_64-linux-gnu/c++11/

default:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/src

	slice2cpp --output-dir $(BUILD_DIR)/src $(SRC_DIR)/PortalInterface.ice

	g++ $(CPP_FLAGS) $(CPP_INCLUDES) -o $(BUILD_DIR)/PortalInterface.o -c $(BUILD_DIR)/src/PortalInterface.cpp
	g++ $(CPP_FLAGS) $(CPP_INCLUDES) -o $(BUILD_DIR)/Portal.o -c $(SRC_DIR)/Portal.cpp
	g++ $(CPP_FLAGS) $(CPP_INCLUDES) -o $(BUILD_DIR)/Streamer.o -c $(SRC_DIR)/Streamer.cpp
	g++ $(CPP_FLAGS) $(CPP_INCLUDES) -o $(BUILD_DIR)/Client.o -c $(SRC_DIR)/Client.cpp
	g++ $(CPP_FLAGS) -o $(BUILD_DIR)/portal $(BUILD_DIR)/PortalInterface.o $(BUILD_DIR)/Portal.o $(CPP_LIBS)
	g++ $(CPP_FLAGS) -o $(BUILD_DIR)/streamer $(BUILD_DIR)/PortalInterface.o $(BUILD_DIR)/Streamer.o $(CPP_LIBS)
	g++ $(CPP_FLAGS) -o $(BUILD_DIR)/client $(BUILD_DIR)/PortalInterface.o $(BUILD_DIR)/Client.o $(CPP_LIBS)

	# copy ffmpeg shell script
	cp -n $(SRC_DIR)/streamer_ffmpeg.sh $(BUILD_DIR)
	cp -n $(SRC_DIR)/streamer_ffmpeg_hls_dash.sh $(BUILD_DIR)

	# setup initial config files
	cp -n $(CONFIG_DIR)/* $(BUILD_DIR)

clean:
	$(RM) $(BUILD_DIR)/src/*
	$(RM) $(BUILD_DIR)/*.o
	$(RM) $(BUILD_DIR)/portal
	$(RM) $(BUILD_DIR)/streamer
	$(RM) $(BUILD_DIR)/client

run_icebox:
	# kill previous icebox instance
	pkill icebox || true

	# reset icebox db
	rm -rf $(BUILD_DIR)/db
	mkdir -p $(BUILD_DIR)/db

	cd $(BUILD_DIR) && icebox --Ice.Config=config.icebox


