# Makefile for LineDrawing project

# Compiler settings
CC ?= gcc
CFLAGS = -Wall -Wextra -O2 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_ttf -lm

# Source directory
SRCDIR := src

# Recursively find all .c source files in SRCDIR
SRCS := $(shell find $(SRCDIR) -type f -name '*.c')

# Generate object files by replacing .c with .o
OBJS := $(SRCS:.c=.o)

# Target executable
TARGET := LineDrawing

.PHONY: all run clean

all: $(TARGET)

run: all
	@./$(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Generic rule for compiling .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

