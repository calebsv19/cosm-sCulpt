#pragma once
#include "Editor/editor_measurement.h"
#include "Editor/editor_numeric_edit.h"

typedef enum EditorReferencePlacementKind {
    EDITOR_PLACE_PROJECTED_DISTANCE,
    EDITOR_PLACE_COINCIDENT_POINTS
} EditorReferencePlacementKind;

typedef struct EditorReferencePlacement {
    EditorGeometricReference a;
    EditorGeometricReference b;
    EditorReferencePlacementKind kind;
    Vec3 world_axis;
    double target_meters;
} EditorReferencePlacement;

/* One-time exact placement: A remains fixed; only B translates. Projected mode
 * preserves B's perpendicular displacement. Coincident mode aligns the two
 * reference points (face centers, not surfaces). This creates no persistent rule.
 * Failure/no-op preserves document, dirty state and undo/redo. */
EditorNumericEditResult Editor_ApplyReferencePlacement(EditorState* editor,
    Layout* layout, const EditorReferencePlacement* placement);
