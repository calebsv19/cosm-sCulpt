# Motion envelopes and pose inspection — S4

Status: saved Travel/Hinge envelopes cover B alone or an explicit rigid assembly.
Checks can inspect sampled obstruction poses in a read-only native preview.
Independent joints and exact swept-solid collision remain outside this contract.

## Mouse workflow

1. In **Measure**, choose or create a saved **Travel** or **Hinge** rule. Apply any
   staged Min/Max/Position edits first.
2. Use **Moves** below Min/Max/Reset to choose **B only** or a containing assembly.
   Scope is saved immediately in one undoable command. The slider and numeric
   Position then move the selected group, including nested assembly frames.
3. Click **Create envelope** below the movement slider and Min/Max/Reset controls.
   The editor opens **Parts → Volumes** and selects the generated wire box.
4. The pane shows **current / STALE**, the moving part, the saved range and the
   box size. **Details** reveals the movement ID, pose count and padding.
5. Click **Run checks** to open **Parts → Checks**. Select a named result, then
   **Select A / Select B** to locate the envelope or obstruction.
6. Use **Edit movement** to return to Measure with the moving B object selected.
   The existing slider, Min/Max/Reset and numeric Position control inspect motion.
7. After changing the range, driver/reference geometry, scope or member geometry, click
   **Regenerate envelope**. It retains the envelope's entity ID. Measure displays
   **Update envelope** for a stale saved envelope and **View envelope** otherwise.
8. **New volume** still opens the ordinary Keep-out/Service form. Envelope
   dimensions are derived, so its pane does not expose manual size/owner fields.
   **Delete…** requires a visible confirmation; deletion participates in undo.

Creation samples the **saved** movement, never unsaved form text. Sampling does
not move the live document, add pose objects, or change the current Position.
The envelope is a persistent world-aligned prism labeled `Motion: <B name>` and
semantically typed `MotionEnvelope`. Its normal wire color is purple; existing
selection/hover colors still apply. Viewport labels explicitly say
`conservative bounds` or `STALE - regenerate`, and obey visibility/filter/clipping.

## Physical coverage and scope

The UI uses 33 evenly spaced poses, including both limits. Generation samples the
existing solver in an isolated object-store copy, so saved pivot offsets, world
axes, locks and constraint behavior are shared with the movement controls.

A world AABB encloses all sampled corners. Linear translation covers intermediate
poses through the union bounds of the endpoint poses. For a hinge, padding on every
box axis is at least `r * deltaAngle / 2`, with deltaAngle in radians and r the
largest sampled corner-to-pivot radius. Every pose is within half a sample interval
of a sample; arc length bounds the intervening displacement. Additional padding
covers physical/numerical tolerance and float geometry storage. A panel receives
positive box thickness, respecting the existing primitive minimum.

These boxes intentionally overestimate the occupied region, especially for doors.
Automatic obstruction checks produce **warnings**, `approximate=true`, and a
possible-obstruction message. Box overlap does not prove a physical collision.
Explicit no-intersection/clearance rules involving an envelope also report bounds
results as warnings; a clear bounds gap satisfies the conservative check only.

Automatic envelope checks exempt the complete saved moving group and fixed A
reference/mounting object, other reserved volumes and Reference geometry. Hidden design parts still
participate. Explicit rules can check the exempt entities if desired.

## Explicit assembly scope

The default remains B only; parenting alone never changes a movement's scope.
A saved Travel/Hinge may explicitly select a containing assembly. That assembly
and all nested members receive the rigid delta solved for B. Their existing IDs,
relative arrangement and assembly frames are retained. The same path serves
slider/numeric movement, accepted direct B edits and isolated envelope sampling.

This bounded slice supports at most 64 design panels/prisms per moving assembly.
A must remain outside the group. Reference/reserved/mesh members and other
geometric constraints mentioning any group member are refused explicitly. This
prevents an independent rule from distorting a rigid group. Locks, construction
plane/bounds conflicts and history failures roll back the entire command. Member
placement can still be edited independently to change the group's arrangement;
that stales the envelope. Reparenting B out of its saved scope is refused until
scope changes; adding/removing/reparenting other members stales coverage.

Scope does not infer attachment links or electrical relationships. Downstream
followers in the original B-only constraint forest remain outside its envelope.
No imported-mesh motion, independent simultaneous joints, pose animation, exact
swept union, penetration depth or structural acceptance is provided here.

## Inspect an obstruction without moving the scene

In Parts → Checks, select a current envelope result and click **Inspect motion**.
For a primitive obstacle this tests the saved sample positions against oriented
primitive geometry, including a saved rule's requested clearance. It opens at the
first sampled failure, or the closest sampled pose when no sample fails.

The focused form names the obstacle, first failure position/member when found,
and the current preview position. **Previous / Next** step through sampled poses;
**Close preview** restores result details. Orange wires and an explicit read-only
viewport label distinguish the preview from the authored objects. The source pose,
movement target, scene file and undo count are unchanged. New checks, selecting a
different result or authored-input drift clear the preview; other tabs hide it.
Visibility and viewport clipping apply to the cached ghost geometry.

A result with no sampled failure does not certify clearance: contact between
samples is untested. The original conservative warning remains. Stale envelopes, meshes,
moving targets in the same scope, or lock/bounds/solver failures cannot be inspected
through this primitive sampling path and receive an explicit unavailable message.

## Freshness and transactions

Stored input fingerprints cover movement references/offsets, axis, range/home,
captured rigid basis, resolved A driver point/direction, B primitive dimensions and
physical world scale. Current target/B pose is excluded so ordinary travel within
the same range does not stale the envelope. A separate bounds fingerprint detects
manual movement, resizing or rotation of the derived box. Label/filter/selection
changes alone do not change geometric freshness. Assembly records additionally
capture member IDs, physical sizes and frames relative to B. Membership, shape or
relative-pose changes stale coverage; normal rigid movement does not. Relative
frame comparisons allow the existing float geometry tolerance (10 micrometers
plus a coordinate-magnitude float guard; orientation tolerance 1e-5).

A stale envelope remains visible but is excluded from automatic obstruction
calculations, replaced by a structured unresolved warning. Explicit rules using it
also return unresolved warnings. Checks never silently interpret its old box as a
current pass. Check-result freshness includes rule and envelope provenance changes.

Generation/regeneration is one candidate transaction and one undo step; failure
preserves objects, IDs, motion Position and provenance. A current regeneration with
the same sample count is a no-op. Sampling/box creation can fail on locks, numeric
precision, constraint conflicts or scene bounds; no partial envelope is published.
The source movement cannot be removed/retyped while an envelope references it.
Generic object deletion directs the user to Volumes, where the atomic command
removes both geometry and provenance. Existing incident links/checks must still be
removed before deleting an envelope.

## Saved document and agent boundary

Schema **18** introduced `engineering.motionEnvelopes`, with up to 32 records:

- `objectId`: existing stable numeric object handle; its persistent entity ID is
  still the normal scene reference identity.
- `ruleId`, `samples` (2–129), `paddingMeters`.
- `inputDigest`, `boundsDigest`: 16 lowercase hexadecimal characters each.

Schema **19** adds `motionAssembly` to Travel/Hinge constraints (empty means B
only), and `assemblyId` plus `members` to each envelope. Member snapshots carry
`entityId`, primitive `kind`, `sizeMeters`, `localOriginMeters` and `localBasis`
relative to B. Existing schema 18 envelopes migrate as B-only and retain freshness.
Schema 19 requires these fields. Malformed scopes, excessive/duplicate members,
invalid physical sizes or nonrigid/left-handed frames reject the load atomically.
Older schema declarations cannot carry nonempty assembly motion data.

Schemas 0–17 migrate with no derived records. Schema 18+ requires the array;
malformed IDs, endpoints, duplicates, sample counts, padding or digests reject the
load atomically. Declaring a nonempty envelope array below schema 18 is rejected.
Stale records are valid saved state and remain explicitly unresolved after reopen.
Reserved geometry and provenance survive the authoritative layout snapshot in
scene export/compiler extensions; envelopes are excluded from physical solids and
camera framing just like other reserved volumes.

Public C operations are in `Layout/layout_motion.h`:
`Layout_SampleMotion` is read-only, `Layout_GenerateMotionEnvelope` accepts an undo
hook, and `Layout_MotionEnvelopeCurrent` reports freshness.
`Layout_SampleMotionSet` samples the explicit group;
`Layout_InspectMotionObstruction` returns first failing/closest sample position,
member ID, gap, hit flag and sample count, without scene writes. Existing
`agent_scene_tool --check-layout` reports envelope warnings through the structured
spatial report. No live MCP mutation transport is introduced.

## Evidence and next boundary

Regression coverage includes current-pose-clear/travel-range-obstructed geometry,
read-only sampling, history failure, undo/redo identity, target-independent
freshness, dense hinge intermediate-pose containment with both 2 and 33 samples,
physical world scale/panels, locked failures, moved drivers, stale range/bounds,
regeneration, deletion guards, schema 17 migration, eight malformed schema 18
loads, export retention and exclusion, and mouse create/check/edit/regenerate/delete.
Continuation tests add nested travel/hinge rigidity, dense group coverage, scaled
units, scope upgrade/downgrade preserving envelope identity, member changes,
lock/bounds/history rollback, malformed schema 19 and schema 18 migration, group
provenance through runtime compilation, sampled hit/clearance/false-positive cases
and mouse scope/preview/next/close/stale handling.
Source-run native fixtures cover Measure, Volumes, potential obstructions and stale
feedback for hinges, plus travel obstruction results. User hands-on acceptance
remains separate from these tests and captures.

![Assembly movement selector](assets/s4-assembly-motion.png)
![Whole-assembly envelope](assets/s4-assembly-envelope.png)
![Read-only obstruction preview](assets/s4-motion-preview.png)

Next is tighter conservative coverage, such as per-member or piecewise bounds,
and explicit analysis between sampled poses. Preserve the distinction between
conservative warnings and sampled contact. Independent constrained followers,
arbitrary moving sets, imported meshes and multiple joint solving need separate
contracts. Routing and compiler topology bridges remain later phases.
