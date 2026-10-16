# Shared runtime HUD and overlay contract

Applies to retained LineDrawing Main Edit; no shared API or vendored source change.
Imports remain kit_ui 0.18.0, kit_render 0.15.0 and vk_renderer 1.7.0.

`shared_overlay_controls.c` adapts active Workspace Authoring controls to the
existing shared surface. Font/theme layout uses current font/zoom metrics for
both rendering and input. Existing owner actions retain preview, Apply/Cancel,
accepted-only preference persistence and disabled placeholder behavior. Arrow
focus and Space activation coexist with owner Tab/Enter/Escape shortcuts.

The existing editor surface registers Object viewport HUD buttons with object
identity and placement. Release dispatch retains face targeting, sketch target
synchronization and hover refresh. Continuous canvas/camera gestures remain local.
Modal/authoring/pop-up ownership blocks HUD actions; resize/focus cancellation
clears capture without clearing drafts. HUD geometry fits its actual viewport,
reserves notice space and hides if no usable arrangement fits.

`shared_overlay_notice.c` bounds read-only geometry diagnostics inside the viewport.
Shared text clips dialog captions, annotation labels and authoring text. The
inactive legacy Info renderer is outside this active render path.

Verification uses a fresh task-owned external copy, never retained build output.
Run build, input/theme/text/special controls, `make test ARGS=SharedHud`, complete
unit suite, kit_ui contracts, package contracts and agent-scene smoke. Native
artifact modes `shared-hud-object`, `shared-hud-notice`, `shared-hud-modal`,
`shared-hud-authoring`, `shared-hud-authoring-zoom` assert unchanged document/history
and expose actual Vulkan frames. Run strict Vulkan rollout plus menu/editor window
lifecycle and previous text/panel modes. These qualify source/runtime behavior;
they do not prove installed package identity or manual OS clipboard/IME acceptance.

## 2026-10-15 — Runtime HUD and authoring overlays qualified

Retained Main Edit now routes Object viewport face buttons and active Workspace
Authoring controls through existing kit_ui capture, release activation and focus.
Current object identity, layout, enable rules and modal ownership cancel stale
releases. Authoring keeps Tab mode switching, Enter Apply, Escape Cancel and local
pane gestures; arrows focus and Space activates shared controls. Window capture
cancellation preserves drafts. Font/theme painting, hit testing and registration
share one layout using the current shared font preset and text zoom.

HUD controls retain rectangular geometry and reflow or hide when the viewport
cannot contain them. Geometry notices wrap within the viewport below the HUD;
modal overlays occlude HUD controls. Dialog captions and visible overlay labels
use bounded shared text. Native review corrected selected font/theme label
contrast. Inactive legacy Info rendering has no caller and was left untouched.

Build, four focused HUD cases, all 633 cases in 69 groups, shared UI contracts,
package contracts and export smoke pass. All 26 final gates pass, including strict
Vulkan startup/resize/restart, both eight-stage window paths, prior text/panel
readbacks and five native HUD/notice/modal/font-theme modes (100% and 140% text).
Private fixture checks preserve serialized document and history. Nine protected
file sets, 15 directory links and 3347 retained build files match the prior slice.
Authoritative evidence: `_private_workspace_artifacts/line_drawing/shared_ui_hud_20261015/final-proof2/`.

This is an uncommitted Main Edit source/runtime checkpoint. Existing paused scene,
canvas, camera, history, saves and drafts remain owned by their current systems.
Canonical source, installed apps, shared imports and VERSION remain unchanged.
Next: isolated package identity and installed-app visual acceptance, followed by
reviewed source adoption and session-safe Main Edit refresh. Native clipboard/IME
candidate-window and broad glyph coverage remain separate hands-on acceptance.
