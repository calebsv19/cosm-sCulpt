include make/config.mk
include make/target.mk
include make/shared.mk
include make/paths.mk
include make/flags.mk
include make/sources.mk
include make/objects.mk

.PHONY: all run run-ide-theme run-daw-theme clean test rebuild debug release format lint shape-sanity shape_pack_tool shape_to_pack export-assets test-shared-theme-font-adapter test-input-policy run-headless-smoke visual-harness visual-artifact visual-artifact-editor test-stable test-legacy memory-check-build memory-check-run memory-check-audit package-desktop package-desktop-smoke package-desktop-print-config package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh package-linux-desktop-contract package-linux-desktop-host-check package-linux-desktop-clean package-linux-desktop package-linux-desktop-self-test package-linux-desktop-determinism-test release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh scene-export-compile scene-pipeline-smoke agent-scene-smoke imported_mesh_harness imported-mesh-harness-smoke shape_tool agent_scene_tool shape_trace_tool shape_to_trace shape_to_trace_batch clang-build fisics-build toolchain-contract dump-sema-canonical-scene-export dump-sema-canonical-scene-export-primitives dump-sema-scene-import print-app-target print-program-bin-dir print-shape-tool-bin print-agent-scene-tool-bin print-imported-mesh-harness-bin

include make/rules-memory-check.mk
include make/rules-build.mk
include make/rules-test.mk
include make/package-macos.mk
include make/package-linux-desktop.mk
include make/release.mk
include make/tools-shape.mk

print-app-target:
	@printf '%s\n' "$(APP_TARGET)"

print-program-bin-dir:
	@printf '%s\n' "$(PROGRAM_BIN_DIR)"

print-shape-tool-bin:
	@printf '%s\n' "$(SHAPE_TOOL_BIN)"

print-agent-scene-tool-bin:
	@printf '%s\n' "$(AGENT_SCENE_TOOL_BIN)"

print-imported-mesh-harness-bin:
	@printf '%s\n' "$(IMPORTED_MESH_HARNESS_BIN)"
