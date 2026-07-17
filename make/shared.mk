SRC_DIR := src
TOOLS_DIR := $(SRC_DIR)/Tools
EXT_DIR := external
TEST_DIR := tests

SHARED_ROOT ?= third_party/codework_shared
SHARED_WORKSPACE_DIR ?= ../shared
SHARED_ASSETS_DIR := $(SHARED_ROOT)/assets
SHAPE_DIR := $(SHARED_ROOT)/shape
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
KIT_PANE_DIR := $(SHARED_ROOT)/kit/kit_pane
KIT_VIEWPORT3D_DIR := $(SHARED_ROOT)/kit/kit_viewport3d
KIT_WORKSPACE_AUTHORING_DIR := $(SHARED_ROOT)/kit/kit_workspace_authoring
VK_RENDERER_DIR := $(SHARED_ROOT)/vk_renderer
CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_MATH_DIR := $(SHARED_ROOT)/core/core_math
CORE_TIME_DIR := $(SHARED_ROOT)/core/core_time
CORE_MESH_ASSET_DIR := $(SHARED_ROOT)/core/core_mesh_asset
CORE_MESH_COMPILE_DIR := $(SHARED_ROOT)/core/core_mesh_compile
CORE_MESH_PREVIEW_DIR := $(SHARED_ROOT)/core/core_mesh_preview
CORE_SCENE_DIR := $(SHARED_ROOT)/core/core_scene
CORE_SCENE_VIEW_DIR := $(SHARED_ROOT)/core/core_scene_view
CORE_SCREEN_PICK_DIR := $(SHARED_ROOT)/core/core_screen_pick
CORE_VIEWPORT3D_DIR := $(SHARED_ROOT)/core/core_viewport3d
CORE_OBJECT_DIR := $(SHARED_ROOT)/core/core_object
CORE_UNITS_DIR := $(SHARED_ROOT)/core/core_units
CORE_LAYOUT_DIR := $(SHARED_ROOT)/core/core_layout
CORE_SCENE_COMPILE_DIR := $(SHARED_ROOT)/core/core_scene_compile
CORE_PANE_DIR := $(SHARED_ROOT)/core/core_pane
CORE_PANE_MODULE_DIR := $(SHARED_ROOT)/core/core_pane_module
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
CORE_TRACE_DIR := $(SHARED_ROOT)/core/core_trace
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
TIMER_HUD_DIR := $(SHARED_ROOT)/timer_hud

ifeq ($(wildcard $(CORE_SCENE_VIEW_DIR)/include/core_scene_view.h),)
CORE_SCENE_VIEW_DIR := $(SHARED_WORKSPACE_DIR)/core/core_scene_view
endif
ifeq ($(wildcard $(CORE_VIEWPORT3D_DIR)/include/core_viewport3d.h),)
CORE_VIEWPORT3D_DIR := $(SHARED_WORKSPACE_DIR)/core/core_viewport3d
endif
ifeq ($(wildcard $(CORE_SCREEN_PICK_DIR)/include/core_screen_pick.h),)
CORE_SCREEN_PICK_DIR := $(SHARED_WORKSPACE_DIR)/core/core_screen_pick
endif
ifeq ($(wildcard $(KIT_VIEWPORT3D_DIR)/include/kit_viewport3d.h),)
KIT_VIEWPORT3D_DIR := $(SHARED_WORKSPACE_DIR)/kit/kit_viewport3d
endif

SHAPE_SYNC_SCRIPT := $(SHAPE_DIR)/sync_exports.sh

IMPORTED_MESH_HARNESS_SHARED_ROOT ?= $(SHARED_ROOT)
IMPORTED_MESH_HARNESS_CORE_BASE_DIR := $(IMPORTED_MESH_HARNESS_SHARED_ROOT)/core/core_base
IMPORTED_MESH_HARNESS_CORE_IO_DIR := $(IMPORTED_MESH_HARNESS_SHARED_ROOT)/core/core_io
IMPORTED_MESH_HARNESS_CORE_OBJECT_DIR := $(IMPORTED_MESH_HARNESS_SHARED_ROOT)/core/core_object
IMPORTED_MESH_HARNESS_CORE_UNITS_DIR := $(IMPORTED_MESH_HARNESS_SHARED_ROOT)/core/core_units
IMPORTED_MESH_HARNESS_CORE_MESH_ASSET_DIR := $(IMPORTED_MESH_HARNESS_SHARED_ROOT)/core/core_mesh_asset
IMPORTED_MESH_HARNESS_CORE_MESH_COMPILE_DIR := $(IMPORTED_MESH_HARNESS_SHARED_ROOT)/core/core_mesh_compile
IMPORTED_MESH_HARNESS_CORE_MESH_PREVIEW_DIR := $(IMPORTED_MESH_HARNESS_SHARED_ROOT)/core/core_mesh_preview
