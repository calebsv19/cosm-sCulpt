CC := gcc
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TEST_OBJ_DIR := $(BUILD_DIR)/tests/obj
TEST_BIN_DIR := $(BUILD_DIR)/tests/bin
UNAME_S := $(shell uname -s)

SRC_DIR := src
TOOLS_DIR := $(SRC_DIR)/Tools
EXT_DIR := external
TEST_DIR := tests

# SDL detection aligned with physics_sim style: explicit macOS paths first,
# sdl2-config/pkg-config elsewhere.
SDL_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
SDL_CFLAGS :=
SDL_LDFLAGS :=
SDL_LIBS := -lSDL2
SDL_TTF_LIB := -lSDL2_ttf

ifeq ($(UNAME_S),Darwin)
	# Prefer explicit Homebrew prefixes so both Intel (/usr/local) and Apple Silicon (/opt/homebrew)
	# builds work even when sdl2-config isn't available.
	ifneq ($(wildcard /opt/homebrew/include/SDL2/SDL.h),)
		SDL_CFLAGS += -I/opt/homebrew/include -D_THREAD_SAFE
		SDL_LDFLAGS += -L/opt/homebrew/lib
	endif
	ifneq ($(wildcard /usr/local/include/SDL2/SDL.h),)
		SDL_CFLAGS += -I/usr/local/include -D_THREAD_SAFE
		SDL_LDFLAGS += -L/usr/local/lib
	endif

	# Framework fallback if Homebrew headers aren't present.
	ifeq ($(strip $(SDL_CFLAGS)),)
		ifneq ($(wildcard /Library/Frameworks/SDL2.framework/Headers/SDL.h),)
			SDL_CFLAGS += -F/Library/Frameworks -I/Library/Frameworks/SDL2.framework/Headers -D_THREAD_SAFE
			SDL_LDFLAGS += -F/Library/Frameworks
			SDL_LIBS :=
			SDL_TTF_LIB :=
			SDL_FRAMEWORKS := -framework SDL2 -framework SDL2_ttf
		endif
	endif

	# Last-resort: sdl2-config if nothing else was found.
	ifeq ($(strip $(SDL_CFLAGS)),)
		ifneq ($(SDL_CONFIG),)
			SDL_CFLAGS += $(shell $(SDL_CONFIG) --cflags)
			SDL_LDFLAGS += $(shell $(SDL_CONFIG) --libs)
		endif
	endif
else
	# Non-macOS: prefer sdl2-config, then pkg-config, then a system fallback.
	ifneq ($(SDL_CONFIG),)
		SDL_CFLAGS += $(shell $(SDL_CONFIG) --cflags)
		SDL_LDFLAGS += $(shell $(SDL_CONFIG) --libs)
	else
		SDL_PKGCONFIG := $(shell pkg-config --exists sdl2 >/dev/null 2>&1 && echo yes)
		ifeq ($(SDL_PKGCONFIG),yes)
			SDL_CFLAGS += $(shell pkg-config --cflags sdl2)
			SDL_LDFLAGS += $(shell pkg-config --libs sdl2)
		else
			SDL_CFLAGS += -I/usr/include/SDL2
			SDL_LDFLAGS += -L/usr/lib
		endif
	endif
endif

ifeq ($(strip $(SDL_LDFLAGS)),)
	SDL_LDFLAGS := -lSDL2
endif

WARN_FLAGS := -Wall -Wextra -Werror -Wpedantic
BASE_CFLAGS := $(WARN_FLAGS) -std=c11 -Isrc -Isrc/Tools -Iexternal $(SDL_CFLAGS)
DEBUG ?= 0

ifeq ($(DEBUG),1)
	CFLAGS := $(BASE_CFLAGS) -O0 -g
else
	CFLAGS := $(BASE_CFLAGS) -O2
endif

LDFLAGS := $(SDL_LDFLAGS) $(SDL_LIBS) $(SDL_TTF_LIB) $(SDL_FRAMEWORKS) -lm

APP_SRCS := $(shell find $(SRC_DIR) -name '*.c' ! -path '$(TOOLS_DIR)/*')
SHAPE_LIB_SRCS := $(shell find $(TOOLS_DIR)/ShapeLib -name '*.c')
SHAPE_BRIDGE_SRCS := $(TOOLS_DIR)/shape_from_layout.c $(TOOLS_DIR)/shape_export.c
EXT_SRCS := $(EXT_DIR)/cjson/cJSON.c
ALL_SRCS := $(APP_SRCS) $(SHAPE_LIB_SRCS) $(SHAPE_BRIDGE_SRCS) $(EXT_SRCS)

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
SHAPE_TOOL_SRCS := $(shell find $(TOOLS_DIR) -name '*.c')
SHAPE_TOOL_OBJS := $(patsubst $(TOOLS_DIR)/%.c,$(BUILD_DIR)/tools/%.o,$(SHAPE_TOOL_SRCS))
SHAPE_TOOL_SHARED_OBJS := \
	$(OBJ_DIR)/external/cjson/cJSON.o \
	$(OBJ_DIR)/src/Layout/layout.o \
	$(OBJ_DIR)/src/Layout/layout_json.o
SHAPE_TOOL_BIN := $(BIN_DIR)/shape_tool

# Compile Tools/*.c into build/tools/*.o
$(BUILD_DIR)/tools/%.o: $(TOOLS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Isrc/Tools -MMD -MP -c $< -o $@

# Link the final binary
$(SHAPE_TOOL_BIN): $(SHAPE_TOOL_OBJS) $(SHAPE_TOOL_SHARED_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

# Public target
.PHONY: shape_tool
shape_tool: $(SHAPE_TOOL_BIN)
	@echo "Built shape_tool successfully."

-include $(SHAPE_TOOL_OBJS:.o=.d)
