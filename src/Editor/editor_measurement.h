#pragma once
#include "Layout/layout.h"

typedef enum EditorReferenceKind {
    EDITOR_REFERENCE_ORIGIN,
    EDITOR_REFERENCE_AXIS_U,
    EDITOR_REFERENCE_AXIS_V,
    EDITOR_REFERENCE_AXIS_N,
    EDITOR_REFERENCE_FACE
} EditorReferenceKind;

/* Stable operand: no selection index, mesh triangle or cached world coordinate.
 * Primitive U/V/N use the authored frame, which already includes rotation. */
typedef struct EditorGeometricReference {
    char entity_id[64];
    EditorReferenceKind kind;
    Object3DFaceKind face;
} EditorGeometricReference;

typedef enum EditorMeasurementStatus {
    EDITOR_MEASUREMENT_OK,
    EDITOR_MEASUREMENT_INVALID,
    EDITOR_MEASUREMENT_UNRESOLVED,
    EDITOR_MEASUREMENT_UNSUPPORTED,
    EDITOR_MEASUREMENT_DEGENERATE
} EditorMeasurementStatus;

typedef struct EditorResolvedReference {
    double point_meters[3];
    double direction[3];
    bool has_direction;
    bool is_plane;
} EditorResolvedReference;

typedef enum EditorMeasurementKind {
    EDITOR_MEASURE_POINT_DISTANCE,
    EDITOR_MEASURE_PROJECTED_DISTANCE,
    EDITOR_MEASURE_DIRECTION_ANGLE,
    EDITOR_MEASURE_PLANAR_ANGLE,
    EDITOR_MEASURE_PARALLEL_PLANE_GAP
} EditorMeasurementKind;

typedef struct EditorMeasurementResult {
    EditorMeasurementStatus status;
    double value; /* meters for distances, degrees for angles */
    char message[128];
} EditorMeasurementResult;

EditorMeasurementStatus Editor_ResolveReference(const Layout* layout,
    const EditorGeometricReference* reference, EditorResolvedReference* out);
/* Read-only, synchronous evaluation against current geometry. The supplied world
 * vector is the projection axis or planar angle normal. Planar angles require
 * both directions in that plane; they are not projected silently. */
EditorMeasurementResult Editor_Measure(const Layout* layout,
    const EditorGeometricReference* a, const EditorGeometricReference* b,
    EditorMeasurementKind kind, Vec3 world_vector);
const char* Editor_MeasurementStatusLabel(EditorMeasurementStatus status);
