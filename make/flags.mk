# SDL detection aligned with physics_sim style: explicit macOS paths first,
# sdl2-config/pkg-config elsewhere.
SDL_CFLAGS :=
SDL_LDFLAGS :=
SDL_LIBS := -lSDL2
SDL_TTF_LIB := -lSDL2_ttf
VULKAN_CFLAGS :=
VULKAN_LIBS :=

ifeq ($(UNAME_S),Darwin)
	SDL_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
	SDL_LDFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs-only-L sdl2 2>/dev/null)
	SDL_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs-only-l sdl2 2>/dev/null)
	ifeq ($(strip $(SDL_CFLAGS)),)
		ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/SDL2/SDL.h),)
			SDL_CFLAGS += -I$(TARGET_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
		else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/SDL2/SDL.h),)
			SDL_CFLAGS += -I$(TARGET_ALT_HOMEBREW_PREFIX)/include -D_THREAD_SAFE
		endif
	endif
	ifeq ($(strip $(SDL_LDFLAGS)),)
		ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libSDL2.dylib),)
			SDL_LDFLAGS += -L$(TARGET_HOMEBREW_PREFIX)/lib
		else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libSDL2.dylib),)
			SDL_LDFLAGS += -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib
		endif
	endif
	ifeq ($(strip $(SDL_LIBS)),)
		SDL_LIBS := -lSDL2
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

	VULKAN_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
	VULKAN_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs vulkan 2>/dev/null)
	ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
		VULKAN_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
		VULKAN_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lvulkan
	endif
else
	# Non-macOS: prefer sdl2-config, then pkg-config, then a system fallback.
	SDL_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
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

	VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
	VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
	ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
		VULKAN_CFLAGS := -I/usr/include
		VULKAN_LIBS := -lvulkan
	endif
endif

ifeq ($(strip $(SDL_LDFLAGS)),)
	SDL_LDFLAGS :=
endif

WARN_FLAGS := -Wall -Wextra -Werror -Wpedantic
BASE_CFLAGS := $(WARN_FLAGS) -std=c11 $(ARCH_FLAGS) -Iinclude -Isrc -Isrc/Tools -Iexternal -I$(VK_RENDERER_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_WORKSPACE_AUTHORING_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_MATH_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_LAYOUT_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_PANE_MODULE_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(TIMER_HUD_DIR)/include $(SDL_CFLAGS)

ifeq ($(DEBUG),1)
	CFLAGS := $(BASE_CFLAGS) -O0 -g
else
	CFLAGS := $(BASE_CFLAGS) -O2
endif

APP_CFLAGS := $(CFLAGS) $(VULKAN_CFLAGS) -DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" -include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h

ifeq ($(UNAME_S),Darwin)
	APP_CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
	VULKAN_LIBS += -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
endif

LDFLAGS := $(ARCH_FLAGS) $(SDL_LDFLAGS) $(SDL_LIBS) $(SDL_TTF_LIB) $(SDL_FRAMEWORKS) $(VULKAN_LIBS) -lm
