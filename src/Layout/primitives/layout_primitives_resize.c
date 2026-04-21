// src/Layout/layout_primitives_resize.c
#include "Layout/layout.h"
#include "Core/global_state.h"

#include <math.h>
#include <float.h>
static const float kConstructionPlaneEpsilon = 1e-4f;
static const float kPlanePrimitiveMinSize = 1e-3f;

static float ClampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

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

static bool SceneBounds_ClampPlaneExtents(const SceneBounds3D* bounds,
                                          Vec3 center,
                                          Vec3 axisU,
                                          Vec3 axisV,
                                          float* width,
                                          float* height,
                                          bool* outAdjusted) {
    if (outAdjusted) *outAdjusted = false;
    if (!bounds || !width || !height) return false;
    if (!bounds->enabled) return true;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;

    const float uPos = SceneBounds_DistanceToBoundary(bounds, center, axisU);
    const float uNeg = SceneBounds_DistanceToBoundary(bounds, center, Vec3_Scale(axisU, -1.0f));
    const float vPos = SceneBounds_DistanceToBoundary(bounds, center, axisV);
    const float vNeg = SceneBounds_DistanceToBoundary(bounds, center, Vec3_Scale(axisV, -1.0f));
    const float maxHalfU = fminf(uPos, uNeg);
    const float maxHalfV = fminf(vPos, vNeg);
    if (maxHalfU <= kPlanePrimitiveMinSize || maxHalfV <= kPlanePrimitiveMinSize) {
        return false;
    }

    float nextW = *width;
    float nextH = *height;
    const float maxW = maxHalfU * 2.0f;
    const float maxH = maxHalfV * 2.0f;
    if (nextW > maxW) nextW = maxW;
    if (nextH > maxH) nextH = maxH;
    if (nextW < kPlanePrimitiveMinSize) nextW = kPlanePrimitiveMinSize;
    if (nextH < kPlanePrimitiveMinSize) nextH = kPlanePrimitiveMinSize;

    if (outAdjusted) {
        *outAdjusted = fabsf(nextW - *width) > 1e-5f ||
                       fabsf(nextH - *height) > 1e-5f;
    }

    *width = nextW;
    *height = nextH;
    return true;
}


static bool Object3D_IsBoundsLocked(const Object3D* object) {
    if (!object) return false;
    switch (object->kind) {
        case OBJECT3D_KIND_PLANE: return object->plane.lockToBounds;
        case OBJECT3D_KIND_RECT_PRISM: return object->rectPrism.lockToBounds;
        case OBJECT3D_KIND_UNKNOWN:
        default: return false;
    }
}

static bool Object3D_ComputeTranslationHalfExtents(const Object3D* object, Vec3* outHalfExtents) {
    if (!object || !outHalfExtents) return false;

    Vec3 axisU = {0};
    Vec3 axisV = {0};
    Vec3 axisN = {0};
    float halfU = 0.0f;
    float halfV = 0.0f;
    float halfN = 0.0f;

    switch (object->kind) {
        case OBJECT3D_KIND_PLANE:
            axisU = Vec3_Normalize(object->plane.frame.axisU);
            axisV = Vec3_Normalize(object->plane.frame.axisV);
            if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
                Vec3_Length(axisV) <= kConstructionPlaneEpsilon) {
                return false;
            }
            halfU = object->plane.width * 0.5f;
            halfV = object->plane.height * 0.5f;
            break;
        case OBJECT3D_KIND_RECT_PRISM:
            axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
            axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
            axisN = Vec3_Normalize(object->rectPrism.frame.normal);
            if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
                Vec3_Length(axisV) <= kConstructionPlaneEpsilon ||
                Vec3_Length(axisN) <= kConstructionPlaneEpsilon) {
                return false;
            }
            halfU = object->rectPrism.width * 0.5f;
            halfV = object->rectPrism.height * 0.5f;
            halfN = object->rectPrism.depth * 0.5f;
            break;
        case OBJECT3D_KIND_UNKNOWN:
        default:
            return false;
    }

    outHalfExtents->x = fabsf(axisU.x) * halfU +
                        fabsf(axisV.x) * halfV +
                        fabsf(axisN.x) * halfN;
    outHalfExtents->y = fabsf(axisU.y) * halfU +
                        fabsf(axisV.y) * halfV +
                        fabsf(axisN.y) * halfN;
    outHalfExtents->z = fabsf(axisU.z) * halfU +
                        fabsf(axisV.z) * halfV +
                        fabsf(axisN.z) * halfN;
    return true;
}

static bool SceneBounds_ClampObjectCenterTranslation(const SceneBounds3D* bounds,
                                                     const Object3D* object,
                                                     Vec3* inOutCenter,
                                                     bool* outAdjusted) {
    if (outAdjusted) *outAdjusted = false;
    if (!bounds || !object || !inOutCenter) return false;
    if (!bounds->enabled) return true;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;

    Vec3 halfExtents = {0};
    if (!Object3D_ComputeTranslationHalfExtents(object, &halfExtents)) return false;

    bool adjusted = false;

    float minAllowed = bounds->min.x + halfExtents.x;
    float maxAllowed = bounds->max.x - halfExtents.x;
    if (minAllowed <= maxAllowed) {
        const float clamped = ClampFloat(inOutCenter->x, minAllowed, maxAllowed);
        if (fabsf(clamped - inOutCenter->x) > 1e-5f) adjusted = true;
        inOutCenter->x = clamped;
    } else {
        const float centered = (bounds->min.x + bounds->max.x) * 0.5f;
        if (fabsf(centered - inOutCenter->x) > 1e-5f) adjusted = true;
        inOutCenter->x = centered;
    }

    minAllowed = bounds->min.y + halfExtents.y;
    maxAllowed = bounds->max.y - halfExtents.y;
    if (minAllowed <= maxAllowed) {
        const float clamped = ClampFloat(inOutCenter->y, minAllowed, maxAllowed);
        if (fabsf(clamped - inOutCenter->y) > 1e-5f) adjusted = true;
        inOutCenter->y = clamped;
    } else {
        const float centered = (bounds->min.y + bounds->max.y) * 0.5f;
        if (fabsf(centered - inOutCenter->y) > 1e-5f) adjusted = true;
        inOutCenter->y = centered;
    }

    minAllowed = bounds->min.z + halfExtents.z;
    maxAllowed = bounds->max.z - halfExtents.z;
    if (minAllowed <= maxAllowed) {
        const float clamped = ClampFloat(inOutCenter->z, minAllowed, maxAllowed);
        if (fabsf(clamped - inOutCenter->z) > 1e-5f) adjusted = true;
        inOutCenter->z = clamped;
    } else {
        const float centered = (bounds->min.z + bounds->max.z) * 0.5f;
        if (fabsf(centered - inOutCenter->z) > 1e-5f) adjusted = true;
        inOutCenter->z = centered;
    }

    if (outAdjusted) *outAdjusted = adjusted;
    return true;
}

static Vec3 Vec3_RotateAroundAxis(Vec3 vector, Vec3 axisUnit, float angleRad) {
    const float cosTheta = cosf(angleRad);
    const float sinTheta = sinf(angleRad);
    const float dot = Vec3_Dot(axisUnit, vector);
    Vec3 termA = Vec3_Scale(vector, cosTheta);
    Vec3 termB = Vec3_Scale(Vec3_Cross(axisUnit, vector), sinTheta);
    Vec3 termC = Vec3_Scale(axisUnit, dot * (1.0f - cosTheta));
    return Vec3_Add(Vec3_Add(termA, termB), termC);
}

static bool PlaneFrame3_RotateAroundAxis(const PlaneFrame3* input,
                                         Vec3 axisWorld,
                                         float angleDeg,
                                         PlaneFrame3* output) {
    if (!input || !output) return false;
    if (!isfinite(angleDeg)) return false;
    if (!isfinite(axisWorld.x) || !isfinite(axisWorld.y) || !isfinite(axisWorld.z)) return false;
    const Vec3 axisUnit = Vec3_Normalize(axisWorld);
    if (Vec3_Length(axisUnit) <= kConstructionPlaneEpsilon) return false;

    const float angleRad = DegToRad(angleDeg);
    Vec3 axisU = Vec3_RotateAroundAxis(input->axisU, axisUnit, angleRad);
    Vec3 axisV = Vec3_RotateAroundAxis(input->axisV, axisUnit, angleRad);
    Vec3 axisN = Vec3_RotateAroundAxis(input->normal, axisUnit, angleRad);

    axisU = Vec3_Normalize(axisU);
    axisV = Vec3_Normalize(axisV);
    axisN = Vec3_Normalize(axisN);
    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisN) <= kConstructionPlaneEpsilon) {
        return false;
    }

    Vec3 crossUV = Vec3_Cross(axisU, axisV);
    if (Vec3_Length(crossUV) <= kConstructionPlaneEpsilon) return false;
    crossUV = Vec3_Normalize(crossUV);
    if (Vec3_Dot(crossUV, axisN) < 0.0f) {
        crossUV = Vec3_Scale(crossUV, -1.0f);
    }

    // Re-orthonormalize with explicit order: U -> N -> V.
    axisN = crossUV;
    axisV = Vec3_Normalize(Vec3_Cross(axisN, axisU));
    axisU = Vec3_Normalize(Vec3_Cross(axisV, axisN));
    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisN) <= kConstructionPlaneEpsilon) {
        return false;
    }

    *output = *input;
    output->axisU = axisU;
    output->axisV = axisV;
    output->normal = axisN;
    return true;
}

static Vec3 RotationDeg_ApplyWorldAxisDelta(Vec3 baseRotationDeg, Vec3 axisWorld, float angleDeg) {
    const float ax = fabsf(axisWorld.x);
    const float ay = fabsf(axisWorld.y);
    const float az = fabsf(axisWorld.z);
    if (ax >= ay && ax >= az) {
        baseRotationDeg.x = Angle_NormalizeSignedDeg(baseRotationDeg.x +
                                                     (axisWorld.x >= 0.0f ? angleDeg : -angleDeg));
    } else if (ay >= ax && ay >= az) {
        baseRotationDeg.y = Angle_NormalizeSignedDeg(baseRotationDeg.y +
                                                     (axisWorld.y >= 0.0f ? angleDeg : -angleDeg));
    } else {
        baseRotationDeg.z = Angle_NormalizeSignedDeg(baseRotationDeg.z +
                                                     (axisWorld.z >= 0.0f ? angleDeg : -angleDeg));
    }
    return baseRotationDeg;
}

static bool SceneBounds_ObjectFits(const SceneBounds3D* bounds, const Object3D* object) {
    if (!bounds || !object) return false;
    if (!bounds->enabled) return true;
    if (!Layout_SceneBounds3D_IsValid(bounds)) return false;

    if (object->kind == OBJECT3D_KIND_PLANE) {
        Vec3 corners[4] = {0};
        if (!Layout_Object3D_ComputePlaneCorners(object, corners)) return false;
        for (size_t i = 0; i < 4; ++i) {
            if (corners[i].x < bounds->min.x - 1e-4f || corners[i].x > bounds->max.x + 1e-4f) return false;
            if (corners[i].y < bounds->min.y - 1e-4f || corners[i].y > bounds->max.y + 1e-4f) return false;
            if (corners[i].z < bounds->min.z - 1e-4f || corners[i].z > bounds->max.z + 1e-4f) return false;
        }
        return true;
    }

    if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        Vec3 corners[8] = {0};
        if (!Layout_Object3D_ComputeRectPrismCorners(object, corners)) return false;
        for (size_t i = 0; i < 8; ++i) {
            if (corners[i].x < bounds->min.x - 1e-4f || corners[i].x > bounds->max.x + 1e-4f) return false;
            if (corners[i].y < bounds->min.y - 1e-4f || corners[i].y > bounds->max.y + 1e-4f) return false;
            if (corners[i].z < bounds->min.z - 1e-4f || corners[i].z > bounds->max.z + 1e-4f) return false;
        }
        return true;
    }

    return false;
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

PlaneResizeHandleKind Layout_ResolvePlaneResizeHandleForDrag(const Object3D* object,
                                                             PlaneResizeHandleKind handle,
                                                             Vec3 draggedWorldPoint) {
    if (!object || object->kind != OBJECT3D_KIND_PLANE) return handle;
    if (!Layout_ObjectStore_ValidateObject(object)) return handle;

    PlaneResizeHandleSpec spec = {0};
    if (!PlaneResize_HandleSpec(handle, &spec)) return handle;

    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->plane.frame.axisV);
    if (Vec3_Length(axisU) <= kConstructionPlaneEpsilon ||
        Vec3_Length(axisV) <= kConstructionPlaneEpsilon) {
        return handle;
    }

    const float halfW = object->plane.width * 0.5f;
    const float halfH = object->plane.height * 0.5f;
    if (halfW <= kConstructionPlaneEpsilon || halfH <= kConstructionPlaneEpsilon) {
        return handle;
    }

    const Vec3 localDelta = Vec3_Sub(draggedWorldPoint, object->plane.frame.origin);
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

bool Layout_ResizePlanePrimitiveFromHandle(Layout* layout,
                                           uint32_t objectId,
                                           PlaneResizeHandleKind handle,
                                           Vec3 draggedWorldPoint,
                                           bool* outBoundsAdjusted) {
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_PLANE) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    PlaneResizeHandleSpec spec = {0};
    if (!PlaneResize_HandleSpec(handle, &spec)) return false;

    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->plane.frame.axisV);
    const Vec3 center = object->plane.frame.origin;
    const float halfW = object->plane.width * 0.5f;
    const float halfH = object->plane.height * 0.5f;
    const float minW = kPlanePrimitiveMinSize + 1e-5f;
    const float minH = kPlanePrimitiveMinSize + 1e-5f;

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

    bool boundsAdjusted = false;
    if (object->plane.lockToBounds) {
        bool centerClamped = false;
        if (!Layout_SceneBounds3D_ClampPoint(&layout->scene3d.bounds, &nextCenter, &centerClamped)) {
            return false;
        }
        if (centerClamped) boundsAdjusted = true;
        bool sizeAdjusted = false;
        if (!SceneBounds_ClampPlaneExtents(&layout->scene3d.bounds,
                                           nextCenter,
                                           axisU,
                                           axisV,
                                           &nextWidth,
                                           &nextHeight,
                                           &sizeAdjusted)) {
            return false;
        }
        if (sizeAdjusted) boundsAdjusted = true;
    }

    Object3D snapshot = *object;
    object->plane.width = nextWidth;
    object->plane.height = nextHeight;
    object->transform.position = nextCenter;
    object->plane.frame.origin = nextCenter;

    if (!Object3D_ApplyCoreRules(object)) {
        *object = snapshot;
        return false;
    }
    object->plane.frame.origin = object->transform.position;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        *object = snapshot;
        return false;
    }

    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    Global_FlagLayoutChanged();
    return true;
}
bool Layout_SetObject3DPosition(Layout* layout,
                                uint32_t objectId,
                                Vec3 position,
                                bool* outBoundsAdjusted) {
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout || objectId == 0u) return false;
    if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z)) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object) return false;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return false;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    Vec3 nextCenter = position;
    bool boundsAdjusted = false;
    if (Object3D_IsBoundsLocked(object)) {
        if (!SceneBounds_ClampObjectCenterTranslation(&layout->scene3d.bounds,
                                                      object,
                                                      &nextCenter,
                                                      &boundsAdjusted)) {
            return false;
        }
    }

    Object3D snapshot = *object;
    object->transform.position = nextCenter;
    if (object->kind == OBJECT3D_KIND_PLANE) {
        object->plane.frame.origin = nextCenter;
    } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        object->rectPrism.frame.origin = nextCenter;
    }

    if (!Object3D_ApplyCoreRules(object)) {
        *object = snapshot;
        return false;
    }
    if (object->kind == OBJECT3D_KIND_PLANE) {
        object->plane.frame.origin = object->transform.position;
    } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        object->rectPrism.frame.origin = object->transform.position;
    }

    if (!Layout_ObjectStore_ValidateObject(object)) {
        *object = snapshot;
        return false;
    }

    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    Global_FlagLayoutChanged();
    return true;
}

bool Layout_RotateObject3D(Layout* layout,
                           uint32_t objectId,
                           Vec3 axisWorld,
                           float angleDeg,
                           const Object3D* baselineObject,
                           bool* outBoundsAdjusted) {
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout || objectId == 0u) return false;
    if (!isfinite(axisWorld.x) || !isfinite(axisWorld.y) || !isfinite(axisWorld.z)) return false;
    if (!isfinite(angleDeg)) return false;
    if (Vec3_Length(axisWorld) <= kConstructionPlaneEpsilon) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object) return false;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return false;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    const Object3D* baseline = object;
    if (baselineObject &&
        baselineObject->objectId == object->objectId &&
        baselineObject->kind == object->kind &&
        !baselineObject->isDeleted) {
        baseline = baselineObject;
    }

    Object3D snapshot = *object;
    Object3D next = *object;
    bool boundsAdjusted = false;

    if (object->kind == OBJECT3D_KIND_PLANE) {
        PlaneFrame3 rotatedFrame = {0};
        if (!PlaneFrame3_RotateAroundAxis(&baseline->plane.frame,
                                          axisWorld,
                                          angleDeg,
                                          &rotatedFrame)) {
            return false;
        }
        rotatedFrame.origin = next.transform.position;
        next.plane.frame = rotatedFrame;
    } else {
        PlaneFrame3 rotatedFrame = {0};
        if (!PlaneFrame3_RotateAroundAxis(&baseline->rectPrism.frame,
                                          axisWorld,
                                          angleDeg,
                                          &rotatedFrame)) {
            return false;
        }
        rotatedFrame.origin = next.transform.position;
        next.rectPrism.frame = rotatedFrame;
    }

    next.transform.rotationDeg =
        RotationDeg_ApplyWorldAxisDelta(baseline->transform.rotationDeg, axisWorld, angleDeg);

    if (Object3D_IsBoundsLocked(&next)) {
        Vec3 center = next.transform.position;
        if (!SceneBounds_ClampObjectCenterTranslation(&layout->scene3d.bounds,
                                                      &next,
                                                      &center,
                                                      &boundsAdjusted)) {
            return false;
        }
        next.transform.position = center;
        if (next.kind == OBJECT3D_KIND_PLANE) {
            next.plane.frame.origin = center;
        } else {
            next.rectPrism.frame.origin = center;
        }
    }

    *object = next;
    if (!Object3D_ApplyCoreRules(object)) {
        *object = snapshot;
        return false;
    }

    if (object->kind == OBJECT3D_KIND_PLANE) {
        object->plane.frame.origin = object->transform.position;
    } else {
        object->rectPrism.frame.origin = object->transform.position;
    }

    if (!Layout_ObjectStore_ValidateObject(object)) {
        *object = snapshot;
        return false;
    }

    if (Object3D_IsBoundsLocked(object) && !SceneBounds_ObjectFits(&layout->scene3d.bounds, object)) {
        *object = snapshot;
        return false;
    }

    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    Global_FlagLayoutChanged();
    return true;
}
