# Compiler and compiler flags
CC 		= gcc
CFLAGS 	= -Wall -Wextra -Werror -std=c99 -g

# Directories for raylib and its submodules
RAYLIB_DIR 				= raylib/src
RAYLIB_SUBMODULES_DIR 	= raylib/src/external

# Libraries for linking
LIBS = -lraylib -lm -ldl -lpthread

# Source files and output executable name
ROOT_DIR	:= $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
SRCS_DIR 	= src
BUILD_DIR 	= build
BIN 		= pong.bin

.PHONY: clean

compile: $(BUILD_DIR) $(BIN)

compile-deps:
	$(MAKE) -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=SHARED

clean:
	rm -rf $(BUILD_DIR)

%.bin: $(SRCS_DIR)/%.c
	$(CC) $(CFLAGS) -DDEBUG -DASSET_PATH=\"$(ROOT_DIR)assets\" $< \
		-I$(RAYLIB_DIR) -I$(RAYLIB_SUBMODULES_DIR) -L$(RAYLIB_DIR) $(LIBS) \
		-Wl,-rpath=$(ROOT_DIR)$(RAYLIB_DIR) -o $(BUILD_DIR)/$(basename $@)

# Create the build directory
$(BUILD_DIR):
	mkdir $(BUILD_DIR)
