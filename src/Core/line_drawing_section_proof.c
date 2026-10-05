#include "Core/line_drawing_section_proof.h"
#include "Core/global_state.h"
#include "Core/viewport_zoom.h"
#include "Layout/layout_json.h"
#include "UI/ui_panel_section.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_right_scroll.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "Editor/object3d_origin_pick.h"
#include <string.h>

static bool click_control(int action) {
    SDL_Rect r = {0};
    return UIPanel_SectionControlRect(action, &r) && UIPanel_SectionClick(r.x + r.w / 2, r.y + r.h / 2);
}

bool LineDrawingSection_StageProof(const char* mode) {
    GlobalState* state = Global_Get();
    UIPanelState* ui = UIPanel_Get();
    if (!state || !ui || !mode ||
        !Layout_LoadFromFile(&state->layout, "config/examples/van_connected_sections_s5.layout.json"))
        return false;
    state->workspaceMode = LINE_DRAWING_WORKSPACE_MODE_SCENE;
    state->spaceMode = SPACE_MODE_3D;
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35;
    state->freeViewCamera.pitchDeg = 15;
    state->freeViewCamera.target = (Vec3){0};
    (void)UIPanel_SetDisplayUnit(CORE_UNIT_MILLIMETER);
    state->editor.selectedObject3DId = 0;
    for (size_t i = 0; i < state->layout.objectStore.count; ++i) {
        if (!strcmp(state->layout.objectStore.items[i].coreMeta.object_id, "storage_driver"))
            state->editor.selectedObject3DId = state->layout.objectStore.items[i].objectId;
    }
    if (!state->editor.selectedObject3DId)
        return false;
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    if (!click_control(UI_SECTION_SELECTED))
        return false;
    if (strcmp(mode, "cabinet-solid")) {
        if (!click_control(UI_SECTION_ALONG) ||
            !click_control(!strcmp(mode, "cabinet-section") ? UI_SECTION_EXACT : UI_SECTION_CUTAWAY))
            return false;
        state->sectionView.position_meters = -.7181;
        if (!strcmp(mode, "cabinet-cutaway"))
            state->freeViewCamera.enabled = true;
        (void)LineDrawingViewportZoom_FitVisibleGeometry(state);
    }
    /* Unselected surfaces prove occlusion without x-ray selection cages. */
    state->editor.selectedObject3DId = 0;
    state->editor.hoveredObject3DId = 0;
    Editor_ClearHistory(&state->editor);
    Global_FlagGridChanged();
    Global_FlagHitboxesDirty();
    return true;
}

bool LineDrawingSection_CheckProof(const char* mode) {
    if (strcmp(mode, "cabinet-section"))
        return true;
    GlobalState* s = Global_Get();
    SpaceViewContext view = SpaceAdapter_BuildViewContext(s);
    Vec3 samples[2] = {{-.655f, -.7181f, .865f}, {-.655f, -.7181f, 1.25f}};
    for (int i = 0; i < 2; ++i) {
        Vec2 p = WorldToScreen(SpaceAdapter_ProjectToView(samples[i], &view), &s->grid);
        uint32_t id = 0;
        if (!Layout_SolidPreviewPick(&s->layout, &view, &s->grid, (int)p.x, (int)p.y, &id))
            return false;
        const Object3D* object = Layout_ObjectStore_FindConst(&s->layout.objectStore, id);
        if ((!i && (!object || strcmp(object->coreMeta.object_id, "storage_driver_shelf"))) || (i && id))
            return false;
        Hitbox hit =
            Editor_ResolveObject3DBodyPick(&s->layout, &s->grid, &view, (int)p.x, (int)p.y,
                                           (Hitbox){.type = HITBOX_OBJECT3D_GIZMO_AXIS, .index = 38});
        if (hit.type != (i ? HITBOX_NONE : HITBOX_OBJECT3D))
            return false;
    }
    return true;
}
