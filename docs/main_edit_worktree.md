# LineDrawing Persistent Main Edit Worktree

Last updated: 2026-08-28

## Lane identities

- Canonical source: `<workspace>/line_drawing` on `main`
- Persistent Main Edit source: `<workspace>/_worktrees/line_drawing_main_edit`
  on `codex/line-drawing-main-edit`
- Canonical package: `sCulpt.app`, bundle `com.cosm.sculpt`
- Main Edit package: `sCulpt Main Edit.app`, bundle
  `com.cosm.sculpt.main-edit`
- Canonical runtime/log namespace: `LineDrawing`
- Main Edit runtime/log namespace: `LineDrawing-Main-Edit`

The Main Edit application is a local development artifact. It is not a release
candidate, Registry record, publication, deployment, or version decision.

## Start gate

Before editing, read both lanes and the complete worktree inventory:

```sh
git -C <workspace>/line_drawing status --short --branch
git -C <workspace>/_worktrees/line_drawing_main_edit status --short --branch
git -C <workspace>/line_drawing worktree list --porcelain
git -C <workspace>/line_drawing rev-list --left-right --count \
  main...codex/line-drawing-main-edit
```

Only one writer may own the Main Edit checkout. Existing specialist, release,
repair, or retained-evidence worktrees remain separate and must not be reset,
cleaned, repurposed, or removed to simplify the Main Edit topology.

## Checkpoint gate

Run the narrowest behavior tests first, followed by the broad source and
package gates appropriate to the change:

```sh
git -C <workspace>/_worktrees/line_drawing_main_edit diff --check
make -C <workspace>/_worktrees/line_drawing_main_edit test
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit-self-test
```

The package self-test verifies the generic local-development identity,
packaged binary digest, `main-edit` profile, isolated runtime/log namespaces,
and the distinct app and bundle identifiers. Commit only the bounded owned
change set and only after explicit operator permission.

## Package targets

```sh
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit-self-test
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit-refresh
```

The refresh target is a separate Desktop mutation. It refuses the canonical
Desktop destination and refuses to replace a running Main Edit application.
Building or self-testing the package does not authorize refresh or launch.

## Integration gate

Before canonical adoption:

1. classify any canonical-only commits
2. merge expected canonical drift into Main Edit
3. rerun focused, full source, and Main Edit package gates
4. adopt with a fast-forward when canonical is an ancestor, otherwise use a
   reviewed merge
5. independently read back both commits and cleanliness

Source adoption does not authorize a `VERSION` edit, release package, signing,
notarization, Registry mutation, publication, deployment, or push.

## Retain and recycle gate

Retain the clean persistent Main Edit lane after adoption by default. Recycle
only after proving that it is clean, all commits are reachable, no user-owned
untracked or ignored evidence needs retention, no process owns the checkout or
development application, and all specialist worktrees remain unaffected.
Never force-remove or destructively reset the lane.

## Main Edit icon packaging

The existing product icon is retained at
`tools/packaging/macos/local_app_icon/AppIcon.icns`; this file is deliberately
excluded from the local-icon ignore rule so future source checkpoints and
worktrees retain it. Main Edit packaging requires an icon input. Its self-test
requires the bundled icon, the matching `CFBundleIconFile`, and byte equality
with the selected `.icns` input. Missing icons must fail rather than silently
produce a generic Desktop icon. Other local icon experiments remain ignored.

## Shared UI preparation — 2026-10-15

Use [contained preparation](ui_preparation_parity.md) before shared UI migration.
The baseline source/import identity and native Vulkan proof are separate from
canonical adoption and installed-package acceptance. Preserve the existing dirty
camera/mesh/scene work, specialized worktrees and protected layouts. Preparation
adds only verifier/documentation; it does not commit or merge paused work.

## Shared window foundation — 2026-10-15

Run `prepare_shared_ui.py --lifecycle` with fresh external roots as described in
[foundation](ui_window_foundation.md). Committed UI/render prefixes are imported
without adopting dirty shared depth work. Keep the import-only change set separate
from app integration when checkpointing later; this slice does not commit paused
functional work or refresh Desktop. Existing scene, history, draft and package
identities remain protected.

## Shared shell controls — 2026-10-15

The bounded launcher/topbar/pane integration is qualified in retained Main Edit;
see [control contract](ui_shared_controls.md). It stays uncommitted under the
checkpoint gate above and does not refresh Desktop. Shared text/editor fields and
specialized panels/HUD remain next. Preserve paused functional source and data.

## Shared text and dialog fields — 2026-10-15

The launcher filter and seven existing editor dialog owners are qualified through
shared UTF-8 editing/presentation; see [text contract](ui_shared_text.md). Pane and
footer containment are refined selectively. Preserve paused source and saved data;
this checkpoint stays uncommitted and does not adopt canonical or refresh Desktop.
Specialized inline fields, lists/panels and HUD remain next.

## Specialized controls and inline text — 2026-10-15

The bounded specialized panel/list and remaining inline-field integration is
qualified in retained Main Edit; see [contract](ui_shared_specialized.md).
Existing scene/canvas/camera/history/save/draft ownership and rectangular chrome
remain. No commit, canonical adoption or Desktop refresh accompanies this slice.
Runtime HUD/overlays and isolated package/installed visual acceptance remain next.

## Runtime HUD / overlay checkpoint — 2026-10-15

See retained Main Edit `docs/ui_shared_hud.md`: shared release/focus, bounded
text, viewport notice/HUD containment and authoring preview/cancel are qualified.
633 cases / 69 groups and 26 final source/runtime gates pass. Source remains
uncommitted; isolated package and installed visual acceptance remain next.

## UI package acceptance — 2026-10-15

Package identity and packaged rendering pass; live restored-workspace exit blocks
in mesh-preview worker shutdown. See `docs/ui_package_acceptance.md` and evidence
`shared_ui_package_20261015`. Repair/reverify exit before adoption/Desktop refresh.

## Preview shutdown qualification — 2026-10-15

See Main Edit `docs/mesh_preview_shutdown.md`: bounded app-local input readers
retain existing shared workers and source validation. Four focused cases and
637 cases / 70 groups pass. Three native restored-workspace quit checks preserve
scene/history/draft state and finish shutdown in 14–16 ms. The previous acceptance
blocker is resolved in the private package; adoption/Desktop refresh remain next.
