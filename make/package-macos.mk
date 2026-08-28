package-desktop:
	@echo "Preparing desktop package..."
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" "$(PACKAGE_SOURCE_BIN)"
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $(PACKAGE_BUNDLE_ID)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleName $(PACKAGE_DISPLAY_NAME)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName $(PACKAGE_DISPLAY_NAME)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(RELEASE_VERSION)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $(RELEASE_VERSION)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :LineDrawingPackageProfile string $(PACKAGE_PROFILE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :LineDrawingRuntimeNamespace string $(PACKAGE_RUNTIME_NAMESPACE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :LineDrawingLogNamespace string $(PACKAGE_LOG_NAMESPACE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :LineDrawingBuildLabel string $(PACKAGE_BUILD_LABEL)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(PACKAGE_MACOS_DIR)/line-drawing-bin"
	@cp "$(PACKAGE_LAUNCHER_SCRIPT_SRC)" "$(PACKAGE_LAUNCHER_SCRIPT_PATH)"
	@$(CLANG_CC) -std=c11 -Os -Wall -Wextra -Werror -Wpedantic "$(PACKAGE_LAUNCHER_NATIVE_SRC)" -o "$(PACKAGE_MACOS_DIR)/line-drawing-launcher"
	@chmod +x "$(PACKAGE_MACOS_DIR)/line-drawing-bin" "$(PACKAGE_MACOS_DIR)/line-drawing-launcher" "$(PACKAGE_LAUNCHER_SCRIPT_PATH)"
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		/usr/bin/iconutil -c icns -o "$(PACKAGE_BUNDLED_ICON_PATH)" "$(PACKAGE_APP_ICONSET_SRC)" || exit 1; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "warning: no app icon source found at $(PACKAGE_APP_ICON_SRC) or $(PACKAGE_APP_ICONSET_SRC)"; \
	fi
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/line-drawing-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
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
	@if [ "$(PACKAGE_EMBED_BUILD_IDENTITY)" = "1" ]; then \
		python3 "$(MEW1_TOOL)" write-identity \
			--output "$(PACKAGE_RESOURCES_DIR)/build_identity.json" \
			--source-root "$(CURDIR)" \
			--binary "$(PACKAGE_MACOS_DIR)/line-drawing-bin" \
			--profile "$(PACKAGE_PROFILE)" \
			--program line_drawing \
			--product sCulpt \
			--version "$(RELEASE_VERSION)" \
			--architecture "$(TARGET_ARCH)" \
			--toolchain "$(PACKAGE_TOOLCHAIN)" \
			--build-label "$(PACKAGE_BUILD_LABEL)"; \
	fi
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@echo "Checking desktop package: app=$(PACKAGE_APP_DIR) resources=$(PACKAGE_RESOURCES_DIR)"
	@test -x "$(PACKAGE_MACOS_DIR)/line-drawing-launcher" || (echo "Missing launcher at $(PACKAGE_MACOS_DIR)/line-drawing-launcher"; exit 1)
	@test -x "$(PACKAGE_LAUNCHER_SCRIPT_PATH)" || (echo "Missing launcher resource at $(PACKAGE_LAUNCHER_SCRIPT_PATH)"; exit 1)
	@file "$(PACKAGE_MACOS_DIR)/line-drawing-launcher" | rg -q 'Mach-O' || (echo "Launcher must be native Mach-O code"; exit 1)
	@file "$(PACKAGE_LAUNCHER_SCRIPT_PATH)" | rg -q 'shell script' || (echo "Launcher resource must be a shell script"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/line-drawing-bin" || (echo "Missing app binary at $(PACKAGE_MACOS_DIR)/line-drawing-bin"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist at $(PACKAGE_CONTENTS_DIR)/Info.plist"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_BUNDLE_ID)" || (echo "Bundle identifier mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_DISPLAY_NAME)" || (echo "Bundle display name mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleShortVersionString' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(RELEASE_VERSION)" || (echo "Bundle version mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :LineDrawingPackageProfile' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_PROFILE)" || (echo "Package profile mismatch"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns at $(PACKAGE_BUNDLED_ICON_PATH)"; exit 1); \
	fi
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan at $(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK at $(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/layout_config.json" || (echo "Missing config/layout_config.json at $(PACKAGE_RESOURCES_DIR)/config/layout_config.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/include/fonts/Lato/Lato-Regular.ttf" || (echo "Missing bundled local font at $(PACKAGE_RESOURCES_DIR)/include/fonts/Lato/Lato-Regular.ttf"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing bundled shared font at $(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/Montserrat-Regular.ttf"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime lane at $(PACKAGE_RESOURCES_DIR)/data/runtime"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/snapshots" || (echo "Missing snapshots lane at $(PACKAGE_RESOURCES_DIR)/data/snapshots"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/export" || (echo "Missing export lane at $(PACKAGE_RESOURCES_DIR)/export"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk renderer shader at $(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader at $(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-print-config: package-desktop
	@"$(PACKAGE_MACOS_DIR)/line-drawing-launcher" --print-config

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/line-drawing-launcher" --self-test || { \
		status=$$?; \
		echo "package-desktop self-test failed; launcher config follows:"; \
		"$(PACKAGE_MACOS_DIR)/line-drawing-launcher" --print-config || true; \
		exit $$status; \
	}
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
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
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

package-desktop-main-edit:
	@test -f "$(MEW1_TOOL)" || (echo "Missing shared MEW1 helper: $(MEW1_TOOL)"; exit 1)
	@before="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	$(MAKE) package-desktop-smoke \
		DIST_DIR="$(MAIN_EDIT_DIST_DIR)" \
		PACKAGE_APP_NAME="$(MAIN_EDIT_APP_NAME)" \
		PACKAGE_DISPLAY_NAME="$(MAIN_EDIT_DISPLAY_NAME)" \
		PACKAGE_BUNDLE_ID="$(MAIN_EDIT_BUNDLE_ID)" \
		PACKAGE_PROFILE="$(MAIN_EDIT_PROFILE)" \
		PACKAGE_RUNTIME_NAMESPACE="$(MAIN_EDIT_RUNTIME_NAMESPACE)" \
		PACKAGE_LOG_NAMESPACE="$(MAIN_EDIT_LOG_NAMESPACE)" \
		PACKAGE_BUILD_LABEL="$(MAIN_EDIT_BUILD_LABEL)" \
		PACKAGE_EMBED_BUILD_IDENTITY=1 || exit 1; \
	after="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	if [ "$$before" != "$$after" ]; then \
		rm -rf "$(MAIN_EDIT_APP_DIR)"; \
		echo "Source changed during Main Edit packaging; discarded generated package."; \
		exit 1; \
	fi
	@echo "Main Edit desktop package ready: $(MAIN_EDIT_APP_DIR)"

package-desktop-main-edit-self-test: package-desktop-main-edit
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_BUNDLE_ID)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_DISPLAY_NAME)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :LineDrawingPackageProfile' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_PROFILE)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :LineDrawingRuntimeNamespace' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_RUNTIME_NAMESPACE)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :LineDrawingLogNamespace' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_LOG_NAMESPACE)"
	@test -f "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json"
	@python3 "$(MEW1_TOOL)" verify-identity \
		--identity "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json" \
		--source-root "$(CURDIR)" \
		--binary "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/line-drawing-bin" \
		--profile "$(MAIN_EDIT_PROFILE)" \
		--program line_drawing \
		--product sCulpt \
		--version "$(RELEASE_VERSION)"
	@set -e; \
	fake_home="$(CURDIR)/$(MAIN_EDIT_SELF_TEST_DIR)/home"; \
	rm -rf "$$fake_home"; \
	mkdir -p "$$fake_home"; \
	HOME="$$fake_home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/line-drawing-launcher" --self-test; \
	config="$$(HOME="$$fake_home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/line-drawing-launcher" --print-config)"; \
	printf '%s\n' "$$config"; \
	printf '%s\n' "$$config" | grep -Fqx "LINE_DRAWING_PACKAGE_PROFILE=$(MAIN_EDIT_PROFILE)"; \
	printf '%s\n' "$$config" | grep -Fqx "LINE_DRAWING_RUNTIME_NAMESPACE=$(MAIN_EDIT_RUNTIME_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "LINE_DRAWING_LOG_NAMESPACE=$(MAIN_EDIT_LOG_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "LINE_DRAWING_BUILD_LABEL=$(MAIN_EDIT_BUILD_LABEL)"; \
	printf '%s\n' "$$config" | grep -Fqx "LINE_DRAWING_RUNTIME_DIR=$$fake_home/Library/Application Support/$(MAIN_EDIT_RUNTIME_NAMESPACE)/runtime"; \
	printf '%s\n' "$$config" | grep -Fqx "LOG_FILE=$$fake_home/Library/Logs/$(MAIN_EDIT_LOG_NAMESPACE)/launcher.log"
	@codesign --verify --deep --strict "$(MAIN_EDIT_APP_DIR)"
	@echo "package-desktop-main-edit-self-test passed."

package-desktop-main-edit-refresh: package-desktop-main-edit-self-test
	@test "$(MAIN_EDIT_DESKTOP_APP_DIR)" != "$(DESKTOP_APP_DIR)" || (echo "Refusing canonical Desktop destination"; exit 1)
	@mkdir -p "$(dir $(MAIN_EDIT_PROCESS_RECEIPT))"
	@python3 "$(MEW1_TOOL)" process-audit --match "$(MAIN_EDIT_DISPLAY_NAME)" --path "$(MAIN_EDIT_DESKTOP_APP_DIR)" > "$(MAIN_EDIT_PROCESS_RECEIPT)"
	@if grep -Fq '"running": true' "$(MAIN_EDIT_PROCESS_RECEIPT)"; then \
		echo "Refusing to replace a running $(MAIN_EDIT_APP_NAME); process receipt: $(MAIN_EDIT_PROCESS_RECEIPT)"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(MAIN_EDIT_DESKTOP_APP_DIR)")"
	@rm -rf "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(MAIN_EDIT_APP_DIR)" "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@echo "Refreshed $(MAIN_EDIT_APP_NAME) at $(MAIN_EDIT_DESKTOP_APP_DIR)"

main-edit-package-contract-checks:
	@./tests/run_main_edit_package_contract_checks.sh
