#include "Core/line_drawing_inspection_proof.h"
#include "Core/global_state.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include <stdio.h>
#include <string.h>

bool LineDrawingInspection_CheckProof(const char *mode) {
    GlobalState *s = Global_Get();
    if (!s || !mode)
        return false;
    bool left = !strcmp(mode, "camera-inspection-left");
    const char *far_id = left ? "storage_passenger_door" : "storage_driver_door";
    const char *near_id = left ? "driver_bench" : "kitchen_base";
    const Object3D *far = NULL;
    const Object3D *near = NULL;
    for (size_t i = 0; i < s->layout.objectStore.count; ++i) {
        const Object3D *o = &s->layout.objectStore.items[i];
        if (!strcmp(o->coreMeta.object_id, far_id))
            far = o;
        if (!strcmp(o->coreMeta.object_id, near_id))
            near = o;
    }
    if (!far || !near)
        return false;
    SpaceViewContext view = SpaceAdapter_BuildViewContext(s);
    Vec2 screen =
        WorldToScreen(SpaceAdapter_ProjectToView(far->rectPrism.frame.origin, &view), &s->grid);
    uint32_t picked = 0;
    if (!Layout_SolidPreviewPick(&s->layout, &view, &s->grid, (int)screen.x, (int)screen.y,
                                 &picked) ||
        picked != far->objectId) {
        fprintf(stderr, "line_drawing: C2 far surface pick failed expected=%u actual=%u\n",
                far->objectId, picked);
        return false;
    }
    /* The reference shell must not mask construction edges behind its outline-only interior. */
    Vec3 edge = Vec3_Add(near->rectPrism.frame.origin,
                         Vec3_Scale(near->rectPrism.frame.normal, near->rectPrism.depth * .5f));
    screen = WorldToScreen(SpaceAdapter_ProjectToView(edge, &view), &s->grid);
    bool found = false;
    for (int y = -4; y <= 4 && !found; ++y)
        for (int x = -4; x <= 4 && !found; ++x) {
            if (Layout_SolidPreviewPick(&s->layout, &view, &s->grid, (int)screen.x + x,
                                        (int)screen.y + y, &picked) &&
                picked == near->objectId)
                found = true;
        }
    if (!found || Layout_MeshSolidPreviewNextUpdateDelayMs() != -1) {
        fprintf(stderr,
                "line_drawing: C2 outline visibility/settle check failed outline=%d delay=%d\n",
                found, Layout_MeshSolidPreviewNextUpdateDelayMs());
        return false;
    }
    fprintf(stderr,
            "line_drawing: C2 %s far surface + near construction picks passed; preview settled\n",
            left ? "left" : "right");
    return true;
}
