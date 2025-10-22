# Makefile for LineDrawing project

CC := gcc
CFLAGS := -Wall -Wextra -O2 -Isrc -Iexternal $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) -lSDL2_ttf -lm

# Source directories
SRC_DIR := src
EXT_DIR := external

# Find all project source files
SRCS := $(shell find $(SRC_DIR) -name '*.c') \
        $(EXT_DIR)/cjson/cJSON.c

# Build object files in build/
OBJ_DIR := build
OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRCS))

# Final executable
TARGET := LineDrawing

.PHONY: all run clean

all: $(TARGET)

# Link final binary
$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Build rule for source files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

