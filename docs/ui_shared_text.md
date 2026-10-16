# Shared filter and editor dialog text

Main Edit uses committed kit_ui 0.18.0 text editing/presentation/native-anchor
helpers. No vendored/shared API, source or version changes are made by this slice.
`UI/shared_text` adapts logical SDL events and measured text to the existing
Vulkan text/rectangle painter. Platform clipboard/IME helpers compile with real
SDL outside the drawing facade. App painting preserves the enclosing clip and
renderer color; text rasterization is synchronous and row storage stays valid
through each draw. Labels retain host word-boundary wrapping while shared
presentation handles scalar boundaries, long words, measured rows and clipping.

`UI/panel/ui_panel_shared_text` binds the currently active dialog's existing
buffer/cursor/length. Save names, session roots, prism dimensions, scene bounds,
construction-plane offset, object transforms and scene-property metadata retain
existing character policies and validation/commit callbacks. Shared selection,
UTF-8 movement/deletion, Home/End, clipboard commands and preedit presentation are
adapted. Enter/Escape intent remains with the app; repeat Enter cannot submit,
and Escape first cancels active preedit. Paste during preedit is swallowed.
Rejected over-capacity input leaves the original bytes intact. Numeric invalid
unit expressions remain available to the existing validator rather than changing
an admitted value. Shared editing does not grant document/history/save ownership.

Native window interruption cancels pointer drag and composition while preserving
committed draft bytes and selection. New dialog sessions reset only adapter
ownership. Pointer hit-testing/selection uses the measured logical field view;
long fields reveal the current caret. SDL candidate anchoring uses shared native
mapping. `Menu/line_drawing_host_menu_text` applies the same adapter to the
launcher filter; filtering and navigation stay with the existing catalog owner.

Summary/overlay text uses measured wrapping/clipping. The footer reserves120
logical pixels for all four existing text rows. Group header height is font height
plus6 pixels so headings do not overlap the first control. Existing rectangles,
canvas gestures, scene/camera/history and save/draft actions retain their owners.

Contained verification: fresh source/build inputs; focused `make test
ARGS=SharedText`; direct input/theme checks; full `make test`; static package
contracts; generic kit_ui tests; agent export smoke; validated native Vulkan
startup/resize/restart; menu/editor eight-stage window lifecycle; and visual
artifact modes `shared-text-scroll`, `shared-text-preedit`, `shared-text-filter`.
Native readback asserts active measured rows, contained caret, positive scrolling
and isolated composition bytes. 622 cases/67 groups and all15 final gates pass.
Native images were inspected. This is source/runtime proof; manual OS clipboard,
native language candidate behavior, complete font coverage and installed-package
acceptance are separate proof layers. Bundled font glyph coverage is unchanged.

Specialized camera/measurement/parts/routes/visibility inline fields, browser/list
controls and runtime HUD remain later slices. The seven dialog owners and the
launcher filter are the bounded completion target here.
