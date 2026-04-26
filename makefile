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
SHARED_ROOT ?= third_party/codework_shared
SHARED_ASSETS_DIR := $(SHARED_ROOT)/assets
SHAPE_DIR := $(SHARED_ROOT)/shape
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
VK_RENDERER_DIR := $(SHARED_ROOT)/vk_renderer
CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_MATH_DIR := $(SHARED_ROOT)/core/core_math
CORE_TIME_DIR := $(SHARED_ROOT)/core/core_time
CORE_SCENE_DIR := $(SHARED_ROOT)/core/core_scene
CORE_OBJECT_DIR := $(SHARED_ROOT)/core/core_object
CORE_UNITS_DIR := $(SHARED_ROOT)/core/core_units
CORE_LAYOUT_DIR := $(SHARED_ROOT)/core/core_layout
CORE_PANE_DIR := $(SHARED_ROOT)/core/core_pane
CORE_PANE_MODULE_DIR := $(SHARED_ROOT)/core/core_pane_module
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
CORE_TRACE_DIR := $(SHARED_ROOT)/core/core_trace
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
TIMER_HUD_DIR := $(SHARED_ROOT)/timer_hud

# SDL detection aligned with physics_sim style: explicit macOS paths first,
# sdl2-config/pkg-config elsewhere.
SDL_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
SDL_CFLAGS :=
SDL_LDFLAGS :=
SDL_LIBS := -lSDL2
SDL_TTF_LIB := -lSDL2_ttf
VULKAN_CFLAGS :=
VULKAN_LIBS :=

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

	VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
	VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
	ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
		VULKAN_CFLAGS := -I/opt/homebrew/include
		VULKAN_LIBS := -L/opt/homebrew/lib -lvulkan
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

	VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
	VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
	ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
		VULKAN_CFLAGS := -I/usr/include
		VULKAN_LIBS := -lvulkan
	endif
endif

ifeq ($(strip $(SDL_LDFLAGS)),)
	SDL_LDFLAGS := -lSDL2
endif

WARN_FLAGS := -Wall -Wextra -Werror -Wpedantic
BASE_CFLAGS := $(WARN_FLAGS) -std=c11 -Iinclude -Isrc -Isrc/Tools -Iexternal -I$(VK_RENDERER_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_MATH_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_LAYOUT_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_PANE_MODULE_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(TIMER_HUD_DIR)/include $(SDL_CFLAGS)
DEBUG ?= 0

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

LDFLAGS := $(SDL_LDFLAGS) $(SDL_LIBS) $(SDL_TTF_LIB) $(SDL_FRAMEWORKS) $(VULKAN_LIBS) -lm

KIT_RENDER_SRCS := \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c \
	$(KIT_RENDER_DIR)/src/kit_render_external_text.c

APP_SRCS := $(shell find $(SRC_DIR) -name '*.c' ! -path '$(TOOLS_DIR)/*')
VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -name '*.c')
SHAPE_LIB_SRCS := $(shell find $(TOOLS_DIR)/ShapeLib -name '*.c')
SHAPE_BRIDGE_SRCS := $(TOOLS_DIR)/shape_from_layout.c $(TOOLS_DIR)/shape_export.c $(TOOLS_DIR)/shape_dataset.c $(TOOLS_DIR)/canonical_scene_export.c $(TOOLS_DIR)/canonical_scene_export_primitives.c
EXT_SRCS := $(EXT_DIR)/cjson/cJSON.c
CORE_TIME_SRCS := $(CORE_TIME_DIR)/src/core_time.c
ifeq ($(UNAME_S),Darwin)
	CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_mac.c
else
	CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_posix.c
endif
CORE_SRCS := $(CORE_BASE_DIR)/src/core_base.c $(CORE_IO_DIR)/src/core_io.c $(CORE_DATA_DIR)/src/core_data.c $(CORE_PACK_DIR)/src/core_pack.c $(CORE_MATH_DIR)/src/core_math.c $(CORE_TIME_SRCS) $(CORE_SCENE_DIR)/src/core_scene.c $(CORE_OBJECT_DIR)/src/core_object.c $(CORE_UNITS_DIR)/src/core_units.c $(CORE_LAYOUT_DIR)/src/core_layout.c $(CORE_PANE_DIR)/src/core_pane.c $(CORE_PANE_MODULE_DIR)/src/core_pane_module.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c
TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -name '*.c')
ALL_SRCS := $(APP_SRCS) $(VK_RENDERER_SRCS) $(KIT_RENDER_SRCS) $(SHAPE_LIB_SRCS) $(SHAPE_BRIDGE_SRCS) $(EXT_SRCS) $(CORE_SRCS) $(TIMER_HUD_SRCS)

APP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ALL_SRCS))
APP_TARGET := $(BIN_DIR)/LineDrawing
DIST_DIR := dist
PACKAGE_APP_NAME := sCulpt.app
PACKAGE_APP_DIR := $(DIST_DIR)/$(PACKAGE_APP_NAME)
PACKAGE_CONTENTS_DIR := $(PACKAGE_APP_DIR)/Contents
PACKAGE_MACOS_DIR := $(PACKAGE_CONTENTS_DIR)/MacOS
PACKAGE_RESOURCES_DIR := $(PACKAGE_CONTENTS_DIR)/Resources
PACKAGE_FRAMEWORKS_DIR := $(PACKAGE_CONTENTS_DIR)/Frameworks
PACKAGE_INFO_PLIST_SRC := tools/packaging/macos/Info.plist
PACKAGE_LAUNCHER_SRC := tools/packaging/macos/line-drawing-launcher
PACKAGE_DYLIB_BUNDLER := tools/packaging/macos/bundle-dylibs.sh
DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(PACKAGE_APP_NAME)
PACKAGE_ADHOC_SIGN_IDENTITY ?= -
RELEASE_VERSION_FILE ?= VERSION
RELEASE_VERSION ?= $(strip $(shell cat "$(RELEASE_VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
RELEASE_CHANNEL ?= stable
RELEASE_PRODUCT_NAME := sCulpt
RELEASE_PROGRAM_KEY := line_drawing
RELEASE_BUNDLE_ID := com.cosm.sculpt
RELEASE_ARTIFACT_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-macOS-$(RELEASE_CHANNEL)
RELEASE_DIR := build/release
RELEASE_APP_ZIP := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).zip
RELEASE_MANIFEST := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).manifest.txt
RELEASE_CODESIGN_IDENTITY ?= $(if $(strip $(APPLE_SIGN_IDENTITY)),$(APPLE_SIGN_IDENTITY),$(PACKAGE_ADHOC_SIGN_IDENTITY))
APPLE_SIGN_IDENTITY ?=
APPLE_NOTARY_PROFILE ?=
APPLE_TEAM_ID ?=
STAPLE_MAX_ATTEMPTS ?= 6
STAPLE_RETRY_DELAY_SEC ?= 15

NON_APP_OBJS := $(filter-out $(OBJ_DIR)/src/main.o,$(APP_OBJS))

TEST_SRCS := $(filter-out $(TEST_DIR)/shared_theme_font_adapter_test.c $(TEST_DIR)/test_input_policy_entry.c,$(shell find $(TEST_DIR) -name '*.c'))
TEST_OBJS := $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_TARGET := $(TEST_BIN_DIR)/run_tests

SHAPE_SANITY_BIN := $(BIN_DIR)/shape_sanity_tool
SHAPE_PACK_TOOL_BIN := $(BIN_DIR)/shape_pack_tool
SHAPE_SYNC_SCRIPT := $(SHAPE_DIR)/sync_exports.sh

.DEFAULT_GOAL := all

.PHONY: all run run-ide-theme run-daw-theme clean test rebuild debug release format lint shape-sanity shape_pack_tool shape_to_pack export-assets test-shared-theme-font-adapter test-input-policy run-headless-smoke visual-harness test-stable test-legacy package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh scene-export-compile scene-pipeline-smoke

all: $(APP_TARGET)

debug:
	@$(MAKE) DEBUG=1

release:
	@$(MAKE) DEBUG=0

run: $(APP_TARGET)
	$(APP_TARGET)

run-ide-theme: $(APP_TARGET)
	LINE_DRAWING3D_USE_SHARED_THEME_FONT=1 LINE_DRAWING3D_USE_SHARED_THEME=1 LINE_DRAWING3D_USE_SHARED_FONT=1 LINE_DRAWING3D_THEME_PRESET=ide_gray LINE_DRAWING3D_FONT_PRESET=ide $(APP_TARGET)

run-daw-theme: $(APP_TARGET)
	LINE_DRAWING3D_USE_SHARED_THEME_FONT=1 LINE_DRAWING3D_USE_SHARED_THEME=1 LINE_DRAWING3D_USE_SHARED_FONT=1 LINE_DRAWING3D_THEME_PRESET=daw_default LINE_DRAWING3D_FONT_PRESET=daw_default $(APP_TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

run-headless-smoke:
	@$(MAKE) test-stable

scene-export-compile:
	@./tools/scene_export_compile_pipeline.sh \
		--layout ./tests/fixtures/ld3d2_layout_fixture.json \
		--scene-id scene_line_drawing_ld3d2 \
		--authoring-out ./tmp/ld3d2/scene_authoring.json \
		--runtime-out ./tmp/ld3d2/scene_runtime.json

scene-pipeline-smoke:
	@bash ./tests/test_scene_pipeline_fixtures.sh

visual-harness:
	@$(MAKE) all

package-desktop: all
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(APP_TARGET)" "$(PACKAGE_MACOS_DIR)/line-drawing-bin"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/line-drawing-launcher"
	@chmod +x "$(PACKAGE_MACOS_DIR)/line-drawing-bin" "$(PACKAGE_MACOS_DIR)/line-drawing-launcher"
	@"$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/line-drawing-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp -R config "$(PACKAGE_RESOURCES_DIR)/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/include"
	@cp -R include/fonts "$(PACKAGE_RESOURCES_DIR)/include/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts"
	@cp -R "$(SHARED_ASSETS_DIR)/fonts/." "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/data/runtime" "$(PACKAGE_RESOURCES_DIR)/data/snapshots" "$(PACKAGE_RESOURCES_DIR)/export"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" "$$dylib"; \
	done
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" "$(PACKAGE_MACOS_DIR)/line-drawing-bin"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" "$(PACKAGE_MACOS_DIR)/line-drawing-launcher"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/line-drawing-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/line-drawing-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/layout_config.json" || (echo "Missing config/layout_config.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/include/fonts/Lato/Lato-Regular.ttf" || (echo "Missing bundled local font"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing bundled shared font"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime lane"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/snapshots" || (echo "Missing snapshots lane"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/export" || (echo "Missing export lane"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk renderer shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/line-drawing-launcher" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop package synchronized: $(DESKTOP_APP_DIR)"

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(PACKAGE_APP_DIR)"
	@echo "Removed desktop package: $(PACKAGE_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

release-contract:
	@echo "RELEASE_PROGRAM_KEY=$(RELEASE_PROGRAM_KEY)"
	@echo "RELEASE_PRODUCT_NAME=$(RELEASE_PRODUCT_NAME)"
	@echo "RELEASE_BUNDLE_ID=$(RELEASE_BUNDLE_ID)"
	@echo "RELEASE_VERSION=$(RELEASE_VERSION)"
	@echo "RELEASE_CHANNEL=$(RELEASE_CHANNEL)"
	@test "$(RELEASE_PRODUCT_NAME)" = "sCulpt" || (echo "Unexpected release product"; exit 1)
	@test "$(RELEASE_PROGRAM_KEY)" = "line_drawing" || (echo "Unexpected release program key"; exit 1)
	@test "$(RELEASE_BUNDLE_ID)" = "com.cosm.sculpt" || (echo "Unexpected release bundle id"; exit 1)
	@test -f "$(RELEASE_VERSION_FILE)" || (echo "Missing VERSION file"; exit 1)
	@echo "release-contract passed."

release-clean:
	@rm -rf "$(RELEASE_DIR)"
	@echo "release-clean complete."

release-build:
	@$(MAKE) package-desktop-self-test
	@echo "release-build complete."

release-bundle-audit: release-build
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "Bundle identifier mismatch"; exit 1)
	@otool -L "$(PACKAGE_MACOS_DIR)/line-drawing-bin" > "$(RELEASE_DIR)/otool_line_drawing_bin.txt"
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		out="$(RELEASE_DIR)/otool_$$(basename "$$dylib").txt"; \
		otool -L "$$dylib" > "$$out"; \
	done
	@! rg -q '/opt/homebrew|/usr/local|/Users/' "$(RELEASE_DIR)"/otool_*.txt || (echo "Found non-portable dylib linkage"; exit 1)
	@! rg -q '@rpath/' "$(RELEASE_DIR)"/otool_*.txt || (echo "Found unresolved @rpath dylib linkage"; exit 1)
	@"$(PACKAGE_MACOS_DIR)/line-drawing-launcher" --print-config > "$(RELEASE_DIR)/print_config.txt"
	@rg -q '^LINE_DRAWING_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" || (echo "Missing LINE_DRAWING_RUNTIME_DIR in launcher config"; exit 1)
	@rg -q '^VK_ICD_FILENAMES=' "$(RELEASE_DIR)/print_config.txt" || (echo "Missing VK_ICD_FILENAMES in launcher config"; exit 1)
	@echo "release-bundle-audit passed."

release-sign: release-bundle-audit
	@test -n "$(RELEASE_CODESIGN_IDENTITY)" || (echo "Missing signing identity"; exit 1)
	@echo "Signing with identity: $(RELEASE_CODESIGN_IDENTITY)"
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		codesign --force --timestamp --options runtime --sign "$(RELEASE_CODESIGN_IDENTITY)" "$$dylib"; \
	done
	@codesign --force --timestamp --options runtime --sign "$(RELEASE_CODESIGN_IDENTITY)" "$(PACKAGE_MACOS_DIR)/line-drawing-bin"
	@codesign --force --timestamp --options runtime --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" "$(PACKAGE_MACOS_DIR)/line-drawing-launcher"
	@codesign --force --timestamp --options runtime --sign "$(RELEASE_CODESIGN_IDENTITY)" "$(PACKAGE_APP_DIR)"
	@echo "release-sign complete."

release-verify: release-sign
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@set +e; spctl_out="$$(spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)" 2>&1)"; spctl_rc=$$?; set -e; \
	echo "$$spctl_out"; \
	if [ $$spctl_rc -eq 0 ]; then \
		echo "release-verify passed."; \
	elif printf '%s' "$$spctl_out" | rg -q 'source=Unnotarized Developer ID'; then \
		echo "release-verify passed (pre-notary signed state)."; \
	else \
		echo "release-verify failed."; \
		exit $$spctl_rc; \
	fi

release-verify-signed: release-verify
	@echo "release-verify-signed passed."

release-notarize: release-sign
	@test -n "$(APPLE_NOTARY_PROFILE)" || (echo "Missing APPLE_NOTARY_PROFILE"; exit 1)
	@mkdir -p "$(RELEASE_DIR)"
	@ditto -c -k --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json > "$(RELEASE_DIR)/notary_submit.json"
	@rg -q '"status"[[:space:]]*:[[:space:]]*"Accepted"' "$(RELEASE_DIR)/notary_submit.json" || (cat "$(RELEASE_DIR)/notary_submit.json" && echo "Notary submission was not accepted" && exit 1)
	@echo "release-notarize passed."

release-staple:
	@attempt=1; \
	while [ $$attempt -le $(STAPLE_MAX_ATTEMPTS) ]; do \
		if xcrun stapler staple "$(PACKAGE_APP_DIR)" && xcrun stapler validate "$(PACKAGE_APP_DIR)"; then \
			echo "release-staple passed."; \
			exit 0; \
		fi; \
		echo "staple attempt $$attempt/$(STAPLE_MAX_ATTEMPTS) failed; retrying in $(STAPLE_RETRY_DELAY_SEC)s"; \
		sleep $(STAPLE_RETRY_DELAY_SEC); \
		attempt=$$((attempt+1)); \
	done; \
	echo "release-staple failed."; \
	exit 1

release-verify-notarized: release-staple
	@spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)"
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-verify-notarized passed."

release-artifact: release-verify-notarized
	@mkdir -p "$(RELEASE_DIR)"
	@ditto -c -k --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@shasum -a 256 "$(RELEASE_APP_ZIP)" > "$(RELEASE_APP_ZIP).sha256"
	@{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(RELEASE_PROGRAM_KEY)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "zip=$(RELEASE_APP_ZIP)"; \
		echo "sha256=$$(cut -d' ' -f1 "$(RELEASE_APP_ZIP).sha256")"; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

release-distribute: release-artifact
	@echo "release-distribute passed."

release-desktop-refresh: release-distribute
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@spctl --assess --type execute --verbose=2 "$(DESKTOP_APP_DIR)"
	@echo "release-desktop-refresh passed."

test-stable: test

test-legacy:
	@$(MAKE) test-shared-theme-font-adapter || true

SHARED_THEME_FONT_ADAPTER_TEST_SRCS := tests/shared_theme_font_adapter_test.c
SHARED_THEME_FONT_ADAPTER_TEST_BIN := $(BUILD_DIR)/tests/shared_theme_font_adapter_test
INPUT_POLICY_TEST_SRCS := tests/test_input_policy_entry.c tests/test_input_policy.c src/Input/input_routing_policy.c
INPUT_POLICY_TEST_BIN := $(BUILD_DIR)/tests/input_policy_test

$(SHARED_THEME_FONT_ADAPTER_TEST_BIN): $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS) src/UI/shared_theme_font_adapter.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c $(CORE_BASE_DIR)/src/core_base.c $(CORE_IO_DIR)/src/core_io.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include \
		tests/shared_theme_font_adapter_test.c src/UI/shared_theme_font_adapter.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c $(CORE_BASE_DIR)/src/core_base.c $(CORE_IO_DIR)/src/core_io.c \
		-o $(SHARED_THEME_FONT_ADAPTER_TEST_BIN) $(LDFLAGS)

test-shared-theme-font-adapter: $(SHARED_THEME_FONT_ADAPTER_TEST_BIN)
	@$(SHARED_THEME_FONT_ADAPTER_TEST_BIN) || (echo "shared theme/font adapter test failed."; exit 1)

$(INPUT_POLICY_TEST_BIN): $(INPUT_POLICY_TEST_SRCS) tests/test_framework.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests \
		$(INPUT_POLICY_TEST_SRCS) tests/test_framework.c \
		-o $(INPUT_POLICY_TEST_BIN) $(LDFLAGS)

test-input-policy: $(INPUT_POLICY_TEST_BIN)
	@$(INPUT_POLICY_TEST_BIN) || (echo "input policy test failed."; exit 1)

shape-sanity: $(SHAPE_SANITY_BIN)

SHAPE_TRACE_TOOL_SRC := $(TOOLS_DIR)/shape_trace_tool.c
SHAPE_TRACE_TOOL_BIN := $(BIN_DIR)/shape_trace_tool
SHAPE_TRACE_TOOL_SRCS := \
	$(SHAPE_TRACE_TOOL_SRC) \
	$(TOOLS_DIR)/ShapeLib/shape_core.c \
	$(TOOLS_DIR)/ShapeLib/shape_json.c \
	$(CORE_TRACE_DIR)/src/core_trace.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(EXT_DIR)/cjson/cJSON.c
SHAPE_TRACE_TOOL_INCS := \
	-I$(SRC_DIR) -I$(EXT_DIR) -I$(SHAPE_DIR) \
	-I$(CORE_TRACE_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_BASE_DIR)/include

$(SHAPE_SANITY_BIN): src/Tools/shape_sanity_tool.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

export-assets:
	@SHAPE_ASSET_DIR="$(SHAPE_ASSET_DIR)" $(SHAPE_SYNC_SCRIPT)

rebuild: clean all

$(APP_TARGET): $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(APP_CFLAGS) -MMD -MP -c $< -o $@

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
SHAPE_TOOL_SRCS := \
	$(TOOLS_DIR)/shape_tool.c \
	$(TOOLS_DIR)/shape_from_layout.c \
	$(TOOLS_DIR)/shape_export.c \
	$(TOOLS_DIR)/canonical_scene_export.c \
	$(TOOLS_DIR)/canonical_scene_export_primitives.c \
	$(TOOLS_DIR)/shape_dataset.c \
	$(TOOLS_DIR)/global_state_stub.c \
	$(shell find $(TOOLS_DIR)/ShapeLib -name '*.c')
SHAPE_TOOL_OBJS := $(patsubst $(TOOLS_DIR)/%.c,$(BUILD_DIR)/tools/%.o,$(SHAPE_TOOL_SRCS))
SHAPE_TOOL_SUPPORT_SRCS := \
	$(EXT_DIR)/cjson/cJSON.c \
	$(SRC_DIR)/Core/data_paths.c \
	$(SRC_DIR)/Core/adapters/space_mode_adapter.c \
	$(SRC_DIR)/Layout/layout.c \
	$(SRC_DIR)/Layout/layout_json.c \
	$(SRC_DIR)/Layout/scene/layout_scene3d.c \
	$(SRC_DIR)/Layout/scene/layout_object_store.c \
	$(SRC_DIR)/Layout/primitives/layout_primitives_create.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_DATA_DIR)/src/core_data.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_MATH_DIR)/src/core_math.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c
SHAPE_TOOL_SHARED_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SHAPE_TOOL_SUPPORT_SRCS))
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

SHAPE_PACK_TOOL_SRCS := \
	$(TOOLS_DIR)/shape_pack_tool.c \
	$(TOOLS_DIR)/shape_dataset.c \
	$(TOOLS_DIR)/global_state_stub.c \
	$(SHAPE_TOOL_SUPPORT_SRCS)

shape_pack_tool:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(SHAPE_PACK_TOOL_BIN) $(SHAPE_PACK_TOOL_SRCS) $(LDFLAGS)

shape_to_pack: shape_pack_tool
	@if [ -z "$(LAYOUT)" ] || [ -z "$(PACK)" ]; then \
		echo "usage: make shape_to_pack LAYOUT=/path/layout.json PACK=/path/output.pack [AXIS=xy|yz|xz]"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(PACK)")"
	@axis="$${AXIS:-xy}"; \
	$(SHAPE_PACK_TOOL_BIN) "$(LAYOUT)" "$(PACK)" --axis "$$axis"

.PHONY: shape_trace_tool shape_to_trace shape_to_trace_batch
shape_trace_tool:
	@mkdir -p $(BIN_DIR)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -g $(SHAPE_TRACE_TOOL_INCS) -o $(SHAPE_TRACE_TOOL_BIN) $(SHAPE_TRACE_TOOL_SRCS) -lm

shape_to_trace: shape_trace_tool
	@if [ -z "$(SHAPE)" ] || [ -z "$(TRACE)" ]; then \
		echo "usage: make shape_to_trace SHAPE=/path/export_shape.json TRACE=/path/output.trace.pack"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(TRACE)")"
	@$(SHAPE_TRACE_TOOL_BIN) "$(SHAPE)" "$(TRACE)"

shape_to_trace_batch: shape_trace_tool
	@for shape in export/*.json; do \
		base=$$(basename "$$shape" .json); \
		out="export/$${base}_trace_v0.pack"; \
		echo "trace: $$shape -> $$out"; \
		$(SHAPE_TRACE_TOOL_BIN) "$$shape" "$$out" || exit 1; \
	done

-include $(SHAPE_TOOL_OBJS:.o=.d)
