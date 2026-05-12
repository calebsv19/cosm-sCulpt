#include "Layout/layout.h"
#include "Core/global_state.h"

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

static bool SceneBoundsHandle_Sides(SceneBoundsHandleKind handle,
                                    int* outX,
                                    int* outY,
                                    int* outZ) {
    int sx = 0;
    int sy = 0;
    int sz = 0;
    switch (handle) {
        case SCENE_BOUNDS_HANDLE_MIN_X: sx = -1; break;
        case SCENE_BOUNDS_HANDLE_MAX_X: sx = 1; break;
        case SCENE_BOUNDS_HANDLE_MIN_Y: sy = -1; break;
        case SCENE_BOUNDS_HANDLE_MAX_Y: sy = 1; break;
        case SCENE_BOUNDS_HANDLE_MIN_Z: sz = -1; break;
        case SCENE_BOUNDS_HANDLE_MAX_Z: sz = 1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MIN_Y_MIN_Z: sx = -1; sy = -1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MIN_Z: sx = 1; sy = -1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MAX_Y_MIN_Z: sx = -1; sy = 1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MAX_Y_MIN_Z: sx = 1; sy = 1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MIN_Y_MAX_Z: sx = -1; sy = -1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z: sx = 1; sy = -1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MAX_Y_MAX_Z: sx = -1; sy = 1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MAX_Y_MAX_Z: sx = 1; sy = 1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MIN_Z: sy = -1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_X_MAX_Y_MIN_Z: sy = 1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z: sy = -1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_X_MAX_Y_MAX_Z: sy = 1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Y_MIN_X_MIN_Z: sx = -1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Y_MAX_X_MIN_Z: sx = 1; sz = -1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Y_MIN_X_MAX_Z: sx = -1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Y_MAX_X_MAX_Z: sx = 1; sz = 1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Z_MIN_X_MIN_Y: sx = -1; sy = -1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Z_MAX_X_MIN_Y: sx = 1; sy = -1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Z_MIN_X_MAX_Y: sx = -1; sy = 1; break;
        case SCENE_BOUNDS_HANDLE_EDGE_Z_MAX_X_MAX_Y: sx = 1; sy = 1; break;
        case SCENE_BOUNDS_HANDLE_CENTER: break;
        case SCENE_BOUNDS_HANDLE_NONE:
        default:
            return false;
    }
    if (outX) *outX = sx;
    if (outY) *outY = sy;
    if (outZ) *outZ = sz;
    return true;
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

bool Layout_SceneBoundsHandle_IsValid(SceneBoundsHandleKind handle) {
    return handle >= SCENE_BOUNDS_HANDLE_MIN_X &&
           handle <= SCENE_BOUNDS_HANDLE_CENTER;
}

bool Layout_SceneBoundsHandleAxisMask(SceneBoundsHandleKind handle,
                                      RectPrismHandleAxisMask* outMask) {
    if (!outMask) return false;
    *outMask = (RectPrismHandleAxisMask){0};
    if (handle == SCENE_BOUNDS_HANDLE_CENTER) {
        *outMask = (RectPrismHandleAxisMask){
            .allowU = true,
            .allowV = true,
            .allowN = true
        };
        return true;
    }

    int sx = 0;
    int sy = 0;
    int sz = 0;
    if (!SceneBoundsHandle_Sides(handle, &sx, &sy, &sz)) return false;
    outMask->allowU = sx != 0;
    outMask->allowV = sy != 0;
    outMask->allowN = sz != 0;
    return outMask->allowU || outMask->allowV || outMask->allowN;
}

Vec3 Layout_SceneBoundsAxisDirection_WorldVector(RectPrismAxisDirection direction) {
    switch (direction) {
        case RECT_PRISM_AXIS_DIR_POS_U: return (Vec3){  1.0f,  0.0f,  0.0f };
        case RECT_PRISM_AXIS_DIR_NEG_U: return (Vec3){ -1.0f,  0.0f,  0.0f };
        case RECT_PRISM_AXIS_DIR_POS_V: return (Vec3){  0.0f,  1.0f,  0.0f };
        case RECT_PRISM_AXIS_DIR_NEG_V: return (Vec3){  0.0f, -1.0f,  0.0f };
        case RECT_PRISM_AXIS_DIR_POS_N: return (Vec3){  0.0f,  0.0f,  1.0f };
        case RECT_PRISM_AXIS_DIR_NEG_N: return (Vec3){  0.0f,  0.0f, -1.0f };
        default: return (Vec3){ 0.0f, 0.0f, 0.0f };
    }
}

bool Layout_SceneBoundsHandleWorldPoint(const SceneBounds3D* bounds,
                                        SceneBoundsHandleKind handle,
                                        Vec3* outPoint) {
    if (!bounds || !outPoint) return false;
    if (!bounds->enabled) return false;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;
    if (!Layout_SceneBoundsHandle_IsValid(handle)) return false;

    const Vec3 center = Vec3_Scale(Vec3_Add(bounds->min, bounds->max), 0.5f);
    int sx = 0;
    int sy = 0;
    int sz = 0;
    if (!SceneBoundsHandle_Sides(handle, &sx, &sy, &sz)) return false;
    *outPoint = (Vec3){
        .x = sx < 0 ? bounds->min.x : (sx > 0 ? bounds->max.x : center.x),
        .y = sy < 0 ? bounds->min.y : (sy > 0 ? bounds->max.y : center.y),
        .z = sz < 0 ? bounds->min.z : (sz > 0 ? bounds->max.z : center.z)
    };
    return true;
}

bool Layout_ResizeSceneBounds3DFromHandle(Layout* layout,
                                          SceneBoundsHandleKind handle,
                                          Vec3 draggedWorldPoint) {
    if (!layout) return false;
    SceneBounds3D* bounds = &layout->scene3d.bounds;
    if (!bounds->enabled) return false;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;

    int sx = 0;
    int sy = 0;
    int sz = 0;
    if (!SceneBoundsHandle_Sides(handle, &sx, &sy, &sz)) return false;
    if (sx == 0 && sy == 0 && sz == 0) return false;

    if (sx < 0) bounds->min.x = fminf(draggedWorldPoint.x, bounds->max.x);
    if (sx > 0) bounds->max.x = fmaxf(draggedWorldPoint.x, bounds->min.x);
    if (sy < 0) bounds->min.y = fminf(draggedWorldPoint.y, bounds->max.y);
    if (sy > 0) bounds->max.y = fmaxf(draggedWorldPoint.y, bounds->min.y);
    if (sz < 0) bounds->min.z = fminf(draggedWorldPoint.z, bounds->max.z);
    if (sz > 0) bounds->max.z = fmaxf(draggedWorldPoint.z, bounds->min.z);

    Global_FlagLayoutChanged();
    return true;
}

bool Layout_TranslateSceneBounds3D(Layout* layout, Vec3 delta) {
    if (!layout) return false;
    SceneBounds3D* bounds = &layout->scene3d.bounds;
    if (!bounds->enabled) return false;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;

    bounds->min = Vec3_Add(bounds->min, delta);
    bounds->max = Vec3_Add(bounds->max, delta);
    Global_FlagLayoutChanged();
    return true;
}

bool Layout_FitSceneBounds3DToObject(Layout* layout,
                                     uint32_t objectId,
                                     float padding) {
    if (!layout || objectId == 0u) return false;

    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    Vec3 minPoint = {0};
    Vec3 maxPoint = {0};
    if (!Layout_Object3D_ComputeWorldAABB(object, &minPoint, &maxPoint)) return false;

    const float pad = fmaxf(padding, 0.0f);
    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){
        .x = minPoint.x - pad,
        .y = minPoint.y - pad,
        .z = minPoint.z - pad
    };
    layout->scene3d.bounds.max = (Vec3){
        .x = maxPoint.x + pad,
        .y = maxPoint.y + pad,
        .z = maxPoint.z + pad
    };

    Global_FlagLayoutChanged();
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
