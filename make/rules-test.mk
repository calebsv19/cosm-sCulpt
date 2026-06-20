run: $(APP_TARGET)
	$(APP_TARGET)

run-ide-theme: $(APP_TARGET)
	LINE_DRAWING3D_USE_SHARED_THEME_FONT=1 LINE_DRAWING3D_USE_SHARED_THEME=1 LINE_DRAWING3D_USE_SHARED_FONT=1 LINE_DRAWING3D_THEME_PRESET=ide_gray LINE_DRAWING3D_FONT_PRESET=ide $(APP_TARGET)

run-daw-theme: $(APP_TARGET)
	LINE_DRAWING3D_USE_SHARED_THEME_FONT=1 LINE_DRAWING3D_USE_SHARED_THEME=1 LINE_DRAWING3D_USE_SHARED_FONT=1 LINE_DRAWING3D_THEME_PRESET=daw_default LINE_DRAWING3D_FONT_PRESET=daw_default $(APP_TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET) $(ARGS)

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

agent-scene-smoke: agent_scene_tool
	@bash ./tests/test_agent_scene_tool.sh

agent-scene-failure-smoke: agent_scene_tool
	@bash ./tests/test_agent_scene_tool.sh failures

visual-harness:
	@$(MAKE) all

VISUAL_ARTIFACT_PATH ?= $(CURDIR)/visual_artifacts/line_drawing_first_frame.bmp
VISUAL_ARTIFACT_EDITOR_PATH ?= $(CURDIR)/visual_artifacts/line_drawing_editor_first_frame.bmp

visual-artifact: $(APP_TARGET)
	@mkdir -p "$(dir $(VISUAL_ARTIFACT_PATH))"
	@rm -f "$(VISUAL_ARTIFACT_PATH)"
	@LINE_DRAWING_VISUAL_ARTIFACT="$(VISUAL_ARTIFACT_PATH)" $(APP_TARGET)
	@test -s "$(VISUAL_ARTIFACT_PATH)" || (echo "visual-artifact failed: missing or empty $(VISUAL_ARTIFACT_PATH)"; exit 1)
	@printf 'visual-artifact: %s\n' "$(VISUAL_ARTIFACT_PATH)"

visual-artifact-editor: $(APP_TARGET)
	@mkdir -p "$(dir $(VISUAL_ARTIFACT_EDITOR_PATH))"
	@rm -f "$(VISUAL_ARTIFACT_EDITOR_PATH)"
	@LINE_DRAWING_VISUAL_ARTIFACT="$(VISUAL_ARTIFACT_EDITOR_PATH)" LINE_DRAWING_VISUAL_ARTIFACT_MODE=editor $(APP_TARGET)
	@test -s "$(VISUAL_ARTIFACT_EDITOR_PATH)" || (echo "visual-artifact-editor failed: missing or empty $(VISUAL_ARTIFACT_EDITOR_PATH)"; exit 1)
	@printf 'visual-artifact-editor: %s\n' "$(VISUAL_ARTIFACT_EDITOR_PATH)"

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
