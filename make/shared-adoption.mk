# Consumer policy only; selected shared implementation stays unchanged.
L4_STAMP := $(PROGRAM_BUILD_DIR)/build-identity.stamp
L4_RULES = $(filter-out %.d,$(MAKEFILE_LIST))
L4_OBJECTS := $(sort $(foreach v,$(filter %_OBJS %_OBJ,$(.VARIABLES)),$($(v))))
L4_COMPILER_INPUTS :=
ifeq ($(BUILD_TOOLCHAIN),fisics)
L4_COMPILER_INPUTS := tools/fisics_compile.py $(FISICS_BIN)
endif
.PHONY: l4-force shared-input-check
l4-force:
shared-input-check:
	@python3 tools/verify_shared_adoption.py --manifest config/shared_adoption.json --module "shape=$(SHAPE_DIR)" --module "kit/kit_render=$(KIT_RENDER_DIR)" --module "kit/kit_ui=$(KIT_UI_DIR)" --module "kit/kit_pane=$(KIT_PANE_DIR)" --module "kit/kit_viewport3d=$(KIT_VIEWPORT3D_DIR)" --module "kit/kit_workspace_authoring=$(KIT_WORKSPACE_AUTHORING_DIR)" --module "vk_renderer=$(VK_RENDERER_DIR)" --module "vk_runtime=$(VK_RUNTIME_DIR)" --module "core/core_base=$(CORE_BASE_DIR)" --module "core/core_io=$(CORE_IO_DIR)" --module "core/core_data=$(CORE_DATA_DIR)" --module "core/core_math=$(CORE_MATH_DIR)" --module "core/core_time=$(CORE_TIME_DIR)" --module "core/core_mesh_asset=$(CORE_MESH_ASSET_DIR)" --module "core/core_mesh_compile=$(CORE_MESH_COMPILE_DIR)" --module "core/core_mesh_preview=$(CORE_MESH_PREVIEW_DIR)" --module "core/core_scene=$(CORE_SCENE_DIR)" --module "core/core_scene_view=$(CORE_SCENE_VIEW_DIR)" --module "core/core_screen_pick=$(CORE_SCREEN_PICK_DIR)" --module "core/core_viewport3d=$(CORE_VIEWPORT3D_DIR)" --module "core/core_object=$(CORE_OBJECT_DIR)" --module "core/core_units=$(CORE_UNITS_DIR)" --module "core/core_layout=$(CORE_LAYOUT_DIR)" --module "core/core_scene_compile=$(CORE_SCENE_COMPILE_DIR)" --module "core/core_pane=$(CORE_PANE_DIR)" --module "core/core_pane_module=$(CORE_PANE_MODULE_DIR)" --module "core/core_pack=$(CORE_PACK_DIR)" --module "core/core_trace=$(CORE_TRACE_DIR)" --module "core/core_theme=$(CORE_THEME_DIR)" --module "core/core_font=$(CORE_FONT_DIR)" --module "timer_hud=$(TIMER_HUD_DIR)" --module "core/core_workers=$(CORE_WORKERS_DIR)" --module "core/core_queue=$(CORE_QUEUE_DIR)"
$(L4_STAMP): l4-force | shared-input-check
	@mkdir -p "$(@D)"
	@{ printf '%s\n' '$(CC)' '$(CLANG_CC)' '$(APP_CFLAGS)' '$(CFLAGS)' '$(DEPFLAGS)' '$(LDFLAGS)' '$(SHARED_ROOT)'; $(CLANG_CC) --version; cksum $(L4_RULES) tools/verify_shared_adoption.py config/shared_adoption.json $(L4_COMPILER_INPUTS); } > "$@.tmp"
	@cmp -s "$@.tmp" "$@" 2>/dev/null || mv "$@.tmp" "$@"
	@rm -f "$@.tmp"
$(L4_OBJECTS): $(L4_STAMP) | shared-input-check
$(APP_TARGET) $(TEST_TARGET) $(SHAPE_TOOL_BIN) $(AGENT_SCENE_TOOL_BIN): | shared-input-check
