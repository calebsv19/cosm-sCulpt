#include "Core/global_state.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "Editor/object3d_origin_pick.h"

#include "Layout/layout.h"
#include "Layout/layout_section.h"
#include "Layout/layout_engineering.h"
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
        if (!Layout_ObjectStore_ValidateObject(object) ||
            !Layout_ObjectShown(&layout->objectStore, object) ||
            !object->coreMeta.flags.selectable) continue;
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

static float Object3DOriginPick_EdgeDistanceSquared(Vec2 point, Vec2 a, Vec2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float length_squared = dx * dx + dy * dy;
    float t = length_squared > 1e-6f
        ? ((point.x - a.x) * dx + (point.y - a.y) * dy) / length_squared : 0;
    t = fmaxf(0, fminf(1, t));
    dx = point.x - a.x - t * dx;
    dy = point.y - a.y - t * dy;
    return dx * dx + dy * dy;
}

/* Wireframe body selection follows visible edges, not the empty area of a large
 * screen bounding rectangle. Centers retain the existing shared pick-index path. */
static Hitbox Object3DOriginPick_PickEdge(const Layout* layout, const Grid* grid,
                                         const SpaceViewContext* view, int x, int y) {
    static const int edges[24][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6},
        {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7},
        {8,9}, {9,10}, {10,11}, {11,8}, {12,13}, {13,14},
        {14,15}, {15,12}, {8,12}, {9,13}, {10,14}, {11,15}
    };
    Hitbox best = {.type = HITBOX_NONE, .index = -1, .subIndex = -1};
    float best_distance = 64; /* Eight screen pixels of edge tolerance. */
    float best_depth = -INFINITY;
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        Vec3 corners[16];
        int edge_count = 0;
        if (!Layout_ObjectStore_ValidateObject(object) ||
            !Layout_ObjectShown(&layout->objectStore, object) ||
            !object->coreMeta.flags.selectable) continue;
        if (object->kind == OBJECT3D_KIND_PLANE &&
            Layout_Object3D_ComputePlaneCorners(object, corners)) edge_count = 4;
        else if (object->kind == OBJECT3D_KIND_RECT_PRISM &&
                 Layout_Object3D_ComputeRectPrismCorners(object, corners)) edge_count = 12;
        else if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE &&
                 Layout_Object3D_ComputeMeshInstanceCorners(object, corners)) edge_count = 12;
        if (edge_count == 12 && object->kind == OBJECT3D_KIND_RECT_PRISM && object->rectPrism.opening.enabled) {
            Layout_PanelOpeningCorners(object,corners+8); edge_count=24;
        }
        float depth = Object3DOriginPick_SignedDepth(view, object->transform.position);
        for (int e = 0; e < edge_count; ++e) {
            Vec2 a = WorldToScreen(SpaceAdapter_ProjectToView(corners[edges[e][0]], view), grid);
            Vec2 b = WorldToScreen(SpaceAdapter_ProjectToView(corners[edges[e][1]], view), grid);
            float distance = Object3DOriginPick_EdgeDistanceSquared((Vec2){(float)x, (float)y}, a, b);
            if (!isfinite(distance) || distance > 64) continue;
            if (distance < best_distance - .01f || (fabsf(distance - best_distance) <= .01f &&
                (best.type == HITBOX_NONE || depth > best_depth ||
                 (depth == best_depth && object->objectId < (uint32_t)best.index)))) {
                best_distance = distance;
                best_depth = depth;
                best = (Hitbox){.type = HITBOX_OBJECT3D, .index = (int)object->objectId, .subIndex = -1};
            }
        }
    }
    return best;
}

Hitbox Editor_ResolveObject3DBodyPick(const Layout* layout,
                                      const Grid* grid,
                                      const SpaceViewContext* viewCtx,
                                      int mouseX,
                                      int mouseY,
                                      Hitbox baseHit) {
    uint32_t object_id = 0u;
    if (!layout || !grid || !viewCtx) return (Hitbox){.type = HITBOX_NONE, .index = -1};
    const GlobalState* state=Global_Get();
    bool section_active = state && state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE &&
        (state->sectionView.mode != LAYOUT_SECTION_OFF || viewCtx->inspection);
    /* Section views hide manipulation overlays; their old hitboxes must not remain clickable. */
    if (!section_active && baseHit.type != HITBOX_NONE && baseHit.type != HITBOX_OBJECT3D) return baseHit;
    if(state && state->workspaceMode==LINE_DRAWING_WORKSPACE_MODE_SCENE &&
       (viewCtx->inspection || state->previewMode==LINE_DRAWING_PREVIEW_MODE_FLAT || state->previewMode==LINE_DRAWING_PREVIEW_MODE_MATERIAL) &&
       Layout_SolidPreviewPick(layout,viewCtx,grid,mouseX,mouseY,&object_id))
        return object_id ? (Hitbox){.type=HITBOX_OBJECT3D,.index=(int)object_id,.subIndex=-1} :
                           (Hitbox){.type=HITBOX_NONE,.index=-1,.subIndex=-1};
    if (section_active) return (Hitbox){.type = HITBOX_NONE, .index = -1, .subIndex = -1};
    if (!Editor_PickNearestObject3DOrigin(layout,
                                          grid,
                                          viewCtx,
                                          mouseX,
                                          mouseY,
                                          &object_id,
                                          NULL)) {
        return Object3DOriginPick_PickEdge(layout, grid, viewCtx, mouseX, mouseY);
    }
    return (Hitbox){ .type = HITBOX_OBJECT3D, .index = (int)object_id, .subIndex = -1 };
}
