#include "Layout/layout.h"
#include "Core/global_state.h"

#include <math.h>
#include <stdlib.h>

static const float kConstructionPlaneEpsilon = 1e-4f;
static const float kPlanePrimitiveMinSize = 1e-3f;
static const float kRectPrismPrimitiveMinSize = 1e-3f;

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

void Layout_ObjectStore_Init(LayoutObjectStore* store) {
    if (!store) return;
    store->items = NULL;
    store->count = 0;
    store->nextObjectId = 1u;
}

void Layout_ObjectStore_Free(LayoutObjectStore* store) {
    if (!store) return;
    free(store->items);
    store->items = NULL;
    store->count = 0;
    store->nextObjectId = 1u;
}

Object3D* Layout_ObjectStore_Find(LayoutObjectStore* store, uint32_t objectId) {
    if (!store || objectId == 0u) return NULL;
    for (size_t i = 0; i < store->count; ++i) {
        Object3D* object = &store->items[i];
        if (object->objectId == objectId && !object->isDeleted) {
            return object;
        }
    }
    return NULL;
}

const Object3D* Layout_ObjectStore_FindConst(const LayoutObjectStore* store, uint32_t objectId) {
    if (!store || objectId == 0u) return NULL;
    for (size_t i = 0; i < store->count; ++i) {
        const Object3D* object = &store->items[i];
        if (object->objectId == objectId && !object->isDeleted) {
            return object;
        }
    }
    return NULL;
}

bool Layout_ObjectStore_Delete(LayoutObjectStore* store, uint32_t objectId) {
    Object3D* object = Layout_ObjectStore_Find(store, objectId);
    if (!object) return false;
    object->isDeleted = true;
    Global_FlagLayoutChanged();
    return true;
}

bool Layout_ObjectStore_ValidateObject(const Object3D* object) {
    if (!object || object->isDeleted) return false;
    if (object->objectId == 0u) return false;
    if (object->kind == OBJECT3D_KIND_UNKNOWN) return false;
    if (object->kind == OBJECT3D_KIND_PLANE) {
        if (object->plane.width <= kPlanePrimitiveMinSize ||
            object->plane.height <= kPlanePrimitiveMinSize) {
            return false;
        }
        if (!PlaneFrame_IsValid(&object->plane.frame)) return false;
        if (Vec3_Distance(object->plane.frame.origin, object->transform.position) > 1e-3f) {
            return false;
        }
    } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        if (object->rectPrism.width <= kRectPrismPrimitiveMinSize ||
            object->rectPrism.height <= kRectPrismPrimitiveMinSize ||
            object->rectPrism.depth < 0.0f) {
            return false;
        }
        if (!PlaneFrame_IsValid(&object->rectPrism.frame)) return false;
        if (Vec3_Distance(object->rectPrism.frame.origin, object->transform.position) > 1e-3f) {
            return false;
        }
    }
    CoreResult validate = core_object_validate(&object->coreMeta);
    return validate.code == CORE_OK;
}

size_t Layout_ObjectStore_LiveCount(const LayoutObjectStore* store) {
    if (!store) return 0u;
    size_t liveCount = 0;
    for (size_t i = 0; i < store->count; ++i) {
        if (!store->items[i].isDeleted) {
            ++liveCount;
        }
    }
    return liveCount;
}

bool Layout_Object3D_ComputePlaneCorners(const Object3D* object, Vec3 outCorners[4]) {
    if (!object || !outCorners) return false;
    if (object->isDeleted || object->kind != OBJECT3D_KIND_PLANE) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    const Vec3 center = object->plane.frame.origin;
    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->plane.frame.axisV);
    const float halfW = object->plane.width * 0.5f;
    const float halfH = object->plane.height * 0.5f;
    const Vec3 u = Vec3_Scale(axisU, halfW);
    const Vec3 v = Vec3_Scale(axisV, halfH);

    outCorners[0] = Vec3_Add(center, Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)));
    outCorners[1] = Vec3_Add(center, Vec3_Add(u, Vec3_Scale(v, -1.0f)));
    outCorners[2] = Vec3_Add(center, Vec3_Add(u, v));
    outCorners[3] = Vec3_Add(center, Vec3_Add(Vec3_Scale(u, -1.0f), v));
    return true;
}

bool Layout_Object3D_ComputeRectPrismCorners(const Object3D* object, Vec3 outCorners[8]) {
    if (!object || !outCorners) return false;
    if (object->isDeleted || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    const Vec3 center = object->rectPrism.frame.origin;
    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    const float halfD = object->rectPrism.depth * 0.5f;
    const Vec3 u = Vec3_Scale(axisU, halfW);
    const Vec3 v = Vec3_Scale(axisV, halfH);
    const Vec3 n = Vec3_Scale(axisN, halfD);

    outCorners[0] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)), Vec3_Scale(n, -1.0f)));
    outCorners[1] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, Vec3_Scale(v, -1.0f)), Vec3_Scale(n, -1.0f)));
    outCorners[2] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, v), Vec3_Scale(n, -1.0f)));
    outCorners[3] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), v), Vec3_Scale(n, -1.0f)));
    outCorners[4] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)), n));
    outCorners[5] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, Vec3_Scale(v, -1.0f)), n));
    outCorners[6] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, v), n));
    outCorners[7] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), v), n));
    return true;
}
