include make/config.mk
include make/target.mk
include make/shared.mk
include make/paths.mk
include make/flags.mk
include make/sources.mk
include make/objects.mk

.PHONY: all run run-ide-theme run-daw-theme clean test rebuild debug release format lint shape-sanity shape_pack_tool shape_to_pack export-assets test-shared-theme-font-adapter test-input-policy run-headless-smoke visual-harness test-stable test-legacy package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh scene-export-compile scene-pipeline-smoke shape_tool shape_trace_tool shape_to_trace shape_to_trace_batch

include make/rules-build.mk
include make/rules-test.mk
include make/package-macos.mk
include make/release.mk
include make/tools-shape.mk
