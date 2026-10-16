# Specialized shared controls and inline text

This contract applies to retained LineDrawing Main Edit, not canonical adoption
or installed-package acceptance. Existing kit_ui 0.18.0, kit_render 0.15.0 and
vk_renderer 1.7.0 imports remain unchanged; no shared API/version change.

## Controls and ownership

`src/UI/shared_special_controls.c` extends the existing single editor surface.
Panel owners expose a no-paint geometry pass; Parts/spatial, Routes, measurement
and collection-inspection cells register their current rectangles, enable rules
and clips. Camera/path/section/furniture expose their existing action geometry.
Generic buttons reuse the renderer's enable calculation. Scene/model lists and
file/STL/visibility pickers provide current visible rows and stable entity/path
identities. Registration is bounded by the shared surface capacity and fails
closed when a snapshot is invalid.

Release activation dispatches once to the existing owner. Control placement is
part of identity, so two visible copies of one command remain distinct and moved
controls cancel a held release. Changed entity identities/order also cancel stale
rows. Disabled controls block pointer activation and skip keyboard focus. Popup
snapshots occlude controls beneath them; the file browser only owns its pane.

Canvas gestures, camera movement, section/path/travel scrubbers, splitter and
scrollbar drags remain continuous local owners. They do not become release-only
buttons. Direct owner calls remain available for existing programmatic behavior.

## Inline drafts

`src/UI/shared_inline_text.c` binds the active owner's buffer through the same
`LDTextField` adapter used by dialogs. Small descriptors expose capacity, replace
selection, current value rectangle and optional furniture edit flag. Active
buffers cover camera/path scalars, Section, furniture/contact/sink dimensions,
Parts/spatial/query fields, Routes/electrical fields, measurement/travel/grid,
collection inspection names and visibility search/names.

Shared editing owns selection, scalar-safe movement/deletion, clipboard commands,
preedit isolation, repeat submission guards, measured caret reveal and pointer
selection. Existing parsing, errors, history, save/apply/cancel behavior and draft
bytes stay with the owner. Measurement expressions retain their ASCII input
policy. Window capture cancellation clears capture/composition without clearing
the draft. Furniture Tab stays with its local field cycle, after preedit ends.
Popup Escape precedes a hidden editor field. Hidden furniture descriptors do not
reset drafts, and active furniture descriptors observe existing selection refresh.

Rendering preserves existing rectangular chrome and labels. The active Section
field names its value in the existing heading, giving the value the full existing
rectangle. Text paints synchronously and preserves renderer clip/color. Logical
coordinates stay independent of Retina/Vulkan output pixels.

## Verification

Use `tools/verification/prepare_shared_ui.py --lifecycle` with fresh external work
and report roots; never build in the retained source root for this lane. Run build,
targeted `make test ARGS=SharedSpecial` plus input/theme/text checks, then the full
suite, shared kit_ui tests, package contracts and agent scene/export smoke.

Native artifact modes `shared-special-parts`, `shared-special-routes`,
`shared-special-visibility` and `shared-special-section` stage private drafts and
assert caret containment/composition with unchanged history. Section also asserts
zero horizontal scroll for 1250 mm. These are source/runtime readbacks, not a
manual OS clipboard/IME candidate or installed-app acceptance claim.

The 2026-10-15 checkpoint passes629 cases/68 groups, seven focused specialized
cases, all20 final gates and native visual inspection. Initial menu/editor images
are pixel-identical to the prior text checkpoint. Private evidence lives under
`_private_workspace_artifacts/line_drawing/shared_ui_special_20261015/`, with
`final-proof5/` authoritative. Earlier compact failure logs are superseded by that
proof. Nine protected roots,15 directory links and3347 retained build files match.

Runtime HUD/overlays remain next. Isolated packaging, canonical source adoption,
commit permission and Desktop refresh remain separately bounded by the Main Edit
runbook. A release-impact dry-run recommends minor for the user-visible workflow;
no VERSION or release action is part of this source slice.
