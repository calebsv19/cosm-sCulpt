# Line Drawing Desktop Packaging

Last updated: 2026-04-01

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
- `line_drawing/dist/LineDrawing.app`

Desktop copy target:
- `/Users/<user>/Desktop/LineDrawing.app`

## Launcher Diagnostics
Launcher path:
- `/Users/<user>/Desktop/LineDrawing.app/Contents/MacOS/line-drawing-launcher`

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
- `VK_RENDERER_SHADER_ROOT=$RES_DIR`
- `SHAPE_ASSET_DIR=$RES_DIR/export`
- shared theme/font toggles enabled by default

## Recommended Local Validation
1. `make -C line_drawing clean && make -C line_drawing`
2. `make -C line_drawing test`
3. `make -C line_drawing package-desktop-self-test`
4. `make -C line_drawing package-desktop-refresh`
5. `/Users/<user>/Desktop/LineDrawing.app/Contents/MacOS/line-drawing-launcher --print-config`
6. `open /Users/<user>/Desktop/LineDrawing.app`
7. `tail -n 120 ~/Library/Logs/LineDrawing/launcher.log`
