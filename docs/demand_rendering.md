# Demand rendering and scene-list reuse

Implemented 2026-10-05 against baseline 23c0dbf03f70018bfd1212e738537ccac497becd.

## Behavior contract

The editor redraws when input or an explicit update invalidates the view. A settled
window blocks in SDL event waiting, interrupted immediately by input; it does not
redraw on the maintenance timeout. The default maintenance interval is 1000 ms.
This interval is not input latency: queued SDL events wake the wait immediately.

`AppContext.redraw_requested` is the update-side invalidation signal.
`AppCallbacks.nextUpdateDelayMs` returns -1 when settled, 0 when due, or a positive
number of milliseconds until a visible timed update. New asynchronous/timed UI
features must either enqueue an SDL event, request redraw from an update, or
contribute a deadline. They must not depend on an idle redraw heartbeat.

| State | Scheduling |
| --- | --- |
| Pointer, keyboard, window, file-drop, user event | Wake immediately; drain pending input; update and redraw |
| Saved-path playback | 16 ms ticks while playing |
| Active STL import | 50 ms progress/completion polls |
| Interactive solid/wire/material preview | One settled-quality update after the 150 ms quality interval |
| Load completion or file-action status | Redraw at the actual expiry deadline |
| Settled | Up to 1000 ms blocked maintenance waits; no forced frame |
| Hidden/minimized | Retain dirty state, block, do not present; restore event redraws |
| Presentation failure | Retain dirty state, retry after 50 ms by default rather than spin |

`LINE_DRAWING_LOOP_MAX_WAIT_MS` can shorten waits for diagnostics. The existing
always-render mode remains available. Delta time for the first update after a
settled wait is bounded, so starting playback cannot consume the preceding idle
interval. Playback while minimized continues on the maintenance updates; this
pass does not change path playback into a pause-on-minimize policy.

## Reused presentation work

The scene list caches ordered visible row indices and total content height.
Unchanged content reuses these for row counts, scroll limits, scrollbar geometry,
selection, and rendering. It no longer recursively recounts the full visible
scene inside every row and scrollbar lookup. Stored indices resolve against the
current layout; cached object pointers are not retained across geometry
replacement, load, or undo.

Preparation runs at relevant list input/render boundaries. A content fingerprint
covers membership/visibility metadata, assembly ancestry, routing electrical
query fields, saved views, active visibility choices, row expansion and font
height. This catches direct metadata changes as well as transaction-driven
changes. Pointer movement outside the list skips list preparation. Shutdown
frees the cache. This is a presentation cache, not an authoritative scene model.
The fingerprint is still linear in the scene metadata on relevant operations;
there is no claim of constant-time whole-scene invalidation for arbitrarily large
models. The visible rows and content heights only rebuild when that fingerprint
changes.

Saved-view matching resolves and validates entity metadata once per visibility
check, then reuses it for the view queries. The public query entry point retains
validation for untrusted inputs. Overlapping-view, unclassified-entity and
isolation behavior is preserved. Unchanged window dimensions also no longer
trigger pane relayout and picking invalidation on every presented frame.

Pane-divider and font metric changes explicitly notify child controls through
`Global_RefreshPaneLayout`; they do not rely on an idle relayout heartbeat.
See [responsive editor panes](responsive_panes.md).

## Evidence and practical limits

The installed Main Edit app was measured with the user's saved 130-object
`van_connected_sections_s5.layout.json` scene. Before this pass, a 12-second
observation accumulated about 10.2 seconds of CPU time (about 85% of one core),
and the scene-list render stack accounted for about 86% of main-thread samples.

After this pass, the restored full scene settled at 0.0% in quiet samples;
furniture-only solid viewing and an independently confirmed minimized window
each held the same accumulated CPU time across four samples spaced 3 seconds
apart. A 3-second sampling profile showed the main thread waiting in
`SDL_WaitEventTimeout`. Loop diagnostics reported 100% blocked intervals with
0 ms active time after settling. The diagnostic `frames` field counts loop
iterations, not presented frames; RS1 records track actual render submissions.

Native controls verified: saved-view filtering, selecting the bed and discovering
its existing `bed_lift` motion, Max moving it from 950 to 1650 mm, live slider
movement to an intermediate position, Undo restoring the bed and attached parts
to 950 mm and Saved state, orbit, solid viewing, slice-slider contents, minimize
and restore. Test movements were undone; the saved van file hash remained
unchanged. Active interaction/rasterization still uses CPU, as expected. These
checks demonstrate wake/sleep behavior and control operation; they are not an
input-to-photon latency benchmark or a sustained frame-rate guarantee.

Validation: warning-clean `make -j4`, all 525 host unit cases, native folder-picker
tests and `make agent-scene-smoke`. New regressions cover idle/input/deadline/
suspension/retry scheduling, unchanged scene-list cache reuse and visibility
invalidation, and unchanged-size picking stability. Existing geometry, undo,
saved-view, section and preview-quality suites pass.

At the performance checkpoint, `make illustrative-van-smoke` failed its exact
fixture-byte assertion. The
pre-change canonical generator and candidate generator produce identical bytes;
both differ from the checked-in illustrative fixture in `file` and `engineering`.
The fixture/source mismatch was pre-existing and retained separately from this
performance change. During bounded release preparation on 2026-10-06, the fixture
was regenerated at schema 21. Only schema metadata and empty route/view fields
changed; geometry, constraints and existing engineering records are unchanged.
Fresh `make illustrative-van-smoke` now passes.

Machine-readable measurements are in
[evidence/demand_rendering_2026-10-05.json](evidence/demand_rendering_2026-10-05.json).
The local Main Edit package self-test verifies package identity and binary digest.
This is a local development update; no VERSION, release or shared-module version
changes are required. Existing `core_time` and SDL event waiting are reused.
