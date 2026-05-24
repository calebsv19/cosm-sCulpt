#include "Editor/object3d_origin_pick.h"

#include "Layout/layout.h"
#include "Math/math_util.h"

#include <math.h>

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
