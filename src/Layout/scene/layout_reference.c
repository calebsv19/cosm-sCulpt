#include "Layout/layout_reference.h"
#include "Layout/scene/layout_object_faces.h"
#include "core_units.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static double dot(const double a[3], const double b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static bool normalize(double v[3]) {
    double n = hypot(hypot(v[0], v[1]), v[2]);
    if (!isfinite(n) || n <= 1e-12) return false;
    for (int i = 0; i < 3; ++i) v[i] /= n;
    return true;
}

const char* Layout_MeasurementStatusLabel(LayoutMeasurementStatus status) {
    switch (status) {
        case LAYOUT_MEASUREMENT_OK: return "Valid";
        case LAYOUT_MEASUREMENT_UNRESOLVED: return "Missing or ambiguous entity";
        case LAYOUT_MEASUREMENT_UNSUPPORTED: return "Unsupported reference or geometry";
        case LAYOUT_MEASUREMENT_DEGENERATE: return "Degenerate geometry or direction";
        default: return "Invalid reference or physical context";
    }
}

LayoutMeasurementStatus Layout_ResolveReference(const Layout* layout,
    const LayoutGeometricReference* reference, LayoutResolvedReference* out) {
    if (out) *out = (LayoutResolvedReference){0};
    if (!layout || !reference || !out || !reference->entity_id[0] ||
        !memchr(reference->entity_id, '\0', sizeof(reference->entity_id)) ||
        reference->kind < LAYOUT_REFERENCE_ORIGIN || reference->kind > LAYOUT_REFERENCE_FACE ||
        core_units_validate_world_scale(Layout_WorldScale(layout)).code != CORE_OK)
        return LAYOUT_MEASUREMENT_INVALID;
    const Object3D* object = NULL;
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* candidate = &layout->objectStore.items[i];
        if (!candidate->isDeleted && strcmp(candidate->coreMeta.object_id, reference->entity_id) == 0) {
            if (object) return LAYOUT_MEASUREMENT_UNRESOLVED;
            object = candidate;
        }
    }
    if (!object) return LAYOUT_MEASUREMENT_UNRESOLVED;
    if (object->kind != OBJECT3D_KIND_PLANE && object->kind != OBJECT3D_KIND_RECT_PRISM)
        return LAYOUT_MEASUREMENT_UNSUPPORTED;
    /* Current primitive geometry lives in its authored world frame. Applying
     * transform Euler angles again would rotate measurements twice. Non-unit
     * instance scale is excluded until its primitive semantics are unified. */
    if (object->transform.scale.x != 1 || object->transform.scale.y != 1 || object->transform.scale.z != 1)
        return LAYOUT_MEASUREMENT_UNSUPPORTED;
    PlaneFrame3 frame = object->kind == OBJECT3D_KIND_PLANE ? object->plane.frame : object->rectPrism.frame;
    double u[3] = {frame.axisU.x, frame.axisU.y, frame.axisU.z};
    double v[3] = {frame.axisV.x, frame.axisV.y, frame.axisV.z};
    double n[3] = {frame.normal.x, frame.normal.y, frame.normal.z};
    if (!normalize(u) || !normalize(v) || !normalize(n)) return LAYOUT_MEASUREMENT_DEGENERATE;
    if (fabs(dot(u,v)) > 1e-5 || fabs(dot(u,n)) > 1e-5 || fabs(dot(v,n)) > 1e-5)
        return LAYOUT_MEASUREMENT_UNSUPPORTED;
    double width = object->kind == OBJECT3D_KIND_PLANE ? object->plane.width : object->rectPrism.width;
    double height = object->kind == OBJECT3D_KIND_PLANE ? object->plane.height : object->rectPrism.height;
    if (!isfinite(width) || !isfinite(height) || width <= 0 || height <= 0 ||
        (object->kind == OBJECT3D_KIND_RECT_PRISM &&
         (!isfinite(object->rectPrism.depth) || object->rectPrism.depth <= 0)))
        return LAYOUT_MEASUREMENT_DEGENERATE;
    Vec3 direction = {0};
    if (reference->kind == LAYOUT_REFERENCE_FACE) {
        if (!Layout_Object3DFace_GetFrame(object, reference->face, &frame))
            return LAYOUT_MEASUREMENT_UNSUPPORTED;
        direction = frame.normal;
        out->is_plane = true;
    } else if (reference->kind == LAYOUT_REFERENCE_AXIS_U) direction = frame.axisU;
    else if (reference->kind == LAYOUT_REFERENCE_AXIS_V) direction = frame.axisV;
    else if (reference->kind == LAYOUT_REFERENCE_AXIS_N) direction = frame.normal;
    const double scale = Layout_WorldScale(layout);
    out->point_meters[0] = (double)frame.origin.x * scale;
    out->point_meters[1] = (double)frame.origin.y * scale;
    out->point_meters[2] = (double)frame.origin.z * scale;
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(reference->local_offset_meters[i])) return LAYOUT_MEASUREMENT_INVALID;
        out->point_meters[i] += u[i]*reference->local_offset_meters[0]
            + v[i]*reference->local_offset_meters[1] + n[i]*reference->local_offset_meters[2];
    }
    for (int i = 0; i < 3; ++i)
        if (!isfinite(out->point_meters[i])) return LAYOUT_MEASUREMENT_INVALID;
    out->has_direction = reference->kind != LAYOUT_REFERENCE_ORIGIN;
    if (out->has_direction) {
        out->direction[0] = direction.x;
        out->direction[1] = direction.y;
        out->direction[2] = direction.z;
        if (!normalize(out->direction)) return LAYOUT_MEASUREMENT_DEGENERATE;
    }
    return LAYOUT_MEASUREMENT_OK;
}

static LayoutMeasurementResult result(LayoutMeasurementStatus status, const char* message, double value) {
    LayoutMeasurementResult r = {.status = status, .value = value};
    snprintf(r.message, sizeof(r.message), "%s", message);
    return r;
}

LayoutMeasurementResult Layout_Measure(const Layout* layout,
    const LayoutGeometricReference* a, const LayoutGeometricReference* b,
    LayoutMeasurementKind kind, Vec3 world_vector) {
    LayoutResolvedReference ra, rb;
    LayoutMeasurementStatus s = Layout_ResolveReference(layout, a, &ra);
    if (s == LAYOUT_MEASUREMENT_OK) s = Layout_ResolveReference(layout, b, &rb);
    if (s != LAYOUT_MEASUREMENT_OK) return result(s, Layout_MeasurementStatusLabel(s), NAN);
    double delta[3], vector[3] = {world_vector.x, world_vector.y, world_vector.z};
    for (int i = 0; i < 3; ++i) delta[i] = rb.point_meters[i] - ra.point_meters[i];
    double value = NAN;
    if (kind == LAYOUT_MEASURE_POINT_DISTANCE) value = hypot(hypot(delta[0], delta[1]), delta[2]);
    else if (kind == LAYOUT_MEASURE_PROJECTED_DISTANCE) {
        if (!normalize(vector)) return result(LAYOUT_MEASUREMENT_DEGENERATE, "Projection axis must be finite and nonzero", NAN);
        value = dot(delta, vector);
    } else if (kind >= LAYOUT_MEASURE_DIRECTION_ANGLE && kind <= LAYOUT_MEASURE_PARALLEL_PLANE_GAP) {
        if (!ra.has_direction || !rb.has_direction)
            return result(LAYOUT_MEASUREMENT_UNSUPPORTED, "Choose two axes or faces for directions", NAN);
        double cosine = fmax(-1.0, fmin(1.0, dot(ra.direction, rb.direction)));
        if (kind == LAYOUT_MEASURE_PARALLEL_PLANE_GAP) {
            if (!ra.is_plane || !rb.is_plane || 1.0 - fabs(cosine) > 1e-10)
                return result(LAYOUT_MEASUREMENT_UNSUPPORTED, "Gap requires two parallel face planes", NAN);
            value = dot(delta, ra.direction);
        } else if (kind == LAYOUT_MEASURE_DIRECTION_ANGLE) value = acos(cosine) * (180.0 / 3.14159265358979323846);
        else {
            if (!normalize(vector)) return result(LAYOUT_MEASUREMENT_DEGENERATE, "Angle normal must be finite and nonzero", NAN);
            if (fabs(dot(ra.direction, vector)) > 1e-5 || fabs(dot(rb.direction, vector)) > 1e-5)
                return result(LAYOUT_MEASUREMENT_UNSUPPORTED, "Directions must lie in the chosen plane", NAN);
            double cross[3] = {
                ra.direction[1]*rb.direction[2] - ra.direction[2]*rb.direction[1],
                ra.direction[2]*rb.direction[0] - ra.direction[0]*rb.direction[2],
                ra.direction[0]*rb.direction[1] - ra.direction[1]*rb.direction[0]};
            value = atan2(dot(vector,cross), cosine) * (180.0 / 3.14159265358979323846);
            if (value <= -180.0) value = 180.0;
        }
    } else return result(LAYOUT_MEASUREMENT_INVALID, "Unknown measurement kind", NAN);
    if (!isfinite(value)) return result(LAYOUT_MEASUREMENT_INVALID, "Measurement exceeds numeric range", NAN);
    return result(LAYOUT_MEASUREMENT_OK, "Current geometry; reference measurement only", value);
}
