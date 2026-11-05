all: $(APP_TARGET)

clang-build:
	@$(MAKE) clean BUILD_TOOLCHAIN=clang
	@$(MAKE) BUILD_TOOLCHAIN=clang all

fisics-build:
	@$(MAKE) clean BUILD_TOOLCHAIN=fisics
	@$(MAKE) BUILD_TOOLCHAIN=fisics all

toolchain-contract:
	@echo "Program root:    $(CURDIR)"
	@echo "clang build:     $(CLANG_CC) $(BASE_CFLAGS) ..."
	@echo "fisiCs build:    $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(BASE_CFLAGS) ..."
	@echo "units sema src:  src/Tools/canonical_scene_export.c"
	@echo "units sema:      $(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) --dump-sema -c src/Tools/canonical_scene_export.c -o $(FISICS_TOOLCHAIN_DIR)/canonical_scene_export.o"
	@echo "units output:    $(LINE_DRAWING_UNITS_SEMA_OUTPUT)"
	@echo "primitives src:  src/Tools/canonical_scene_export_primitives.c"
	@echo "primitives sema: $(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) --dump-sema -c src/Tools/canonical_scene_export_primitives.c -o $(FISICS_TOOLCHAIN_DIR)/canonical_scene_export_primitives.o"
	@echo "primitives out:  $(LINE_DRAWING_UNITS_PRIMITIVES_SEMA_OUTPUT)"
	@echo "import src:      src/Tools/scene_import.c"
	@echo "import sema:     $(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) --dump-sema -c src/Tools/scene_import.c -o $(FISICS_TOOLCHAIN_DIR)/scene_import.o"
	@echo "import out:      $(LINE_DRAWING_UNITS_IMPORT_SEMA_OUTPUT)"

dump-sema-canonical-scene-export:
	@mkdir -p "$(FISICS_TOOLCHAIN_DIR)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(BASE_CFLAGS) --dump-sema -c "src/Tools/canonical_scene_export.c" -o "$(FISICS_TOOLCHAIN_DIR)/canonical_scene_export.o" > "$(LINE_DRAWING_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(LINE_DRAWING_UNITS_SEMA_OUTPUT)"

dump-sema-canonical-scene-export-primitives:
	@mkdir -p "$(FISICS_TOOLCHAIN_DIR)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(BASE_CFLAGS) --dump-sema -c "src/Tools/canonical_scene_export_primitives.c" -o "$(FISICS_TOOLCHAIN_DIR)/canonical_scene_export_primitives.o" > "$(LINE_DRAWING_UNITS_PRIMITIVES_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(LINE_DRAWING_UNITS_PRIMITIVES_SEMA_OUTPUT)"

dump-sema-scene-import:
	@mkdir -p "$(FISICS_TOOLCHAIN_DIR)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(BASE_CFLAGS) --dump-sema -c "src/Tools/scene_import.c" -o "$(FISICS_TOOLCHAIN_DIR)/scene_import.o" > "$(LINE_DRAWING_UNITS_IMPORT_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(LINE_DRAWING_UNITS_IMPORT_SEMA_OUTPUT)"

debug:
	@$(MAKE) DEBUG=1

release:
	@$(MAKE) DEBUG=0

export-assets:
	@SHAPE_ASSET_DIR="$(SHAPE_ASSET_DIR)" $(SHAPE_SYNC_SCRIPT)

rebuild: clean all

$(APP_TARGET): $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(PROGRAM_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(APP_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(HOST_TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CFLAGS) -Isrc -Iexternal -MMD -MP -c $< -o $@

# The host suite can stop the real worker at the snapshot boundary. Production
# and the standalone proof tools keep the normal job object.
MESH_JOB_SRC := src/Layout/scene/layout_mesh_prepare_job.c
MESH_JOB_TEST_OBJ := $(HOST_TEST_OBJ_DIR)/mesh_prepare_gated.o
$(MESH_JOB_TEST_OBJ): $(MESH_JOB_SRC) make/rules-build.mk
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CFLAGS) -DLayout_MeshSourceSnapshot=ld_test_mesh_snapshot -Dcalloc=ld_proof_calloc -Dcore_workers_submit=ld_proof_submit -MMD -MP -c $< -o $@
-include $(MESH_JOB_TEST_OBJ:.o=.d)

MESH_PROOF_SRCS := src/Layout/scene/layout_mesh_prepared_store.c src/Layout/scene/layout_mesh_source_snapshot.c $(CORE_PACK_DIR)/src/core_pack.c $(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview_lod.c
MESH_STORE_TEST_OBJ := $(HOST_TEST_OBJ_DIR)/mesh_store_fault.o
MESH_SNAPSHOT_TEST_OBJ := $(HOST_TEST_OBJ_DIR)/mesh_snapshot_fault.o
$(MESH_STORE_TEST_OBJ): src/Layout/scene/layout_mesh_prepared_store.c make/rules-build.mk
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CFLAGS) -Dmalloc=ld_proof_store_malloc -Drename=ld_proof_rename -MMD -MP -c $< -o $@
$(MESH_SNAPSHOT_TEST_OBJ): src/Layout/scene/layout_mesh_source_snapshot.c make/rules-build.mk
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CFLAGS) -Dmalloc=ld_proof_snapshot_malloc -MMD -MP -c $< -o $@
MESH_PACK_TEST_OBJ := $(HOST_TEST_OBJ_DIR)/mesh_pack_fault.o
MESH_LOD_TEST_OBJ := $(HOST_TEST_OBJ_DIR)/mesh_lod_fault.o
$(MESH_PACK_TEST_OBJ): $(CORE_PACK_DIR)/src/core_pack.c make/rules-build.mk
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CFLAGS) -Dcore_realloc=ld_proof_pack_realloc -Dfwrite=ld_proof_write -MMD -MP -c $< -o $@
$(MESH_LOD_TEST_OBJ): $(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview_lod.c make/rules-build.mk
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CFLAGS) -Dcalloc=ld_proof_lod_calloc -MMD -MP -c $< -o $@
-include $(MESH_PACK_TEST_OBJ:.o=.d) $(MESH_LOD_TEST_OBJ:.o=.d)
-include $(MESH_STORE_TEST_OBJ:.o=.d) $(MESH_SNAPSHOT_TEST_OBJ:.o=.d)

$(TEST_TARGET): $(filter-out $(addprefix $(PROGRAM_OBJ_DIR)/,$(MESH_JOB_SRC:.c=.o) $(MESH_PROOF_SRCS:.c=.o)),$(NON_APP_OBJS)) $(MESH_JOB_TEST_OBJ) $(MESH_STORE_TEST_OBJ) $(MESH_SNAPSHOT_TEST_OBJ) $(MESH_PACK_TEST_OBJ) $(MESH_LOD_TEST_OBJ) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CLANG_CC) $^ -o $@ $(LDFLAGS) $(FISICS_MEMCHECK_LINK_LIBS)

clean:
	rm -rf $(BUILD_DIR)

-include $(APP_OBJS:.o=.d)
-include $(TEST_OBJS:.o=.d)

# Native window observation/probes use real SDL, outside the drawing facade.
$(PROGRAM_OBJ_DIR)/$(KIT_UI_DIR)/src/kit_ui_window_sdl.o: $(KIT_UI_DIR)/src/kit_ui_window_sdl.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(PROGRAM_OBJ_DIR)/$(KIT_UI_DIR)/src/kit_ui_window_probe_sdl.o: $(KIT_UI_DIR)/src/kit_ui_window_probe_sdl.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(PROGRAM_OBJ_DIR)/$(KIT_UI_DIR)/src/kit_ui_interaction_sdl.o: $(KIT_UI_DIR)/src/kit_ui_interaction_sdl.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Clipboard/IME platform helpers use real SDL; painting stays app-owned Vulkan.
$(PROGRAM_OBJ_DIR)/$(KIT_UI_DIR)/src/kit_ui_text_edit_sdl.o: $(KIT_UI_DIR)/src/kit_ui_text_edit_sdl.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
$(PROGRAM_OBJ_DIR)/$(KIT_UI_DIR)/src/kit_ui_native_text_sdl.o: $(KIT_UI_DIR)/src/kit_ui_native_text_sdl.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
