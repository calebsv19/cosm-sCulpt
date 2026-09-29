# Engineering Document Foundation (S0)

Status: S0/S1a/S1b foundations implemented; S1c and initial S1d rules added (schema 11).
Date: 2026-09-28

Current schema is **11**, adding persistent driving rules and validating them on
load/save/export. See [geometric constraints](geometric_constraints.md). The schema
10 account below records the original physical-context migration; 0–10 are still
readable, while older readers reject 11.

## Physical meaning and compatibility

Canonical physical quantities are meters and the coordinate basis is explicitly
right-handed Z-up. Display units and viewport zoom do not change geometry.
New layouts use one meter per stored world unit. Legacy scene `world_scale` is
retained as meters per stored world unit; no automatic geometry rewrite occurs.
The conversion is `meters = stored_coordinate * metersPerWorldUnit` through the
existing core_units library. This applies to scalar dimensions and positions;
instance scale remains a separate dimensionless geometric transform.

Layout JSON schema 10 adds required `physicalContext.coordinateSystem` equal to
`right_handed_z_up_meters`, a finite positive `metersPerWorldUnit`, and per-object
`persistentId`. Layout versions 0-9 remain readable with historical meter defaults
when physical context is absent. Older readers reject version 10 rather than
silently lose scale/identity. Unsupported explicit frames require an explicit
conversion before import; this slice does not implement arbitrary frame conversion.

Canonical scene import retains its root world_scale in the layout. Export without
an explicit scale option uses the retained scale; an explicitly supplied export
scale is also written into the embedded layout snapshot. Existing authoring/runtime
schema names and consumer world_scale semantics remain unchanged. Changing an
export scale option intentionally changes physical interpretation; it is not a
coordinate-normalization operation.

This fixes a reproduced regression: a scene with world_scale=1.25 imported
successfully but re-exported at 1.0. Unit-aware UI conversion now uses the retained
scale. Generated mesh asset contents are not rewritten; asset-local size and
instance transform remain distinct, and arbitrary mesh-unit normalization is
outside this slice.

## Stable references

Persistent identity is the existing CoreObject string ID, independent of the
numeric object handle, display name or future parent path. Version 10 serializes
that ID through layout save/reopen and history. Invalid/duplicate persisted IDs
are rejected without replacing the live layout. Legacy layouts keep their existing
numeric-handle-derived IDs; no new UUID scheme or bulk re-identification is introduced.

Future geometric operands are `(entity_id, reference_kind, feature_or_datum_id)`.
Begin with origin, local axis, primitive face and explicit point. Do not use screen
coordinates, preview triangle indices or array offsets as persistent references.
Origin/axis/named-face operands are implemented and now used by persistent rules.
Independently authored point/axis datums remain future work.
Clone/reparent/fork semantics need their own operations and tests before adoption.

## Numerical command boundary (S1a)

The first command family covers exact primitive dimensions and absolute/relative
world-space position. It must resolve a unique persistent ID, validate finite
physical inputs, build an isolated candidate, check requested against actual
physical values, reserve undo history, and only then publish one change.
Failure/no-op must preserve geometry, dirty state and undo/redo counts. A numerical
edit that existing bounds/plane-lock policy changes is rejected, not silently
reported as the requested result. Driving constraints now participate through the common Layout transaction.
Multi-command/revision-checked external agent transactions remain future work.

Primitive numerical dimensions initially require unit instance scale. This avoids
confusing raw primitive size with scaled physical dimensions. Position refers to
primitive center; mesh placement retains the existing UI bounds-center convention.
Dimensions and positions use core_units conversion. Existing Euler rotation UI is
outside this first command family's guarantee.

For float-backed app geometry, requested-versus-actual comparison uses
`1e-6 meters + 1e-7 * abs(requested_meters)` per edited component. This is an
acceptance tolerance, not a claim of exact arithmetic or measurement accuracy.
Large/unrepresentable/non-finite inputs must fail. Retain authored numeric values
in meters for command evaluation, convert only at the storage boundary, and state
physical units in diagnostics. Do not use screen-pixel tolerance for engineering.

## Ownership and next gates

core_units owns conversion; core_object/core_scene retain identity/root contracts.
App Layout owns physical context, reference evaluation, persistent rules and
geometry transactions; Editor owns typed commands/history hooks, UI owns input/display. No shared API/version changes are needed.

S0 verification covers scale-loss regression, retained-scale UI conversion,
legacy layout loading, physical context/identity round trips and invalid/duplicate
context rejection. S1a adds edit-conflict/undo/reopen/export tests. Named views,
At that checkpoint, measurement selection, persistent gaps/angles, movement limits
and assemblies were the subsequent slices. The current S1b/S1c and initial S1d
status is recorded below and in the constraint contract. See [product direction](spatial_engineering_direction.md).

## Delivered S1a and evidence

`Editor_ApplyNumericEdit` implements single-operation dimensions, absolute position
and relative translation by persistent entity ID. Existing dimension and position
dialogs use this command. Dimension input accepts suffixes such as `20 mm` and
`3.5 in`; bare values use display units. Position UI retains its three-number
current-unit entry. Relative translation is available through the C command, not
a new UI control or external agent endpoint. Object visibility, lock and
selectability flags now survive snapshots as well.

Verification on 2026-09-28: `make test` passed 396 tests across 40 reported suites
(including the separate folder-picker suite); the additional
`LayoutObject3DResize` group passed 25 tests. `make agent-scene-smoke` and
`make package-desktop-main-edit-self-test` passed. Source editor first-frame
capture completed with wrapper exit 0; this is startup/render evidence, not
hands-on acceptance of the new dialogs. UI-handler tests cover unit input,
malformed input and rejected bounds edits. No new release or Desktop install.

This slice does not add measurement tools, orthographic controls, persistent
distance/angle constraints, a solver, arbitrary frame conversion, numerical
rotation transactions or revision-checked multi-operation agent edits.

## S1b follow-up

The [geometric measurement contract](geometric_measurements.md) now implements
stable primitive origin/axis/face resolution and read-only distance/angle queries
through the Object tab Measure dialog. The S1a exclusions above describe that
checkpoint. Viewport markers/annotations and named views are implemented. The
[persistent-rule follow-up](geometric_constraints.md) delivers S1c and initial S1d;
broader snapping and authored datums remain future work.
