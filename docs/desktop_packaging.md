# Line Drawing Desktop Packaging

Last updated: 2026-10-04

## Standard Targets
- `make -C line_drawing package-desktop`
- `make -C line_drawing package-desktop-smoke`
- `make -C line_drawing package-desktop-print-config`
- `make -C line_drawing package-desktop-self-test`
- `make -C line_drawing package-desktop-copy-desktop`
- `make -C line_drawing package-desktop-sync`
- `make -C line_drawing package-desktop-open`
- `make -C line_drawing package-desktop-remove`
- `make -C line_drawing package-desktop-refresh`

Persistent Main Edit targets:

- `make -C line_drawing package-desktop-main-edit`
- `make -C line_drawing package-desktop-main-edit-self-test`
- `make -C line_drawing package-desktop-main-edit-refresh`

The Main Edit output is
`line_drawing/build/dist/dev/main-edit/sCulpt Main Edit.app`. It uses bundle
identifier `com.cosm.sculpt.main-edit`, runtime/log namespace
`LineDrawing-Main-Edit`, and embeds `Resources/build_identity.json` using the
generic `codework_local_development_build_identity_v1` schema. The self-test
verifies the exact source fingerprint and packaged binary digest.

Main Edit refresh is an explicit Desktop mutation. It refuses the canonical
`sCulpt.app` destination and refuses replacement while the development app is
running. See `docs/main_edit_worktree.md` for the complete lane lifecycle.

Optional icon inputs:

```sh
make -C line_drawing package-desktop-refresh \
  PACKAGE_APP_ICONSET_SRC="/absolute/path/AppIcon.iconset"
```

or

```sh
make -C line_drawing package-desktop-refresh \
  PACKAGE_APP_ICON_SRC="/absolute/path/AppIcon.icns"
```

If either variable is supplied, packaging will bundle `Contents/Resources/AppIcon.icns` and the app plist will advertise `CFBundleIconFile=AppIcon`.

Default local icon store:
- `line_drawing/tools/packaging/macos/local_app_icon/AppIcon.icns`
- `line_drawing/tools/packaging/macos/local_app_icon/AppIcon.iconset`

Plain `make -C line_drawing package-desktop-refresh` and `package-desktop-self-test` now look in that local store first. The local icon store is gitignored so refreshed icon copies do not dirty the normal repo worktree.

Bundle output:
- `line_drawing/dist/sCulpt.app`

Desktop copy target:
- `/Users/<user>/Desktop/sCulpt.app`

## File access and editor startup

The host displays a frame before reopening its remembered document. Restore does
not scan the surrounding file-browser folder. Host catalog discovery happens when
you choose Layouts, Scenes, Recents or Browse; unread counts show an ellipsis.
Hover/render events do not rescan directories. Browse offers Choose input folder
and Choose output folder. A failed scan shows an error and retry guidance, while a
failed document open retains the current document and Undo history.

On macOS, folder selection uses an app-owned Cocoa panel and starts input selection
in the current input root. Cancel leaves roots unchanged. Linux uses existing
zenity/kdialog selection/fallback. Explicit filesystem reads/scans remain synchronous.
The app does not grant itself Full Disk Access or modify OS privacy settings;
folder approval remains under macOS control. This checkpoint verifies choosing the
existing app-private project folder, not lasting access to every protected folder.

## Launcher Diagnostics
Launcher path:
- `/Users/<user>/Desktop/sCulpt.app/Contents/MacOS/line-drawing-launcher`

The executable launcher is a small native Mach-O shim. It invokes the sealed
shell implementation at
`Contents/Resources/line-drawing-launcher.sh`. The shell resource must not be
signed as nested code: detached script signatures do not survive ZIP transport
without invalidating the enclosing app seal.

Diagnostics commands:
- `.../line-drawing-launcher --print-config`
- `.../line-drawing-launcher --self-test`
- `make -C line_drawing package-desktop-print-config`

Log lane:
- `~/Library/Logs/LineDrawing/launcher.log`
- Main Edit: `~/Library/Logs/LineDrawing-Main-Edit/launcher.log`
- tmp fallback: `${TMPDIR:-/tmp}/<namespace>/logs/launcher.log`

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
- Main Edit default:
  `LINE_DRAWING_RUNTIME_DIR=$HOME/Library/Application Support/LineDrawing-Main-Edit/runtime`
- `VK_RENDERER_SHADER_ROOT=$LINE_DRAWING_RUNTIME_DIR/vk_renderer`
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

Failure diagnostics:
- `package-desktop-smoke` prints the package app and resource roots before
  validation, and missing-resource failures include the expected package path
- launcher `--self-test` prints the resolved launcher config when a packaged
  runtime resource check fails
- `package-desktop-self-test` also prints `--print-config` output on launcher
  self-test failure, so failed make logs include the runtime dir, log file, ICD
  file, shader root, shape asset root, and bundled MoltenVK path

## Contained release preparation

`RELEASE_ROOT` binds both the application and every release ZIP, checksum,
manifest, audit and notary output. Release Control may select either a contained
`build/release-authenticated/<job-id>` source slot or an absolute slot beneath the
configured Registry data root's `line_drawing/build/release-authenticated/` lane.
The slot must be absent; symlink ancestors, ambiguous paths, other programs and
existing output slots are refused before packaging. Bound `rapt_<digest>` target
subslots are also supported. Unbound ordinary desktop builds retain `dist/`.

Both disposable input creation and authenticated release builds reserve their
own create-only slot. A failed slot remains evidence; continuation uses the
Release Control owner's exact retained attempt and disjoint destination rules.
Do not overwrite a failed slot or change `RELEASE_ROOT` to retry an uncertain job.

Package `--self-test`, package `--print-config`, bundle audits and final ZIP
round-trip checks use temporary runtime/log directories and clear inherited
shader/ICD/asset overrides. They do not initialize the user's Application Support
or Logs directories. Temporary diagnostic lanes are removed on exit. Invoking
the installed launcher directly retains its normal user-runtime defaults.

`make test-package-release` verifies positive local/data-root slot creation,
collision preservation, traversal/symlink rejection, Make output-root rebinding
and diagnostic cleanup without signing. It is included in `make test`; it does
not replace the real package, signature, notarization or archive round-trip gates.

## Release Readiness Targets
- `make -C line_drawing release-contract`
- `make -C line_drawing release-bundle-audit`
- `make -C line_drawing release-sign APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
- `make -C line_drawing release-notarize APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"`
- `make -C line_drawing release-staple`
- `make -C line_drawing release-verify-notarized APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
- `make -C line_drawing release-artifact-roundtrip-test`
- `make -C line_drawing release-distribute APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"`

`release-artifact` now extracts the final ZIP into a disposable directory and
requires deep codesign verification, native-launcher/resource identity,
launcher self-test, no materialized detached-signature files, Gatekeeper
acceptance, staple validation, and exact source-commit binding before writing
checksum/manifest sidecars.

## Recommended Local Validation
1. `make -C line_drawing clean && make -C line_drawing`
2. `make -C line_drawing test`
3. `make -C line_drawing package-desktop-self-test`
4. `make -C line_drawing release-artifact-roundtrip-test`
5. `make -C line_drawing package-desktop-refresh`
6. `/Users/<user>/Desktop/sCulpt.app/Contents/MacOS/line-drawing-launcher --print-config`
7. `open /Users/<user>/Desktop/sCulpt.app`
8. `tail -n 120 ~/Library/Logs/LineDrawing/launcher.log`

Note:
- a fresh clone will still need an `AppIcon.icns` copied into `tools/packaging/macos/local_app_icon/` before plain packaging picks it up, because that lane is intentionally ignored.

## Ordinary macOS release input

`make RELEASE_ROOT=build/release-authenticated/<job-id> release-artifact-disposable` produces a create-only pre-authentication sCulpt.app, ZIP, checksum and source-bound manifest. The job root and its ancestors must be contained and non-symlink; existing roots are rejected. The package self-test runs against that isolated app. Developer ID signing/notarization is a later approved stage. This target does not refresh Desktop or produce a Linux artifact; Linux packaging remains a separately selected release scope.

The release linkage audit checks indented `otool -L` dependency entries. The
absolute inspected-file headers are retained as diagnostics and do not count as
nonportable dependencies. Homebrew, user-local and unresolved `@rpath` dependency
entries still fail the audit, including for absolute Registry output roots.
