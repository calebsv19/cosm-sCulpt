# Line Drawing Desktop Packaging

Last updated: 2026-04-05

## Standard Targets
- `make -C line_drawing package-desktop`
- `make -C line_drawing package-desktop-smoke`
- `make -C line_drawing package-desktop-self-test`
- `make -C line_drawing package-desktop-copy-desktop`
- `make -C line_drawing package-desktop-sync`
- `make -C line_drawing package-desktop-open`
- `make -C line_drawing package-desktop-remove`
- `make -C line_drawing package-desktop-refresh`

Bundle output:
- `line_drawing/dist/sCulpt.app`

Desktop copy target:
- `/Users/<user>/Desktop/sCulpt.app`

## Launcher Diagnostics
Launcher path:
- `/Users/<user>/Desktop/sCulpt.app/Contents/MacOS/line-drawing-launcher`

Diagnostics commands:
- `.../line-drawing-launcher --print-config`
- `.../line-drawing-launcher --self-test`

Log lane:
- `~/Library/Logs/LineDrawing/launcher.log`
- tmp fallback: `${TMPDIR:-/tmp}/line-drawing-launcher.log`

## Packaged Resource Contract
The package bundles and validates:
- `Resources/config/`
- `Resources/include/fonts/`
- `Resources/shared/assets/fonts/`
- `Resources/data/runtime/`
- `Resources/data/snapshots/`
- `Resources/export/`
- `Resources/vk_renderer/shaders/`
- `Resources/shaders/`

Runtime env defaults set by launcher:
- `LINE_DRAWING_RUNTIME_DIR=$HOME/Library/Application Support/LineDrawing/runtime`
- `VK_RENDERER_SHADER_ROOT=$LINE_DRAWING_RUNTIME_DIR/data/runtime/vk_renderer`
- `SHAPE_ASSET_DIR=$LINE_DRAWING_RUNTIME_DIR/export`
- `VK_ICD_FILENAMES=$LINE_DRAWING_RUNTIME_DIR/vk/MoltenVK_icd.json`
- `VK_DRIVER_FILES=$LINE_DRAWING_RUNTIME_DIR/vk/MoltenVK_icd.json`
- `MOLTENVK_DYLIB=$APP_CONTENTS_DIR/Frameworks/libMoltenVK.dylib`
- shared theme/font toggles enabled by default

Bundled framework contract:
- app-local `Frameworks/` is required and must include Vulkan portability libs
  - `libMoltenVK.dylib`
  - `libvulkan.1.dylib`
- package step applies ad-hoc signing after install-name rewrites for local launch safety

## Release Readiness Targets
- `make -C line_drawing release-contract`
- `make -C line_drawing release-bundle-audit`
- `make -C line_drawing release-sign APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
- `make -C line_drawing release-notarize APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"`
- `make -C line_drawing release-staple`
- `make -C line_drawing release-verify-notarized APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
- `make -C line_drawing release-distribute APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"`

## Recommended Local Validation
1. `make -C line_drawing clean && make -C line_drawing`
2. `make -C line_drawing test`
3. `make -C line_drawing package-desktop-self-test`
4. `make -C line_drawing package-desktop-refresh`
5. `/Users/<user>/Desktop/sCulpt.app/Contents/MacOS/line-drawing-launcher --print-config`
6. `open /Users/<user>/Desktop/sCulpt.app`
7. `tail -n 120 ~/Library/Logs/LineDrawing/launcher.log`
