# Shared window foundation

The retained Main Edit lane imports the committed kit_ui 0.18.0,
kit_render 0.15.0 and vk_renderer 1.7.0 source sets at shared commit
23c94f30ecd22733a96c8ab7476755c407cd31f7. Only those module prefixes were applied
through the managed archive synchronizer. Source hashes are recorded in the
foundation receipt. The dirty workspace renderer 1.8.0 depth work is excluded;
vk_runtime 0.6.0 and the existing theme/font and domain modules retain their owners.
No shared APIs or versions were changed by this app integration.

## Runtime ownership

`src/Core/SDLApp/sdl_app_window.c` adapts shared native window observation,
logical coordinate mapping, F11 fullscreen and the opt-in lifecycle probe.
The SDL event/runtime path refreshes geometry and cancels obsolete capture.
Vulkan presentation, swapchain resources and framebuffer lifetime remain owned
by the existing app framework/adapter. Pending recreation is scheduled only when
observed drawable extent differs from the resource owner's current extent;
stale resize-event payloads do not override queried native geometry. Native SDL
observer/probe units compile outside the SDL-shaped Vulkan drawing facade.
The required kit_render command-validation source is linked explicitly.

`src/Input/input_window_capture.c` ends navigation, gizmo, anchor, selection,
splitter, scrollbar and timeline capture through existing owners. It retains the
last admitted edits and does not clear documents, save names, authoring fields,
measurements, history, camera settings or background mesh jobs. Stale releases
and held key repeats are suppressed until release/fresh press. Text input is
suspended with the native window and resumed without committing a draft.
Pointer coordinates stay logical; Retina drawable scale is applied by Vulkan.
Outside capture coordinates remain outside. Hidden/minimized windows do not
present frames. Normal runtime remains demand-driven; proof redraws are opt-in.

## Verification

Use the [contained preparation route](ui_preparation_parity.md), adding
`--lifecycle`. It creates separate private menu/editor runtimes and runs both
native eight-stage loops, validates six captures per loop, and compares serialized
document bytes, undo/redo counts and saved-state flags before/after. Focused tests
preserve save/measurement drafts and camera state on capture cancellation and
verify logical/Retina/outside mapping. Fresh build, 613 tests in 65 groups,
shared UI module contracts, agent exports and original strict Vulkan
startup/resize/restart pass. Validation is enabled and reports zero warnings and
errors. Menu/editor/initial/resized reference RGB pixels are exactly unchanged.
Fullscreen/restored frames are inspected. This is source/runtime qualification;
installed-package identity and human workflow acceptance remain later boundaries.

## Remaining rollout

Launcher/reopen/catalog, menus, topbar and pane controls are next. Text fields,
specialized lists/panels and HUD integration follow. Existing sidebar/footer
containment findings stay recorded; this slice does not round or restyle controls.
Canonical remains clean and unchanged, Main Edit keeps the existing dirty scene
work and branch divergence, and Desktop apps/saves/drafts are untouched. No source
adoption, commit, VERSION change, release or Desktop refresh is implied.
