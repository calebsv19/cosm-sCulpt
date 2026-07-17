#include "Editor/object3d_origin_pick.h"

#include "Layout/layout.h"
#include "Layout/scene/layout_object_faces.h"
#include "Math/math_util.h"
#include "core_screen_pick.h"

#include <math.h>
#include <stdlib.h>

static CoreScreenPickIndex s_origin_pick_index;
static bool s_origin_pick_initialized = false;
static uint64_t s_origin_pick_revision = 0u;

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

static bool Editor_EnsureObject3DOriginPickIndex(void) {
    if (s_origin_pick_initialized) return true;
    if (core_screen_pick_index_init(&s_origin_pick_index,
                                    core_screen_pick_config_default()).code != CORE_OK) {
        return false;
    }
    s_origin_pick_initialized = true;
    return true;
}

bool Editor_RebuildObject3DOriginPickIndex(const Layout* layout,
                                           const Grid* grid,
                                           const SpaceViewContext* viewCtx) {
    CoreScreenPickCandidate* candidates = NULL;
    size_t candidate_count = 0u;
    bool ok = false;
    if (!layout || !grid || !viewCtx || !Editor_EnsureObject3DOriginPickIndex()) return false;
    if (layout->objectStore.count > 0u) {
        candidates = malloc(layout->objectStore.count * sizeof(*candidates));
        if (!candidates) return false;
    }
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        Vec3 center = {0};
        Vec2 center_view = {0};
        Vec2 center_screen = {0};
        if (!Layout_ObjectStore_ValidateObject(object)) continue;
        center = object->transform.position;
        (void)Layout_Object3D_ComputeVisualCenter(object, &center);
        center_view = SpaceAdapter_ProjectToView(center, viewCtx);
        center_screen = WorldToScreen(center_view, grid);
        if (!isfinite(center_screen.x) || !isfinite(center_screen.y)) continue;
        candidates[candidate_count++] = (CoreScreenPickCandidate){
            .stable_key = object->objectId,
            .payload = (int64_t)object->objectId,
            .screen_x = center_screen.x,
            .screen_y = center_screen.y,
            .view_depth = Object3DOriginPick_SignedDepth(viewCtx, center)
        };
    }
    s_origin_pick_revision += 1u;
    ok = core_screen_pick_index_rebuild(&s_origin_pick_index,
                                        candidates,
                                        candidate_count,
                                        s_origin_pick_revision).code == CORE_OK;
    free(candidates);
    return ok;
}

void Editor_ShutdownObject3DOriginPickIndex(void) {
    if (!s_origin_pick_initialized) return;
    core_screen_pick_index_destroy(&s_origin_pick_index);
    s_origin_pick_initialized = false;
    s_origin_pick_revision = 0u;
}

bool Editor_PickNearestObject3DOrigin(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      uint32_t* outObjectId,
                                      float* outDistSq) {
    CoreScreenPickResult result = {0};
    (void)layout;
    (void)grid;
    (void)viewCtx;
    if (!outObjectId || !s_origin_pick_initialized) return false;
    if (core_screen_pick_query_nearest(&s_origin_pick_index,
                                       (double)mouseX,
                                       (double)mouseY,
                                       &result).code != CORE_OK ||
        !result.found || result.payload < 0 || result.payload > UINT32_MAX) {
        return false;
    }
    *outObjectId = (uint32_t)result.payload;
    if (outDistSq) *outDistSq = (float)result.distance_sq;
    return true;
}

Hitbox Editor_ResolveObject3DBodyPick(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      Hitbox baseHit) {
    uint32_t object_id = 0u;
    if (baseHit.type != HITBOX_NONE && baseHit.type != HITBOX_OBJECT3D) return baseHit;
    if (!Editor_PickNearestObject3DOrigin(layout,
                                          grid,
                                          viewCtx,
                                          mouseX,
                                          mouseY,
                                          &object_id,
                                          NULL)) {
        return (Hitbox){ .type = HITBOX_NONE, .index = -1, .subIndex = -1 };
    }
    return (Hitbox){ .type = HITBOX_OBJECT3D, .index = (int)object_id, .subIndex = -1 };
}
