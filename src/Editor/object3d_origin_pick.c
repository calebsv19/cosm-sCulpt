#include "Editor/object3d_origin_pick.h"

#include "Layout/layout.h"
#include "Layout/scene/layout_object_faces.h"
#include "Math/math_util.h"

#include <math.h>

static float Object3DOriginPick_SignedDepth(const SpaceViewContext* viewCtx,
                                            Vec3 point) {
    if (!viewCtx) return point.z;
    if (SpaceAdapter_IsFreeViewEnabled(viewCtx)) {
        return Vec3_Dot(Vec3_Sub(point, viewCtx->camera.target),
                        FreeView_Forward(&viewCtx->camera));
    }

    switch (viewCtx->plane.axis) {
        case VIEW_PLANE_YZ: return point.x;
        case VIEW_PLANE_XZ: return point.y;
        case VIEW_PLANE_XY:
        default: return point.z;
    }
}

static bool Editor_PickTopmostVisibleObject3DFace(const Layout* layout,
                                                  const Grid* grid,
                                                  const SpaceViewContext* viewCtx,
                                                  int mouseX,
                                                  int mouseY,
                                                  uint32_t* outObjectId) {
    bool found = false;
    float bestDepth = 0.0f;
    uint32_t bestObjectId = 0u;

    if (!layout || !grid || !viewCtx || !outObjectId) return false;

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        Object3DFaceKind face = OBJECT3D_FACE_NONE;
        PlaneFrame3 faceFrame = {0};
        float depth = 0.0f;

        if (!Layout_ObjectStore_ValidateObject(object)) continue;
        if (!Layout_Object3D_PickVisibleFaceAtScreenPoint(object,
                                                          viewCtx,
                                                          grid,
                                                          mouseX,
                                                          mouseY,
                                                          &face)) {
            continue;
        }
        if (!Layout_Object3DFace_GetFrame(object, face, &faceFrame)) continue;

        depth = Object3DOriginPick_SignedDepth(viewCtx, faceFrame.origin);
        if (!found ||
            depth > bestDepth + 1e-4f ||
            (fabsf(depth - bestDepth) <= 1e-4f && object->objectId > bestObjectId)) {
            found = true;
            bestDepth = depth;
            bestObjectId = object->objectId;
        }
    }

    if (!found) return false;

    *outObjectId = bestObjectId;
    return true;
}

bool Editor_PickNearestObject3DOrigin(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      float captureRadiusPx,
                                      uint32_t* outObjectId,
                                      float* outDistSq) {
    bool found = false;
    float bestDistSq = 0.0f;
    uint32_t bestObjectId = 0u;
    const float captureDistSq = captureRadiusPx * captureRadiusPx;

    if (!layout || !grid || !viewCtx || !outObjectId || captureRadiusPx <= 0.0f) {
        return false;
    }

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        float dx = 0.0f;
        float dy = 0.0f;
        float distSq = 0.0f;

        if (!Layout_ObjectStore_ValidateObject(object)) continue;

        Vec2 originView = SpaceAdapter_ProjectToView(object->transform.position, viewCtx);
        Vec2 originScreen = WorldToScreen(originView, grid);
        dx = originScreen.x - (float)mouseX;
        dy = originScreen.y - (float)mouseY;
        distSq = dx * dx + dy * dy;
        if (distSq > captureDistSq) continue;

        if (!found ||
            distSq + 0.25f < bestDistSq ||
            (fabsf(distSq - bestDistSq) <= 0.25f && object->objectId > bestObjectId)) {
            found = true;
            bestDistSq = distSq;
            bestObjectId = object->objectId;
        }
    }

    if (!found) return false;

    *outObjectId = bestObjectId;
    if (outDistSq) *outDistSq = bestDistSq;
    return true;
}

Hitbox Editor_ResolveObject3DBodyPick(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      Hitbox baseHit,
                                      float captureRadiusPx) {
    uint32_t objectId = 0u;

    if (baseHit.type != HITBOX_NONE && baseHit.type != HITBOX_OBJECT3D) {
        return baseHit;
    }

    if (baseHit.type == HITBOX_OBJECT3D) {
        if (!Editor_PickNearestObject3DOrigin(layout,
                                              grid,
                                              viewCtx,
                                              mouseX,
                                              mouseY,
                                              captureRadiusPx,
                                              &objectId,
                                              NULL)) {
            return (Hitbox){ .type = HITBOX_NONE, .index = -1, .subIndex = -1 };
        }
        return (Hitbox){ .type = HITBOX_OBJECT3D, .index = (int)objectId, .subIndex = -1 };
    }

    if (Editor_PickTopmostVisibleObject3DFace(layout,
                                              grid,
                                              viewCtx,
                                              mouseX,
                                              mouseY,
                                              &objectId)) {
        return (Hitbox){ .type = HITBOX_OBJECT3D, .index = (int)objectId, .subIndex = -1 };
    }

    if (!Editor_PickNearestObject3DOrigin(layout,
                                          grid,
                                          viewCtx,
                                          mouseX,
                                          mouseY,
                                          captureRadiusPx,
                                          &objectId,
                                          NULL)) {
        return (Hitbox){ .type = HITBOX_NONE, .index = -1, .subIndex = -1 };
    }

    return (Hitbox){ .type = HITBOX_OBJECT3D, .index = (int)objectId, .subIndex = -1 };
}
