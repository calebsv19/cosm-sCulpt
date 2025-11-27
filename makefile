CC := gcc
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TEST_OBJ_DIR := $(BUILD_DIR)/tests/obj
TEST_BIN_DIR := $(BUILD_DIR)/tests/bin

SRC_DIR := src
TOOLS_DIR := $(SRC_DIR)/Tools
EXT_DIR := external
TEST_DIR := tests

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LDFLAGS := $(shell sdl2-config --libs)

WARN_FLAGS := -Wall -Wextra -Werror -Wpedantic
BASE_CFLAGS := $(WARN_FLAGS) -std=c11 -Isrc -Iexternal $(SDL_CFLAGS)
DEBUG ?= 0

ifeq ($(DEBUG),1)
	CFLAGS := $(BASE_CFLAGS) -O0 -g
else
	CFLAGS := $(BASE_CFLAGS) -O2
endif

LDFLAGS := $(SDL_LDFLAGS) -lSDL2_ttf -lm

APP_SRCS := $(shell find $(SRC_DIR) -name '*.c' ! -path '$(TOOLS_DIR)/*')
EXT_SRCS := $(EXT_DIR)/cjson/cJSON.c
ALL_SRCS := $(APP_SRCS) $(EXT_SRCS)

APP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ALL_SRCS))
APP_TARGET := $(BIN_DIR)/LineDrawing

NON_APP_OBJS := $(filter-out $(OBJ_DIR)/src/main.o,$(APP_OBJS))

TEST_SRCS := $(shell find $(TEST_DIR) -name '*.c')
TEST_OBJS := $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_TARGET := $(TEST_BIN_DIR)/run_tests

.PHONY: all run clean test rebuild debug release format lint

all: $(APP_TARGET)

debug:
	@$(MAKE) DEBUG=1

release:
	@$(MAKE) DEBUG=0

run: $(APP_TARGET)
	$(APP_TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

rebuild: clean all

$(APP_TARGET): $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Iexternal -MMD -MP -c $< -o $@

$(TEST_TARGET): $(NON_APP_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

-include $(APP_OBJS:.o=.d)
-include $(TEST_OBJS:.o=.d)



###############################################################################
# Shape Tool
###############################################################################

# Object files for the shape tool
SHAPE_TOOL_SRCS := $(wildcard $(TOOLS_DIR)/*.c)
SHAPE_TOOL_OBJS := $(patsubst $(TOOLS_DIR)/%.c,$(BUILD_DIR)/tools/%.o,$(SHAPE_TOOL_SRCS))
SHAPE_TOOL_SHARED_OBJS := \
	$(OBJ_DIR)/external/cjson/cJSON.o \
	$(OBJ_DIR)/src/Layout/layout.o \
	$(OBJ_DIR)/src/Layout/layout_json.o
SHAPE_TOOL_BIN := $(BIN_DIR)/shape_tool

# Compile Tools/*.c into build/tools/*.o
$(BUILD_DIR)/tools/%.o: $(TOOLS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -MMD -MP -c $< -o $@

# Link the final binary
$(SHAPE_TOOL_BIN): $(SHAPE_TOOL_OBJS) $(SHAPE_TOOL_SHARED_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

# Public target
.PHONY: shape_tool
shape_tool: $(SHAPE_TOOL_BIN)
	@echo "Built shape_tool successfully."

-include $(SHAPE_TOOL_OBJS:.o=.d)
