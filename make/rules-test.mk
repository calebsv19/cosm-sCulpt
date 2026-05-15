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

test-stable: test

test-legacy:
	@$(MAKE) test-shared-theme-font-adapter || true

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
