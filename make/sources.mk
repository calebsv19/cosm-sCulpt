KIT_RENDER_SRCS := \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c \
	$(KIT_RENDER_DIR)/src/kit_render_external_text.c
KIT_PANE_SRCS := $(KIT_PANE_DIR)/src/kit_pane.c
KIT_WORKSPACE_AUTHORING_SRCS := \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c

APP_SRCS := $(shell find $(SRC_DIR) -name '*.c' ! -path '$(TOOLS_DIR)/*')
VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -name '*.c')
SHAPE_LIB_SRCS := $(shell find $(TOOLS_DIR)/ShapeLib -name '*.c')
SHAPE_BRIDGE_SRCS := $(TOOLS_DIR)/shape_from_layout.c $(TOOLS_DIR)/shape_export.c $(TOOLS_DIR)/shape_dataset.c $(TOOLS_DIR)/canonical_scene_export.c $(TOOLS_DIR)/canonical_scene_export_primitives.c $(TOOLS_DIR)/scene_export.c $(TOOLS_DIR)/scene_import.c
EXT_SRCS := $(EXT_DIR)/cjson/cJSON.c
CORE_TIME_SRCS := $(CORE_TIME_DIR)/src/core_time.c
ifeq ($(UNAME_S),Darwin)
	CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_mac.c
else
	CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_posix.c
endif
CORE_SRCS := $(CORE_BASE_DIR)/src/core_base.c $(CORE_IO_DIR)/src/core_io.c $(CORE_DATA_DIR)/src/core_data.c $(CORE_PACK_DIR)/src/core_pack.c $(CORE_MATH_DIR)/src/core_math.c $(CORE_TIME_SRCS) $(CORE_SCENE_DIR)/src/core_scene.c $(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c $(CORE_OBJECT_DIR)/src/core_object.c $(CORE_UNITS_DIR)/src/core_units.c $(CORE_LAYOUT_DIR)/src/core_layout.c $(CORE_PANE_DIR)/src/core_pane.c $(CORE_PANE_MODULE_DIR)/src/core_pane_module.c $(CORE_THEME_DIR)/src/core_theme.c $(CORE_FONT_DIR)/src/core_font.c
TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -name '*.c')
ALL_SRCS := $(APP_SRCS) $(VK_RENDERER_SRCS) $(KIT_RENDER_SRCS) $(KIT_PANE_SRCS) $(KIT_WORKSPACE_AUTHORING_SRCS) $(SHAPE_LIB_SRCS) $(SHAPE_BRIDGE_SRCS) $(EXT_SRCS) $(CORE_SRCS) $(TIMER_HUD_SRCS)

TEST_SRCS := $(filter-out $(TEST_DIR)/shared_theme_font_adapter_test.c $(TEST_DIR)/test_input_policy_entry.c,$(shell find $(TEST_DIR) -name '*.c'))
SHARED_THEME_FONT_ADAPTER_TEST_SRCS := tests/shared_theme_font_adapter_test.c
INPUT_POLICY_TEST_SRCS := tests/test_input_policy_entry.c tests/test_input_policy.c src/Input/input_routing_policy.c

SHAPE_TRACE_TOOL_SRC := $(TOOLS_DIR)/shape_trace_tool.c
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

SHAPE_TOOL_SRCS := \
	$(TOOLS_DIR)/shape_tool.c \
	$(TOOLS_DIR)/shape_from_layout.c \
	$(TOOLS_DIR)/shape_export.c \
	$(TOOLS_DIR)/canonical_scene_export.c \
	$(TOOLS_DIR)/canonical_scene_export_primitives.c \
	$(TOOLS_DIR)/shape_dataset.c \
	$(TOOLS_DIR)/global_state_stub.c \
	$(shell find $(TOOLS_DIR)/ShapeLib -name '*.c')
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
SHAPE_PACK_TOOL_SRCS := \
	$(TOOLS_DIR)/shape_pack_tool.c \
	$(TOOLS_DIR)/shape_dataset.c \
	$(TOOLS_DIR)/global_state_stub.c \
	$(SHAPE_TOOL_SUPPORT_SRCS)
