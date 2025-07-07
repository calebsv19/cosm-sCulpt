# Makefile for LineDrawing project

CC := gcc
CFLAGS := -Wall -Wextra -O2 -Isrc $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) -lSDL2_ttf -lm

# Recursively find all .c files under src/
SRC_DIR := src
SRCS := $(shell find $(SRC_DIR) -name '*.c')

# Object files live in build/ with same structure
OBJ_DIR := build
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Final executable
TARGET := LineDrawing

.PHONY: all run clean

all: $(TARGET)

# Rule to link final binary
$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Rule to build .o files in build/ from src/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

