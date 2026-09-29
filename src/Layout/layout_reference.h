#pragma once
#include "Layout/layout.h"

typedef enum LayoutMeasurementStatus {
    LAYOUT_MEASUREMENT_OK,
    LAYOUT_MEASUREMENT_INVALID,
    LAYOUT_MEASUREMENT_UNRESOLVED,
    LAYOUT_MEASUREMENT_UNSUPPORTED,
    LAYOUT_MEASUREMENT_DEGENERATE
} LayoutMeasurementStatus;

typedef struct LayoutResolvedReference {
    double point_meters[3];
    double direction[3];
    bool has_direction;
    bool is_plane;
} LayoutResolvedReference;

typedef enum LayoutMeasurementKind {
    LAYOUT_MEASURE_POINT_DISTANCE,
    LAYOUT_MEASURE_PROJECTED_DISTANCE,
    LAYOUT_MEASURE_DIRECTION_ANGLE,
    LAYOUT_MEASURE_PLANAR_ANGLE,
    LAYOUT_MEASURE_PARALLEL_PLANE_GAP
} LayoutMeasurementKind;

typedef struct LayoutMeasurementResult {
    LayoutMeasurementStatus status;
    double value; /* meters for distances, degrees for angles */
    char message[128];
} LayoutMeasurementResult;

LayoutMeasurementStatus Layout_ResolveReference(const Layout* layout,
    const LayoutGeometricReference* reference, LayoutResolvedReference* out);
/* Read-only, synchronous evaluation against current geometry. The supplied world
 * vector is the projection axis or planar angle normal. Planar angles require
 * both directions in that plane; they are not projected silently. */
LayoutMeasurementResult Layout_Measure(const Layout* layout,
    const LayoutGeometricReference* a, const LayoutGeometricReference* b,
    LayoutMeasurementKind kind, Vec3 world_vector);
const char* Layout_MeasurementStatusLabel(LayoutMeasurementStatus status);
