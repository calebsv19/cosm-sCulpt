// src/Layout/layout_primitives_rect_prism_resize.c
#include "layout.h"
#include "Core/global_state.h"

#include <math.h>
#include <float.h>

static const float kConstructionPlaneEpsilon = 1e-4f;
static const float kRectPrismPrimitiveMinSize = 1e-3f;

static float SceneBounds_DistanceToBoundary(const SceneBounds3D* bounds, Vec3 origin, Vec3 direction) {
    if (!bounds || !Layout_SceneBounds3D_IsValid(bounds)) return 0.0f;
    if (Vec3_Length(direction) <= kConstructionPlaneEpsilon) return 0.0f;

    Vec3 dir = Vec3_Normalize(direction);
    float limit = FLT_MAX;

    if (fabsf(dir.x) > kConstructionPlaneEpsilon) {
        const float tx = (dir.x > 0.0f) ? (bounds->max.x - origin.x) / dir.x
                                        : (bounds->min.x - origin.x) / dir.x;
        if (tx >= 0.0f) limit = fminf(limit, tx);
    }
    if (fabsf(dir.y) > kConstructionPlaneEpsilon) {
        const float ty = (dir.y > 0.0f) ? (bounds->max.y - origin.y) / dir.y
                                        : (bounds->min.y - origin.y) / dir.y;
        if (ty >= 0.0f) limit = fminf(limit, ty);
    }
    if (fabsf(dir.z) > kConstructionPlaneEpsilon) {
        const float tz = (dir.z > 0.0f) ? (bounds->max.z - origin.z) / dir.z
                                        : (bounds->min.z - origin.z) / dir.z;
        if (tz >= 0.0f) limit = fminf(limit, tz);
    }

    if (limit == FLT_MAX) return 0.0f;
    if (limit < 0.0f) return 0.0f;
    return limit;
}

static bool SceneBounds_ClampRectPrismExtents(const SceneBounds3D* bounds,
                                              Vec3 center,
                                              Vec3 axisU,
                                              Vec3 axisV,
                                              Vec3 axisN,
                                              float* width,
                                              float* height,
                                              float* depth,
                                              bool* outAdjusted) {
    if (outAdjusted) *outAdjusted = false;
    if (!bounds || !width || !height || !depth) return false;
    if (!bounds->enabled) return true;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;

    const float uPos = SceneBounds_DistanceToBoundary(bounds, center, axisU);
    const float uNeg = SceneBounds_DistanceToBoundary(bounds, center, Vec3_Scale(axisU, -1.0f));
    const float vPos = SceneBounds_DistanceToBoundary(bounds, center, axisV);
    const float vNeg = SceneBounds_DistanceToBoundary(bounds, center, Vec3_Scale(axisV, -1.0f));
    const float nPos = SceneBounds_DistanceToBoundary(bounds, center, axisN);
    const float nNeg = SceneBounds_DistanceToBoundary(bounds, center, Vec3_Scale(axisN, -1.0f));
    const float maxHalfU = fminf(uPos, uNeg);
    const float maxHalfV = fminf(vPos, vNeg);
    const float maxHalfN = fminf(nPos, nNeg);
    if (maxHalfU <= kRectPrismPrimitiveMinSize ||
        maxHalfV <= kRectPrismPrimitiveMinSize ||
        maxHalfN < 0.0f) {
        return false;
    }

    float nextW = *width;
    float nextH = *height;
    float nextD = *depth;
    const float maxW = maxHalfU * 2.0f;
    const float maxH = maxHalfV * 2.0f;
    const float maxD = maxHalfN * 2.0f;
    if (nextW > maxW) nextW = maxW;
    if (nextH > maxH) nextH = maxH;
    if (nextD > maxD) nextD = maxD;
    if (nextW < kRectPrismPrimitiveMinSize) nextW = kRectPrismPrimitiveMinSize;
    if (nextH < kRectPrismPrimitiveMinSize) nextH = kRectPrismPrimitiveMinSize;
    if (nextD < 0.0f) nextD = 0.0f;

    if (outAdjusted) {
        *outAdjusted = fabsf(nextW - *width) > 1e-5f ||
                       fabsf(nextH - *height) > 1e-5f ||
                       fabsf(nextD - *depth) > 1e-5f;
    }

    *width = nextW;
    *height = nextH;
    *depth = nextD;
    return true;
}

typedef struct {
    bool affectsU;
    bool affectsV;
    float signU;
    float signV;
} PlaneResizeHandleSpec;

static bool PlaneResize_HandleSpec(PlaneResizeHandleKind handle, PlaneResizeHandleSpec* outSpec) {
    if (!outSpec) return false;
    PlaneResizeHandleSpec spec = {0};
    switch (handle) {
        case PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V:
            spec.affectsU = true;
            spec.affectsV = true;
            spec.signU = -1.0f;
            spec.signV = -1.0f;
            break;
        case PLANE_RESIZE_HANDLE_CORNER_POS_U_NEG_V:
            spec.affectsU = true;
            spec.affectsV = true;
            spec.signU = 1.0f;
            spec.signV = -1.0f;
            break;
        case PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V:
            spec.affectsU = true;
            spec.affectsV = true;
            spec.signU = 1.0f;
            spec.signV = 1.0f;
            break;
        case PLANE_RESIZE_HANDLE_CORNER_NEG_U_POS_V:
            spec.affectsU = true;
            spec.affectsV = true;
            spec.signU = -1.0f;
            spec.signV = 1.0f;
            break;
        case PLANE_RESIZE_HANDLE_EDGE_NEG_V:
            spec.affectsU = false;
            spec.affectsV = true;
            spec.signV = -1.0f;
            break;
        case PLANE_RESIZE_HANDLE_EDGE_POS_U:
            spec.affectsU = true;
            spec.affectsV = false;
            spec.signU = 1.0f;
            break;
        case PLANE_RESIZE_HANDLE_EDGE_POS_V:
            spec.affectsU = false;
            spec.affectsV = true;
            spec.signV = 1.0f;
            break;
        case PLANE_RESIZE_HANDLE_EDGE_NEG_U:
            spec.affectsU = true;
            spec.affectsV = false;
            spec.signU = -1.0f;
            break;
        case PLANE_RESIZE_HANDLE_NONE:
        default:
            return false;
    }
    *outSpec = spec;
    return true;
}

static PlaneResizeHandleKind PlaneResize_HandleFromSpec(const PlaneResizeHandleSpec* spec) {
    if (!spec) return PLANE_RESIZE_HANDLE_NONE;
    if (spec->affectsU && spec->affectsV) {
        if (spec->signU < 0.0f && spec->signV < 0.0f) return PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V;
        if (spec->signU > 0.0f && spec->signV < 0.0f) return PLANE_RESIZE_HANDLE_CORNER_POS_U_NEG_V;
        if (spec->signU > 0.0f && spec->signV > 0.0f) return PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V;
        if (spec->signU < 0.0f && spec->signV > 0.0f) return PLANE_RESIZE_HANDLE_CORNER_NEG_U_POS_V;
        return PLANE_RESIZE_HANDLE_NONE;
    }
    if (spec->affectsU && !spec->affectsV) {
        return (spec->signU > 0.0f) ? PLANE_RESIZE_HANDLE_EDGE_POS_U
                                    : PLANE_RESIZE_HANDLE_EDGE_NEG_U;
    }
    if (!spec->affectsU && spec->affectsV) {
        return (spec->signV > 0.0f) ? PLANE_RESIZE_HANDLE_EDGE_POS_V
                                    : PLANE_RESIZE_HANDLE_EDGE_NEG_V;
    }
    return PLANE_RESIZE_HANDLE_NONE;
}

typedef struct {
    RectPrismResizeHandleKind handle;
    int signU;
    int signV;
    int signN;
} RectPrismResizeHandleSpec3D;

static const RectPrismResizeHandleSpec3D kRectPrismResizeHandleSpecs3D[] = {
    { RECT_PRISM_RESIZE_HANDLE_CORNER_0, -1, -1, -1 },
    { RECT_PRISM_RESIZE_HANDLE_CORNER_1,  1, -1, -1 },
    { RECT_PRISM_RESIZE_HANDLE_CORNER_2,  1,  1, -1 },
    { RECT_PRISM_RESIZE_HANDLE_CORNER_3, -1,  1, -1 },
    { RECT_PRISM_RESIZE_HANDLE_CORNER_4, -1, -1,  1 },
    { RECT_PRISM_RESIZE_HANDLE_CORNER_5,  1, -1,  1 },
    { RECT_PRISM_RESIZE_HANDLE_CORNER_6,  1,  1,  1 },
    { RECT_PRISM_RESIZE_HANDLE_CORNER_7, -1,  1,  1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_0,    0, -1, -1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_1,    1,  0, -1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_2,    0,  1, -1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_3,   -1,  0, -1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_4,    0, -1,  1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_5,    1,  0,  1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_6,    0,  1,  1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_7,   -1,  0,  1 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_8,   -1, -1,  0 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_9,    1, -1,  0 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_10,   1,  1,  0 },
    { RECT_PRISM_RESIZE_HANDLE_EDGE_11,  -1,  1,  0 }
};

static bool RectPrismResize3D_HandleSpec(RectPrismResizeHandleKind handle,
                                         RectPrismResizeHandleSpec3D* outSpec) {
    if (!outSpec || handle == RECT_PRISM_RESIZE_HANDLE_NONE) return false;
    const size_t count =
        sizeof(kRectPrismResizeHandleSpecs3D) / sizeof(kRectPrismResizeHandleSpecs3D[0]);
    for (size_t i = 0; i < count; ++i) {
        if (kRectPrismResizeHandleSpecs3D[i].handle == handle) {
            *outSpec = kRectPrismResizeHandleSpecs3D[i];
            return true;
        }
    }
    return false;
}

static RectPrismResizeHandleKind RectPrismResize3D_HandleFromSigns(int signU,
                                                                   int signV,
                                                                   int signN) {
    const size_t count =
        sizeof(kRectPrismResizeHandleSpecs3D) / sizeof(kRectPrismResizeHandleSpecs3D[0]);
    for (size_t i = 0; i < count; ++i) {
        const RectPrismResizeHandleSpec3D* spec = &kRectPrismResizeHandleSpecs3D[i];
        if (spec->signU == signU &&
            spec->signV == signV &&
            spec->signN == signN) {
            return spec->handle;
        }
    }
    return RECT_PRISM_RESIZE_HANDLE_NONE;
}

static float Layout_DepthDistanceForViewPoint(Vec3 point,
                                              ViewPlane plane,
                                              const FreeViewCamera* camera) {
    if (camera && camera->enabled) {
        Vec3 delta = Vec3_Sub(point, camera->target);
        Vec3 forward = FreeView_Forward(camera);
        return fabsf(Vec3_Dot(delta, forward));
    }
    return ViewPlane_AbsDistance(plane, point);
}

static void PlaneResize_ResolveAxisExtents(float fixedCoord,
                                           float draggedCoord,
                                           float minSpan,
                                           float fallbackSign,
                                           float* outMin,
                                           float* outMax) {
    if (!outMin || !outMax) return;

    float moved = draggedCoord;
    float delta = moved - fixedCoord;
    if (fabsf(delta) < minSpan) {
        float sign = (delta >= 0.0f) ? 1.0f : -1.0f;
        if (fabsf(delta) <= 1e-6f) {
            sign = (fallbackSign >= 0.0f) ? 1.0f : -1.0f;
        }
        moved = fixedCoord + sign * minSpan;
    }

    if (fixedCoord <= moved) {
        *outMin = fixedCoord;
        *outMax = moved;
    } else {
        *outMin = moved;
        *outMax = fixedCoord;
    }
}

static void Transform3D_ToCore(const Transform3D* in, CoreObjectTransform* out) {
    if (!in || !out) return;
    out->position.x = (double)in->position.x;
    out->position.y = (double)in->position.y;
    out->position.z = (double)in->position.z;
    out->rotation_deg.x = (double)in->rotationDeg.x;
    out->rotation_deg.y = (double)in->rotationDeg.y;
    out->rotation_deg.z = (double)in->rotationDeg.z;
    out->scale.x = (double)in->scale.x;
    out->scale.y = (double)in->scale.y;
    out->scale.z = (double)in->scale.z;
}

static void Transform3D_FromCore(const CoreObjectTransform* in, Transform3D* out) {
    if (!in || !out) return;
    out->position.x = (float)in->position.x;
    out->position.y = (float)in->position.y;
    out->position.z = (float)in->position.z;
    out->rotationDeg.x = (float)in->rotation_deg.x;
    out->rotationDeg.y = (float)in->rotation_deg.y;
    out->rotationDeg.z = (float)in->rotation_deg.z;
    out->scale.x = (float)in->scale.x;
    out->scale.y = (float)in->scale.y;
    out->scale.z = (float)in->scale.z;
}

static bool Object3D_ApplyCoreRules(Object3D* object) {
    if (!object) return false;
    Transform3D_ToCore(&object->transform, &object->coreMeta.transform);
    CoreResult enforce = core_object_enforce_dimensional_rules(&object->coreMeta);
    if (enforce.code != CORE_OK) return false;
    Transform3D_FromCore(&object->coreMeta.transform, &object->transform);
    CoreResult validate = core_object_validate(&object->coreMeta);
    return validate.code == CORE_OK;
}

PlaneResizeHandleKind Layout_ResolveRectPrismResizeHandleForDrag(const Object3D* object,
                                                                 PlaneResizeHandleKind handle,
                                                                 Vec3 draggedWorldPoint) {
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return handle;
    if (!Layout_ObjectStore_ValidateObject(object)) return handle;

    PlaneResizeHandleSpec spec = {0};
    if (!PlaneResize_HandleSpec(handle, &spec)) return handle;

    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon) {
        return handle;
    }

    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    if (halfW <= kConstructionPlaneEpsilon || halfH <= kConstructionPlaneEpsilon) {
        return handle;
    }

    const Vec3 localDelta = Vec3_Sub(draggedWorldPoint, object->rectPrism.frame.origin);
    const float dragU = Vec3_Dot(localDelta, axisU);
    const float dragV = Vec3_Dot(localDelta, axisV);

    if (spec.affectsU) {
        const float anchorU = (spec.signU > 0.0f) ? -halfW : halfW;
        const bool crossedU = (spec.signU > 0.0f) ? (dragU < anchorU)
                                                  : (dragU > anchorU);
        if (crossedU) spec.signU = -spec.signU;
    }
    if (spec.affectsV) {
        const float anchorV = (spec.signV > 0.0f) ? -halfH : halfH;
        const bool crossedV = (spec.signV > 0.0f) ? (dragV < anchorV)
                                                  : (dragV > anchorV);
        if (crossedV) spec.signV = -spec.signV;
    }

    PlaneResizeHandleKind resolved = PlaneResize_HandleFromSpec(&spec);
    return (resolved == PLANE_RESIZE_HANDLE_NONE) ? handle : resolved;
}

bool Layout_RectPrismHandleAxisMask(PlaneResizeHandleKind handle,
                                    RectPrismHandleAxisMask* outMask) {
    if (!outMask) return false;
    PlaneResizeHandleSpec spec = {0};
    if (!PlaneResize_HandleSpec(handle, &spec)) return false;

    RectPrismHandleAxisMask mask = {0};
    if (spec.affectsU && spec.affectsV) {
        mask.allowU = true;
        mask.allowV = true;
        mask.allowN = true;
    } else if (spec.affectsU) {
        mask.allowU = true;
        mask.allowN = true;
    } else if (spec.affectsV) {
        mask.allowV = true;
        mask.allowN = true;
    } else {
        return false;
    }

    *outMask = mask;
    return true;
}

bool Layout_RectPrismAxisDirection_IsValid(RectPrismAxisDirection direction) {
    return direction >= RECT_PRISM_AXIS_DIR_POS_U &&
           direction <= RECT_PRISM_AXIS_DIR_NEG_N;
}

int Layout_RectPrismAxisDirection_Family(RectPrismAxisDirection direction) {
    if (!Layout_RectPrismAxisDirection_IsValid(direction)) return -1;
    if (direction == RECT_PRISM_AXIS_DIR_POS_U || direction == RECT_PRISM_AXIS_DIR_NEG_U) return 0;
    if (direction == RECT_PRISM_AXIS_DIR_POS_V || direction == RECT_PRISM_AXIS_DIR_NEG_V) return 1;
    return 2;
}

Vec3 Layout_RectPrismAxisDirection_WorldVector(const Object3D* object,
                                               RectPrismAxisDirection direction) {
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return (Vec3){ 0.0f, 0.0f, 0.0f };
    }

    Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);

    switch (direction) {
        case RECT_PRISM_AXIS_DIR_POS_U: return axisU;
        case RECT_PRISM_AXIS_DIR_NEG_U: return Vec3_Scale(axisU, -1.0f);
        case RECT_PRISM_AXIS_DIR_POS_V: return axisV;
        case RECT_PRISM_AXIS_DIR_NEG_V: return Vec3_Scale(axisV, -1.0f);
        case RECT_PRISM_AXIS_DIR_POS_N: return axisN;
        case RECT_PRISM_AXIS_DIR_NEG_N: return Vec3_Scale(axisN, -1.0f);
        default: return (Vec3){ 0.0f, 0.0f, 0.0f };
    }
}

bool Layout_RectPrismResizeHandle_IsValid(RectPrismResizeHandleKind handle) {
    RectPrismResizeHandleSpec3D spec = {0};
    return RectPrismResize3D_HandleSpec(handle, &spec);
}

bool Layout_RectPrismResizeHandleAxisMask(RectPrismResizeHandleKind handle,
                                          RectPrismHandleAxisMask* outMask) {
    if (!outMask) return false;
    RectPrismResizeHandleSpec3D spec = {0};
    if (!RectPrismResize3D_HandleSpec(handle, &spec)) return false;

    outMask->allowU = (spec.signU != 0);
    outMask->allowV = (spec.signV != 0);
    outMask->allowN = (spec.signN != 0);
    return outMask->allowU || outMask->allowV || outMask->allowN;
}

bool Layout_RectPrismResizeHandleWorldPoint(const Object3D* object,
                                            RectPrismResizeHandleKind handle,
                                            Vec3* outPoint) {
    if (!outPoint) return false;
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    RectPrismResizeHandleSpec3D spec = {0};
    if (!RectPrismResize3D_HandleSpec(handle, &spec)) return false;

    const Vec3 center = object->rectPrism.frame.origin;
    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    const float halfD = object->rectPrism.depth * 0.5f;

    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisN) <= kConstructionPlaneEpsilon ||
        halfW <= kRectPrismPrimitiveMinSize ||
        halfH <= kRectPrismPrimitiveMinSize ||
        halfD < 0.0f) {
        return false;
    }

    Vec3 point = center;
    point = Vec3_Add(point, Vec3_Scale(axisU, (float)spec.signU * halfW));
    point = Vec3_Add(point, Vec3_Scale(axisV, (float)spec.signV * halfH));
    point = Vec3_Add(point, Vec3_Scale(axisN, (float)spec.signN * halfD));
    *outPoint = point;
    return true;
}

RectPrismResizeHandleKind Layout_ResolveRectPrismResizeHandleFor3DDrag(
    const Object3D* object,
    RectPrismResizeHandleKind handle,
    Vec3 draggedWorldPoint) {
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return handle;
    if (!Layout_ObjectStore_ValidateObject(object)) return handle;

    RectPrismResizeHandleSpec3D spec = {0};
    if (!RectPrismResize3D_HandleSpec(handle, &spec)) return handle;

    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisN) <= kConstructionPlaneEpsilon) {
        return handle;
    }

    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    const float halfD = object->rectPrism.depth * 0.5f;
    if (halfW <= kConstructionPlaneEpsilon || halfH <= kConstructionPlaneEpsilon || halfD < 0.0f) {
        return handle;
    }

    const Vec3 localDelta = Vec3_Sub(draggedWorldPoint, object->rectPrism.frame.origin);
    const float dragU = Vec3_Dot(localDelta, axisU);
    const float dragV = Vec3_Dot(localDelta, axisV);
    const float dragN = Vec3_Dot(localDelta, axisN);

    int nextSignU = spec.signU;
    int nextSignV = spec.signV;
    int nextSignN = spec.signN;
    if (nextSignU != 0) {
        const float anchorU = (nextSignU > 0) ? -halfW : halfW;
        const bool crossedU = (nextSignU > 0) ? (dragU < anchorU) : (dragU > anchorU);
        if (crossedU) nextSignU = -nextSignU;
    }
    if (nextSignV != 0) {
        const float anchorV = (nextSignV > 0) ? -halfH : halfH;
        const bool crossedV = (nextSignV > 0) ? (dragV < anchorV) : (dragV > anchorV);
        if (crossedV) nextSignV = -nextSignV;
    }
    if (nextSignN != 0) {
        const float anchorN = (nextSignN > 0) ? -halfD : halfD;
        const bool crossedN = (nextSignN > 0) ? (dragN < anchorN) : (dragN > anchorN);
        if (crossedN) nextSignN = -nextSignN;
    }

    RectPrismResizeHandleKind resolved =
        RectPrismResize3D_HandleFromSigns(nextSignU, nextSignV, nextSignN);
    return (resolved == RECT_PRISM_RESIZE_HANDLE_NONE) ? handle : resolved;
}

bool Layout_ResizeRectPrismFrom3DHandle(Layout* layout,
                                        uint32_t objectId,
                                        RectPrismResizeHandleKind handle,
                                        Vec3 draggedWorldPoint,
                                        bool* outBoundsAdjusted) {
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    RectPrismResizeHandleSpec3D spec = {0};
    if (!RectPrismResize3D_HandleSpec(handle, &spec)) return false;

    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const Vec3 center = object->rectPrism.frame.origin;
    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    const float halfD = object->rectPrism.depth * 0.5f;
    const float minW = kRectPrismPrimitiveMinSize + 1e-5f;
    const float minH = kRectPrismPrimitiveMinSize + 1e-5f;
    const float minD = 0.0f;

    float minU = -halfW;
    float maxU = halfW;
    float minV = -halfH;
    float maxV = halfH;
    float minN = -halfD;
    float maxN = halfD;

    const Vec3 localDelta = Vec3_Sub(draggedWorldPoint, center);
    const float dragU = Vec3_Dot(localDelta, axisU);
    const float dragV = Vec3_Dot(localDelta, axisV);
    const float dragN = Vec3_Dot(localDelta, axisN);

    if (spec.signU != 0) {
        const float fixedU = (spec.signU > 0) ? -halfW : halfW;
        PlaneResize_ResolveAxisExtents(fixedU, dragU, minW, (float)spec.signU, &minU, &maxU);
    }
    if (spec.signV != 0) {
        const float fixedV = (spec.signV > 0) ? -halfH : halfH;
        PlaneResize_ResolveAxisExtents(fixedV, dragV, minH, (float)spec.signV, &minV, &maxV);
    }
    if (spec.signN != 0) {
        const float fixedN = (spec.signN > 0) ? -halfD : halfD;
        PlaneResize_ResolveAxisExtents(fixedN, dragN, minD, (float)spec.signN, &minN, &maxN);
    }

    float nextWidth = maxU - minU;
    float nextHeight = maxV - minV;
    float nextDepth = maxN - minN;
    if (nextWidth < minW) nextWidth = minW;
    if (nextHeight < minH) nextHeight = minH;
    if (nextDepth < 0.0f) nextDepth = 0.0f;

    Vec3 nextCenter = center;
    nextCenter = Vec3_Add(nextCenter, Vec3_Scale(axisU, (minU + maxU) * 0.5f));
    nextCenter = Vec3_Add(nextCenter, Vec3_Scale(axisV, (minV + maxV) * 0.5f));
    nextCenter = Vec3_Add(nextCenter, Vec3_Scale(axisN, (minN + maxN) * 0.5f));

    bool boundsAdjusted = false;
    if (object->rectPrism.lockToBounds) {
        bool centerClamped = false;
        if (!Layout_SceneBounds3D_ClampPoint(&layout->scene3d.bounds, &nextCenter, &centerClamped)) {
            return false;
        }
        if (centerClamped) boundsAdjusted = true;
        bool sizeAdjusted = false;
        if (!SceneBounds_ClampRectPrismExtents(&layout->scene3d.bounds,
                                               nextCenter,
                                               axisU,
                                               axisV,
                                               axisN,
                                               &nextWidth,
                                               &nextHeight,
                                               &nextDepth,
                                               &sizeAdjusted)) {
            return false;
        }
        if (sizeAdjusted) boundsAdjusted = true;
    }

    Object3D snapshot = *object;
    object->rectPrism.width = nextWidth;
    object->rectPrism.height = nextHeight;
    object->rectPrism.depth = nextDepth;
    object->transform.position = nextCenter;
    object->rectPrism.frame.origin = nextCenter;

    if (!Object3D_ApplyCoreRules(object)) {
        *object = snapshot;
        return false;
    }
    object->rectPrism.frame.origin = object->transform.position;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        *object = snapshot;
        return false;
    }

    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    Global_FlagLayoutChanged();
    return true;
}

bool Layout_RectPrismSelectFaceForView(const Object3D* object,
                                       ViewPlane plane,
                                       const FreeViewCamera* camera,
                                       bool* outUseTopFace) {
    if (!outUseTopFace) return false;
    *outUseTopFace = true;
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    const Vec3 center = object->rectPrism.frame.origin;
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const float halfD = object->rectPrism.depth * 0.5f;
    if (Vec3_Length(axisN) <= kConstructionPlaneEpsilon ||
        halfD < 0.0f) {
        return false;
    }

    Vec3 bottomCenter = Vec3_Add(center, Vec3_Scale(axisN, -halfD));
    Vec3 topCenter = Vec3_Add(center, Vec3_Scale(axisN, halfD));
    const float topDepth = Layout_DepthDistanceForViewPoint(topCenter, plane, camera);
    const float bottomDepth = Layout_DepthDistanceForViewPoint(bottomCenter, plane, camera);
    *outUseTopFace = (topDepth <= bottomDepth);
    return true;
}

bool Layout_RectPrismHandleWorldPoint(const Object3D* object,
                                      PlaneResizeHandleKind handle,
                                      bool useTopFace,
                                      Vec3* outPoint) {
    if (!outPoint) return false;
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    PlaneResizeHandleSpec spec = {0};
    if (!PlaneResize_HandleSpec(handle, &spec)) return false;

    const Vec3 center = object->rectPrism.frame.origin;
    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    const float halfD = object->rectPrism.depth * 0.5f;

    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisN) <= kConstructionPlaneEpsilon ||
        halfW <= kRectPrismPrimitiveMinSize ||
        halfH <= kRectPrismPrimitiveMinSize ||
        halfD < 0.0f) {
        return false;
    }

    float localU = 0.0f;
    float localV = 0.0f;
    if (spec.affectsU && spec.affectsV) {
        localU = spec.signU * halfW;
        localV = spec.signV * halfH;
    } else if (spec.affectsU) {
        localU = spec.signU * halfW;
        localV = 0.0f;
    } else if (spec.affectsV) {
        localU = 0.0f;
        localV = spec.signV * halfH;
    } else {
        return false;
    }

    const float signN = useTopFace ? 1.0f : -1.0f;
    const float localN = signN * halfD;
    Vec3 point = center;
    point = Vec3_Add(point, Vec3_Scale(axisU, localU));
    point = Vec3_Add(point, Vec3_Scale(axisV, localV));
    point = Vec3_Add(point, Vec3_Scale(axisN, localN));
    *outPoint = point;
    return true;
}

bool Layout_ResizeRectPrismPrimitiveFromHandle(Layout* layout,
                                               uint32_t objectId,
                                               PlaneResizeHandleKind handle,
                                               Vec3 draggedWorldPoint,
                                               bool* outBoundsAdjusted) {
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    PlaneResizeHandleSpec spec = {0};
    if (!PlaneResize_HandleSpec(handle, &spec)) return false;

    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const Vec3 center = object->rectPrism.frame.origin;
    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    const float minW = kRectPrismPrimitiveMinSize + 1e-5f;
    const float minH = kRectPrismPrimitiveMinSize + 1e-5f;

    float minU = -halfW;
    float maxU = halfW;
    float minV = -halfH;
    float maxV = halfH;

    const Vec3 localDelta = Vec3_Sub(draggedWorldPoint, center);
    const float dragU = Vec3_Dot(localDelta, axisU);
    const float dragV = Vec3_Dot(localDelta, axisV);

    if (spec.affectsU) {
        const float fixedU = (spec.signU > 0.0f) ? -halfW : halfW;
        PlaneResize_ResolveAxisExtents(fixedU, dragU, minW, spec.signU, &minU, &maxU);
    }
    if (spec.affectsV) {
        const float fixedV = (spec.signV > 0.0f) ? -halfH : halfH;
        PlaneResize_ResolveAxisExtents(fixedV, dragV, minH, spec.signV, &minV, &maxV);
    }

    float nextWidth = maxU - minU;
    float nextHeight = maxV - minV;
    if (nextWidth < minW) nextWidth = minW;
    if (nextHeight < minH) nextHeight = minH;

    Vec3 nextCenter = center;
    nextCenter = Vec3_Add(nextCenter, Vec3_Scale(axisU, (minU + maxU) * 0.5f));
    nextCenter = Vec3_Add(nextCenter, Vec3_Scale(axisV, (minV + maxV) * 0.5f));
    float nextDepth = object->rectPrism.depth;

    bool boundsAdjusted = false;
    if (object->rectPrism.lockToBounds) {
        bool centerClamped = false;
        if (!Layout_SceneBounds3D_ClampPoint(&layout->scene3d.bounds, &nextCenter, &centerClamped)) {
            return false;
        }
        if (centerClamped) boundsAdjusted = true;
        bool sizeAdjusted = false;
        if (!SceneBounds_ClampRectPrismExtents(&layout->scene3d.bounds,
                                               nextCenter,
                                               axisU,
                                               axisV,
                                               axisN,
                                               &nextWidth,
                                               &nextHeight,
                                               &nextDepth,
                                               &sizeAdjusted)) {
            return false;
        }
        if (sizeAdjusted) boundsAdjusted = true;
    }

    Object3D snapshot = *object;
    object->rectPrism.width = nextWidth;
    object->rectPrism.height = nextHeight;
    object->rectPrism.depth = nextDepth;
    object->transform.position = nextCenter;
    object->rectPrism.frame.origin = nextCenter;

    if (!Object3D_ApplyCoreRules(object)) {
        *object = snapshot;
        return false;
    }
    object->rectPrism.frame.origin = object->transform.position;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        *object = snapshot;
        return false;
    }

    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    Global_FlagLayoutChanged();
    return true;
}

bool Layout_ResizeRectPrismDepthFromFaceHandle(Layout* layout,
                                               uint32_t objectId,
                                               PlaneResizeHandleKind handle,
                                               bool useTopFace,
                                               Vec3 draggedWorldPoint,
                                               bool* outBoundsAdjusted) {
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    PlaneResizeHandleSpec spec = {0};
    if (!PlaneResize_HandleSpec(handle, &spec)) return false;

    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const Vec3 center = object->rectPrism.frame.origin;
    const float halfD = object->rectPrism.depth * 0.5f;
    const float minD = 0.0f;
    const float signN = useTopFace ? 1.0f : -1.0f;

    if (Vec3_Length(axisN) <= kConstructionPlaneEpsilon ||
        halfD < 0.0f) {
        return false;
    }

    float minN = -halfD;
    float maxN = halfD;
    const Vec3 localDelta = Vec3_Sub(draggedWorldPoint, center);
    const float dragN = Vec3_Dot(localDelta, axisN);
    const float fixedN = (signN > 0.0f) ? -halfD : halfD;
    PlaneResize_ResolveAxisExtents(fixedN, dragN, minD, signN, &minN, &maxN);

    float nextDepth = maxN - minN;
    if (nextDepth < 0.0f) nextDepth = 0.0f;
    Vec3 nextCenter = Vec3_Add(center, Vec3_Scale(axisN, (minN + maxN) * 0.5f));
    float nextWidth = object->rectPrism.width;
    float nextHeight = object->rectPrism.height;

    bool boundsAdjusted = false;
    if (object->rectPrism.lockToBounds) {
        bool centerClamped = false;
        if (!Layout_SceneBounds3D_ClampPoint(&layout->scene3d.bounds, &nextCenter, &centerClamped)) {
            return false;
        }
        if (centerClamped) boundsAdjusted = true;
        bool sizeAdjusted = false;
        if (!SceneBounds_ClampRectPrismExtents(&layout->scene3d.bounds,
                                               nextCenter,
                                               axisU,
                                               axisV,
                                               axisN,
                                               &nextWidth,
                                               &nextHeight,
                                               &nextDepth,
                                               &sizeAdjusted)) {
            return false;
        }
        if (sizeAdjusted) boundsAdjusted = true;
    }

    Object3D snapshot = *object;
    object->rectPrism.width = nextWidth;
    object->rectPrism.height = nextHeight;
    object->rectPrism.depth = nextDepth;
    object->transform.position = nextCenter;
    object->rectPrism.frame.origin = nextCenter;

    if (!Object3D_ApplyCoreRules(object)) {
        *object = snapshot;
        return false;
    }
    object->rectPrism.frame.origin = object->transform.position;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        *object = snapshot;
        return false;
    }

    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    Global_FlagLayoutChanged();
    return true;
}

bool Layout_SetRectPrismDimensions(Layout* layout,
                                   uint32_t objectId,
                                   float width,
                                   float height,
                                   float depth,
                                   bool* outBoundsAdjusted) {
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout) return false;
    if (!isfinite(width) || !isfinite(height) || !isfinite(depth)) return false;
    if (width <= kRectPrismPrimitiveMinSize ||
        height <= kRectPrismPrimitiveMinSize ||
        depth < 0.0f) {
        return false;
    }

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    const Vec3 center = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisN) <= kConstructionPlaneEpsilon) {
        return false;
    }

    float nextWidth = width;
    float nextHeight = height;
    float nextDepth = depth;
    bool boundsAdjusted = false;
    if (object->rectPrism.lockToBounds) {
        bool sizeAdjusted = false;
        if (!SceneBounds_ClampRectPrismExtents(&layout->scene3d.bounds,
                                               center,
                                               axisU,
                                               axisV,
                                               axisN,
                                               &nextWidth,
                                               &nextHeight,
                                               &nextDepth,
                                               &sizeAdjusted)) {
            return false;
        }
        if (sizeAdjusted) boundsAdjusted = true;
    }

    if (nextWidth <= kRectPrismPrimitiveMinSize ||
        nextHeight <= kRectPrismPrimitiveMinSize ||
        nextDepth < 0.0f) {
        return false;
    }

    Object3D snapshot = *object;
    object->rectPrism.width = nextWidth;
    object->rectPrism.height = nextHeight;
    object->rectPrism.depth = nextDepth;

    if (!Object3D_ApplyCoreRules(object)) {
        *object = snapshot;
        return false;
    }
    object->rectPrism.frame.origin = object->transform.position;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        *object = snapshot;
        return false;
    }

    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    Global_FlagLayoutChanged();
    return true;
}
