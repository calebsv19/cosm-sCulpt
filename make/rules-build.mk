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

$(TEST_TARGET): $(NON_APP_OBJS) $(TEST_OBJS)
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
