#include "Layout/layout.h"

#include <math.h>

static const float kDefaultSceneBoundsHalfExtent = 100.0f;
static const float kConstructionPlaneEpsilon = 1e-4f;

static float ClampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static bool IsAxisValid(ViewPlaneAxis axis) {
    return axis == VIEW_PLANE_XY || axis == VIEW_PLANE_YZ || axis == VIEW_PLANE_XZ;
}

void Layout_Scene3DSettings_SetDefaults(Scene3DSettings* settings) {
    if (!settings) return;
    settings->bounds.enabled = true;
    settings->bounds.clampOnEdit = false;
    settings->bounds.min = (Vec3){
        .x = -kDefaultSceneBoundsHalfExtent,
        .y = -kDefaultSceneBoundsHalfExtent,
        .z = -kDefaultSceneBoundsHalfExtent
    };
    settings->bounds.max = (Vec3){
        .x = kDefaultSceneBoundsHalfExtent,
        .y = kDefaultSceneBoundsHalfExtent,
        .z = kDefaultSceneBoundsHalfExtent
    };
    Layout_ConstructionPlane3D_SetDefaults(&settings->constructionPlane);
}

bool Layout_SceneBounds3D_IsValid(const SceneBounds3D* bounds) {
    if (!bounds) return false;
    return bounds->min.x <= bounds->max.x &&
           bounds->min.y <= bounds->max.y &&
           bounds->min.z <= bounds->max.z;
}

bool Layout_SceneBounds3D_ClampPoint(const SceneBounds3D* bounds, Vec3* point, bool* outClamped) {
    if (outClamped) *outClamped = false;
    if (!bounds || !point) return false;
    if (!bounds->enabled) return true;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;

    Vec3 clamped = *point;
    clamped.x = ClampFloat(clamped.x, bounds->min.x, bounds->max.x);
    clamped.y = ClampFloat(clamped.y, bounds->min.y, bounds->max.y);
    clamped.z = ClampFloat(clamped.z, bounds->min.z, bounds->max.z);

    if (outClamped) {
        *outClamped = clamped.x != point->x ||
                      clamped.y != point->y ||
                      clamped.z != point->z;
    }

    *point = clamped;
    return true;
}

void Layout_ConstructionPlane3D_SetDefaults(ConstructionPlane3D* plane) {
    if (!plane) return;
    plane->mode = CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
    plane->axisAligned.axis = VIEW_PLANE_XY;
    plane->axisAligned.offset = 0.0f;

    Plane3 p = Plane3_FromViewPlane((ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f });
    plane->customFrame = PlaneFrame3_FromPlane(p, (Vec3){ 0.0f, 0.0f, 0.0f });
}

void Layout_ConstructionPlane3D_SetFromViewPlane(ConstructionPlane3D* plane, ViewPlane viewPlane) {
    if (!plane) return;
    plane->mode = CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
    plane->axisAligned = viewPlane;
}

bool Layout_ConstructionPlane3D_IsValid(const ConstructionPlane3D* plane) {
    if (!plane) return false;
    if (plane->mode == CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED) {
        return IsAxisValid(plane->axisAligned.axis);
    }
    if (plane->mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME) {
        const float uLen = Vec3_Length(plane->customFrame.axisU);
        const float vLen = Vec3_Length(plane->customFrame.axisV);
        const float nLen = Vec3_Length(plane->customFrame.normal);
        if (uLen <= kConstructionPlaneEpsilon ||
            vLen <= kConstructionPlaneEpsilon ||
            nLen <= kConstructionPlaneEpsilon) {
            return false;
        }

        Vec3 cross = Vec3_Cross(plane->customFrame.axisU, plane->customFrame.axisV);
        const float crossLen = Vec3_Length(cross);
        if (crossLen <= kConstructionPlaneEpsilon) return false;

        Vec3 crossN = Vec3_Normalize(cross);
        Vec3 normalN = Vec3_Normalize(plane->customFrame.normal);
        const float alignment = fabsf(Vec3_Dot(crossN, normalN));
        return alignment >= 0.90f;
    }
    return false;
}

ViewPlane Layout_ConstructionPlane3D_ToViewPlane(const ConstructionPlane3D* plane) {
    ViewPlane fallback = { .axis = VIEW_PLANE_XY, .offset = 0.0f };
    if (!Layout_ConstructionPlane3D_IsValid(plane)) return fallback;

    if (plane->mode == CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED) {
        return plane->axisAligned;
    }

    const Vec3 n = Vec3_Normalize(plane->customFrame.normal);
    const Vec3 o = plane->customFrame.origin;
    const float ax = fabsf(n.x);
    const float ay = fabsf(n.y);
    const float az = fabsf(n.z);
    if (ax >= ay && ax >= az) {
        return (ViewPlane){ .axis = VIEW_PLANE_YZ, .offset = o.x };
    }
    if (ay >= ax && ay >= az) {
        return (ViewPlane){ .axis = VIEW_PLANE_XZ, .offset = o.y };
    }
    return (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = o.z };
}
