# Compiler and compiler flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99

# Directories for raylib and its submodules
RAYLIB_DIR = raylib/src
RAYLIB_SUBMODULES_DIR = raylib/src/external

# Libraries for linking
LIBS = -lraylib -lm -ldl -lpthread

# Source files and output executable name
SRCS_DIR = src
BUILD_DIR = build
BIN = pong.bin

compile: $(BUILD_DIR) $(BIN)

compile-deps:
	$(MAKE) -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=SHARED

clean:
	rm -rf $(BUILD_DIR)

%.bin: $(SRCS_DIR)/%.c
	$(CC) $(CFLAGS) -DDEBUG $< -I$(RAYLIB_DIR) -I$(RAYLIB_SUBMODULES_DIR) \
		-L$(RAYLIB_DIR) $(LIBS) -Wl,-rpath=$(RAYLIB_DIR) -o $(BUILD_DIR)/$(basename $@)

# Create the build directory
$(BUILD_DIR):
	mkdir $(BUILD_DIR)
