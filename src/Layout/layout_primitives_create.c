#include "layout.h"
#include "Core/global_state.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float kConstructionPlaneEpsilon = 1e-4f;
static const float kPlanePrimitiveDefaultSize = 4.0f;
static const float kPlanePrimitiveMinSize = 1e-3f;
static const float kRectPrismPrimitiveDefaultSize = 4.0f;
static const float kRectPrismPrimitiveMinSize = 1e-3f;

static const char* Object3DKind_DefaultType(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "plane_primitive";
        case OBJECT3D_KIND_RECT_PRISM: return "rect_prism_primitive";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "unknown_primitive";
    }
}

static CoreObjectPlane CorePlane_FromViewAxis(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return CORE_OBJECT_PLANE_YZ;
        case VIEW_PLANE_XZ: return CORE_OBJECT_PLANE_XZ;
        case VIEW_PLANE_XY:
        default: return CORE_OBJECT_PLANE_XY;
    }
}

static CoreObjectPlane CorePlane_FromNormal(Vec3 normal) {
    const Vec3 n = Vec3_Normalize(normal);
    const float ax = fabsf(n.x);
    const float ay = fabsf(n.y);
    const float az = fabsf(n.z);
    if (ax >= ay && ax >= az) return CORE_OBJECT_PLANE_YZ;
    if (ay >= ax && ay >= az) return CORE_OBJECT_PLANE_XZ;
    return CORE_OBJECT_PLANE_XY;
}

static bool PlaneFrame_IsValid(const PlaneFrame3* frame) {
    if (!frame) return false;
    const float uLen = Vec3_Length(frame->axisU);
    const float vLen = Vec3_Length(frame->axisV);
    const float nLen = Vec3_Length(frame->normal);
    if (uLen <= kConstructionPlaneEpsilon ||
        vLen <= kConstructionPlaneEpsilon ||
        nLen <= kConstructionPlaneEpsilon) {
        return false;
    }
    Vec3 cross = Vec3_Cross(frame->axisU, frame->axisV);
    if (Vec3_Length(cross) <= kConstructionPlaneEpsilon) {
        return false;
    }
    Vec3 crossN = Vec3_Normalize(cross);
    Vec3 normalN = Vec3_Normalize(frame->normal);
    return fabsf(Vec3_Dot(crossN, normalN)) >= 0.90f;
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

Transform3D Layout_Transform3D_Default(void) {
    return (Transform3D){
        .position = { 0.0f, 0.0f, 0.0f },
        .rotationDeg = { 0.0f, 0.0f, 0.0f },
        .scale = { 1.0f, 1.0f, 1.0f }
    };
}

void Layout_PlanePrimitiveCreateParams_SetDefaults(PlanePrimitiveCreateParams* params) {
    if (!params) return;
    memset(params, 0, sizeof(*params));
    params->width = kPlanePrimitiveDefaultSize;
    params->height = kPlanePrimitiveDefaultSize;
    params->lockToConstructionPlane = true;
    params->lockToBounds = true;
}

void Layout_RectPrismPrimitiveCreateParams_SetDefaults(RectPrismPrimitiveCreateParams* params) {
    if (!params) return;
    memset(params, 0, sizeof(*params));
    params->width = kRectPrismPrimitiveDefaultSize;
    params->height = kRectPrismPrimitiveDefaultSize;
    params->depth = kRectPrismPrimitiveDefaultSize;
    params->lockToConstructionPlane = true;
    params->lockToBounds = true;
}

uint32_t Layout_ObjectStore_Create(LayoutObjectStore* store,
                                   Object3DKind kind,
                                   const Transform3D* transform,
                                   const char* objectType,
                                   CoreObjectDimensionalMode dimensionalMode,
                                   CoreObjectPlane lockedPlane) {
    if (!store) return 0u;
    if (kind == OBJECT3D_KIND_UNKNOWN) return 0u;

    const Transform3D resolvedTransform = transform ? *transform : Layout_Transform3D_Default();
    const char* resolvedType = (objectType && objectType[0]) ? objectType : Object3DKind_DefaultType(kind);

    Object3D candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.objectId = store->nextObjectId;
    candidate.kind = kind;
    candidate.transform = resolvedTransform;
    candidate.plane.width = kPlanePrimitiveDefaultSize;
    candidate.plane.height = kPlanePrimitiveDefaultSize;
    candidate.plane.lockToConstructionPlane = true;
    candidate.plane.lockToBounds = true;
    {
        const ViewPlane objectPlane = { .axis = VIEW_PLANE_XY, .offset = resolvedTransform.position.z };
        Plane3 p = Plane3_FromViewPlane(objectPlane);
        candidate.plane.frame = PlaneFrame3_FromPlane(p, resolvedTransform.position);
        candidate.plane.frame.origin = resolvedTransform.position;
    }
    candidate.rectPrism.width = kRectPrismPrimitiveDefaultSize;
    candidate.rectPrism.height = kRectPrismPrimitiveDefaultSize;
    candidate.rectPrism.depth = kRectPrismPrimitiveDefaultSize;
    candidate.rectPrism.lockToConstructionPlane = false;
    candidate.rectPrism.lockToBounds = true;
    {
        const ViewPlane objectPlane = { .axis = VIEW_PLANE_XY, .offset = resolvedTransform.position.z };
        Plane3 p = Plane3_FromViewPlane(objectPlane);
        candidate.rectPrism.frame = PlaneFrame3_FromPlane(p, resolvedTransform.position);
        candidate.rectPrism.frame.origin = resolvedTransform.position;
    }
    candidate.isDeleted = false;
    core_object_init(&candidate.coreMeta);

    char objectIdBuf[64];
    snprintf(objectIdBuf, sizeof(objectIdBuf), "obj3d_%u", candidate.objectId);
    if (core_object_set_identity(&candidate.coreMeta, objectIdBuf, resolvedType).code != CORE_OK) {
        return 0u;
    }

    if (dimensionalMode == CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) {
        if (core_object_set_plane_lock(&candidate.coreMeta, lockedPlane).code != CORE_OK) {
            return 0u;
        }
    } else if (dimensionalMode == CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) {
        if (core_object_promote_to_full_3d(&candidate.coreMeta).code != CORE_OK) {
            return 0u;
        }
    } else {
        return 0u;
    }

    if (!Object3D_ApplyCoreRules(&candidate)) return 0u;
    if (candidate.kind == OBJECT3D_KIND_PLANE) {
        candidate.plane.frame.origin = candidate.transform.position;
    } else if (candidate.kind == OBJECT3D_KIND_RECT_PRISM) {
        candidate.rectPrism.frame.origin = candidate.transform.position;
    }

    Object3D* resized = realloc(store->items, sizeof(Object3D) * (store->count + 1u));
    if (!resized) return 0u;
    store->items = resized;
    store->items[store->count++] = candidate;
    store->nextObjectId += 1u;
    Global_FlagLayoutChanged();
    return candidate.objectId;
}

bool Layout_CreatePlanePrimitive(Layout* layout,
                                 const PlanePrimitiveCreateParams* params,
                                 uint32_t* outObjectId,
                                 bool* outBoundsAdjusted) {
    if (outObjectId) *outObjectId = 0u;
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout) return false;
    if (!Layout_ConstructionPlane3D_IsValid(&layout->scene3d.constructionPlane)) return false;
    if (layout->scene3d.bounds.enabled && !Layout_SceneBounds3D_IsValid(&layout->scene3d.bounds)) return false;

    PlanePrimitiveCreateParams resolved;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&resolved);
    if (params) {
        if (params->width > 0.0f) resolved.width = params->width;
        if (params->height > 0.0f) resolved.height = params->height;
        resolved.lockToConstructionPlane = params->lockToConstructionPlane;
        resolved.lockToBounds = params->lockToBounds;
        resolved.useExplicitFrame = params->useExplicitFrame;
        if (params->useExplicitFrame) {
            resolved.explicitFrame = params->explicitFrame;
        }
    }

    if (resolved.width <= kPlanePrimitiveMinSize || resolved.height <= kPlanePrimitiveMinSize) {
        return false;
    }

    PlaneFrame3 frame;
    if (resolved.useExplicitFrame) {
        frame = resolved.explicitFrame;
    } else if (layout->scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME) {
        frame = layout->scene3d.constructionPlane.customFrame;
    } else {
        ViewPlane view = layout->scene3d.constructionPlane.axisAligned;
        Plane3 p = Plane3_FromViewPlane(view);
        Vec3 origin = { 0.0f, 0.0f, 0.0f };
        switch (view.axis) {
            case VIEW_PLANE_YZ: origin.x = view.offset; break;
            case VIEW_PLANE_XZ: origin.y = view.offset; break;
            case VIEW_PLANE_XY:
            default: origin.z = view.offset; break;
        }
        frame = PlaneFrame3_FromPlane(p, origin);
        frame.origin = origin;
    }
    if (!PlaneFrame_IsValid(&frame)) return false;

    frame.axisU = Vec3_Normalize(frame.axisU);
    frame.axisV = Vec3_Normalize(frame.axisV);
    frame.normal = Vec3_Normalize(frame.normal);

    bool boundsAdjusted = false;
    Vec3 center = frame.origin;
    bool centerClamped = false;
    if (!Layout_SceneBounds3D_ClampPoint(&layout->scene3d.bounds, &center, &centerClamped)) {
        return false;
    }
    if (centerClamped) boundsAdjusted = true;
    frame.origin = center;

    if (resolved.lockToBounds) {
        bool sizeAdjusted = false;
        if (!SceneBounds_ClampPlaneExtents(&layout->scene3d.bounds,
                                           center,
                                           frame.axisU,
                                           frame.axisV,
                                           &resolved.width,
                                           &resolved.height,
                                           &sizeAdjusted)) {
            return false;
        }
        if (sizeAdjusted) boundsAdjusted = true;
    }

    CoreObjectDimensionalMode dimensionalMode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    CoreObjectPlane corePlane = CorePlane_FromNormal(frame.normal);
    bool canPlaneLockInCore = false;
    if (resolved.lockToConstructionPlane) {
        if (layout->scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED) {
            corePlane = CorePlane_FromViewAxis(layout->scene3d.constructionPlane.axisAligned.axis);
            float lockedCoord = center.z;
            if (corePlane == CORE_OBJECT_PLANE_YZ) lockedCoord = center.x;
            if (corePlane == CORE_OBJECT_PLANE_XZ) lockedCoord = center.y;
            canPlaneLockInCore = fabsf(lockedCoord) <= 1e-4f;
        } else {
            const Vec3 n = Vec3_Normalize(frame.normal);
            const float strongest = fmaxf(fabsf(n.x), fmaxf(fabsf(n.y), fabsf(n.z)));
            if (strongest >= 0.98f) {
                float lockedCoord = center.z;
                corePlane = CorePlane_FromNormal(n);
                if (corePlane == CORE_OBJECT_PLANE_YZ) lockedCoord = center.x;
                if (corePlane == CORE_OBJECT_PLANE_XZ) lockedCoord = center.y;
                canPlaneLockInCore = fabsf(lockedCoord) <= 1e-4f;
            }
        }
    }
    if (canPlaneLockInCore) {
        dimensionalMode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    }

    Transform3D t = Layout_Transform3D_Default();
    t.position = center;
    const uint32_t objectId = Layout_ObjectStore_Create(&layout->objectStore,
                                                        OBJECT3D_KIND_PLANE,
                                                        &t,
                                                        "plane_primitive",
                                                        dimensionalMode,
                                                        corePlane);
    if (objectId == 0u) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object) return false;
    object->plane.width = resolved.width;
    object->plane.height = resolved.height;
    object->plane.frame = frame;
    object->plane.frame.origin = object->transform.position;
    object->plane.lockToConstructionPlane = resolved.lockToConstructionPlane;
    object->plane.lockToBounds = resolved.lockToBounds;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        (void)Layout_ObjectStore_Delete(&layout->objectStore, objectId);
        return false;
    }

    if (outObjectId) *outObjectId = objectId;
    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    return true;
}

bool Layout_CreateRectPrismPrimitive(Layout* layout,
                                     const RectPrismPrimitiveCreateParams* params,
                                     uint32_t* outObjectId,
                                     bool* outBoundsAdjusted) {
    if (outObjectId) *outObjectId = 0u;
    if (outBoundsAdjusted) *outBoundsAdjusted = false;
    if (!layout) return false;
    if (!Layout_ConstructionPlane3D_IsValid(&layout->scene3d.constructionPlane)) return false;
    if (layout->scene3d.bounds.enabled && !Layout_SceneBounds3D_IsValid(&layout->scene3d.bounds)) return false;

    RectPrismPrimitiveCreateParams resolved;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&resolved);
    if (params) {
        if (params->width > 0.0f) resolved.width = params->width;
        if (params->height > 0.0f) resolved.height = params->height;
        if (params->depth >= 0.0f) resolved.depth = params->depth;
        resolved.lockToConstructionPlane = params->lockToConstructionPlane;
        resolved.lockToBounds = params->lockToBounds;
        resolved.useExplicitFrame = params->useExplicitFrame;
        if (params->useExplicitFrame) {
            resolved.explicitFrame = params->explicitFrame;
        }
    }

    if (resolved.width <= kRectPrismPrimitiveMinSize ||
        resolved.height <= kRectPrismPrimitiveMinSize ||
        resolved.depth < 0.0f) {
        return false;
    }

    PlaneFrame3 frame;
    if (resolved.useExplicitFrame) {
        frame = resolved.explicitFrame;
    } else if (layout->scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME) {
        frame = layout->scene3d.constructionPlane.customFrame;
    } else {
        ViewPlane view = layout->scene3d.constructionPlane.axisAligned;
        Plane3 p = Plane3_FromViewPlane(view);
        Vec3 origin = { 0.0f, 0.0f, 0.0f };
        switch (view.axis) {
            case VIEW_PLANE_YZ: origin.x = view.offset; break;
            case VIEW_PLANE_XZ: origin.y = view.offset; break;
            case VIEW_PLANE_XY:
            default: origin.z = view.offset; break;
        }
        frame = PlaneFrame3_FromPlane(p, origin);
        frame.origin = origin;
    }
    if (!PlaneFrame_IsValid(&frame)) return false;

    frame.axisU = Vec3_Normalize(frame.axisU);
    frame.axisV = Vec3_Normalize(frame.axisV);
    frame.normal = Vec3_Normalize(frame.normal);

    bool boundsAdjusted = false;
    Vec3 center = frame.origin;
    bool centerClamped = false;
    if (!Layout_SceneBounds3D_ClampPoint(&layout->scene3d.bounds, &center, &centerClamped)) {
        return false;
    }
    if (centerClamped) boundsAdjusted = true;
    frame.origin = center;

    if (resolved.lockToBounds) {
        bool sizeAdjusted = false;
        if (!SceneBounds_ClampRectPrismExtents(&layout->scene3d.bounds,
                                               center,
                                               frame.axisU,
                                               frame.axisV,
                                               frame.normal,
                                               &resolved.width,
                                               &resolved.height,
                                               &resolved.depth,
                                               &sizeAdjusted)) {
            return false;
        }
        if (sizeAdjusted) boundsAdjusted = true;
    }

    Transform3D t = Layout_Transform3D_Default();
    t.position = center;
    const uint32_t objectId = Layout_ObjectStore_Create(&layout->objectStore,
                                                        OBJECT3D_KIND_RECT_PRISM,
                                                        &t,
                                                        "rect_prism_primitive",
                                                        CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                                        CorePlane_FromNormal(frame.normal));
    if (objectId == 0u) return false;

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object) return false;
    object->rectPrism.width = resolved.width;
    object->rectPrism.height = resolved.height;
    object->rectPrism.depth = resolved.depth;
    object->rectPrism.frame = frame;
    object->rectPrism.frame.origin = object->transform.position;
    object->rectPrism.lockToConstructionPlane = resolved.lockToConstructionPlane;
    object->rectPrism.lockToBounds = resolved.lockToBounds;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        (void)Layout_ObjectStore_Delete(&layout->objectStore, objectId);
        return false;
    }

    if (outObjectId) *outObjectId = objectId;
    if (outBoundsAdjusted) *outBoundsAdjusted = boundsAdjusted;
    return true;
}

float Layout_PlanePrimitiveMinSize(void) {
    return kPlanePrimitiveMinSize;
}
