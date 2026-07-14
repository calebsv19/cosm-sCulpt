#include "test_layout_internal.h"

#include "UI/input_ui_panel.h"
#include "UI/ui_panel_edit_layout.h"
#include "UI/ui_panel_object_inspector.h"
#include "UI/ui_panel_object_layout.h"
#include "UI/ui_panel_scene_authoring_inspector.h"
#include "Layout/scene/layout_scene_camera_authoring.h"

static const UIButton* ld_test_find_button(const UIPanelState* ui, int button_id) {
    if (!ui) return NULL;
    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == button_id) return &ui->buttons[i];
    }
    return NULL;
}

static bool ld_test_click_button_center(const UIButton* button) {
    TEST_ASSERT(button != NULL);
    TEST_ASSERT(button->bounds.w > 0);
    TEST_ASSERT(button->bounds.h > 0);
    return UIPanel_HandleClick(button->bounds.x + button->bounds.w / 2,
                               button->bounds.y + button->bounds.h / 2);
}

static bool test_object_inspector_reserves_space_for_selected_object(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    uint32_t object_id = 0u;
    bool adjusted = false;
    int summary_height = 0;
    int details_height = 0;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect details_rect = {0, 0, 0, 0};
    SDL_Rect actions_rect = {0, 0, 0, 0};
    const UIButton* action_button = NULL;
    const UIButton* prism_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    plane = (PlanePrimitiveCreateParams){
        .width = 10.0f,
        .height = 6.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 2.0f, 2.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };

    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    summary_height = UIPanel_ObjectInspectorReservedHeight(ui);
    details_height = UIPanel_ObjectInspectorDetailsHeight(ui);
    TEST_ASSERT(summary_height > 0);
    TEST_ASSERT(details_height > 0);
    TEST_ASSERT(UIPanel_GetObjectPaneRects(ui,
                                           &summary_rect,
                                           &details_rect,
                                           &actions_rect,
                                           NULL,
                                           NULL,
                                           NULL));
    TEST_ASSERT(summary_rect.h == summary_height);
    TEST_ASSERT(details_rect.h <= details_height);
    TEST_ASSERT(details_rect.h >= 0);
    TEST_ASSERT(details_rect.y >= summary_rect.y + summary_rect.h);
    TEST_ASSERT(actions_rect.y >= details_rect.y + details_rect.h);

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_OBJECT_CLEAR_SELECTION) {
            action_button = &ui->buttons[i];
        } else if (ui->buttons[i].id == UI_BTN_EDIT_PRISM_WIDTH) {
            prism_button = &ui->buttons[i];
        }
    }
    TEST_ASSERT(action_button != NULL);
    TEST_ASSERT(action_button->group == UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS);
    TEST_ASSERT(action_button->bounds.y >= actions_rect.y);
    TEST_ASSERT(action_button->bounds.y <= actions_rect.y + actions_rect.h);
    TEST_ASSERT(prism_button != NULL);
    TEST_ASSERT(prism_button->group == UI_PANEL_GROUP_RIGHT_PRISM);
    TEST_ASSERT(prism_button->bounds.y >= actions_rect.y + actions_rect.h);
    TEST_ASSERT(action_button->bounds.x >= actions_rect.x);
    TEST_ASSERT(action_button->bounds.x + action_button->bounds.w <= actions_rect.x + actions_rect.w);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_inspector_layout_stays_stable_when_selection_changes(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    uint32_t object_id = 0u;
    bool adjusted = false;
    SDL_Rect empty_summary_rect = {0, 0, 0, 0};
    SDL_Rect empty_details_rect = {0, 0, 0, 0};
    SDL_Rect empty_actions_rect = {0, 0, 0, 0};
    SDL_Rect empty_prism_rect = {0, 0, 0, 0};
    SDL_Rect empty_gizmo_rect = {0, 0, 0, 0};
    SDL_Rect empty_transform_rect = {0, 0, 0, 0};
    SDL_Rect selected_summary_rect = {0, 0, 0, 0};
    SDL_Rect selected_details_rect = {0, 0, 0, 0};
    SDL_Rect selected_actions_rect = {0, 0, 0, 0};
    SDL_Rect selected_prism_rect = {0, 0, 0, 0};
    SDL_Rect selected_gizmo_rect = {0, 0, 0, 0};
    SDL_Rect selected_transform_rect = {0, 0, 0, 0};

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    state->editor.selectedObject3DId = 0u;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetObjectPaneRects(ui,
                                           &empty_summary_rect,
                                           &empty_details_rect,
                                           &empty_actions_rect,
                                           &empty_prism_rect,
                                           &empty_gizmo_rect,
                                           &empty_transform_rect));

    plane = (PlanePrimitiveCreateParams){
        .width = 10.0f,
        .height = 6.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 2.0f, 2.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetObjectPaneRects(ui,
                                           &selected_summary_rect,
                                           &selected_details_rect,
                                           &selected_actions_rect,
                                           &selected_prism_rect,
                                           &selected_gizmo_rect,
                                           &selected_transform_rect));

    TEST_ASSERT(empty_summary_rect.y == selected_summary_rect.y);
    TEST_ASSERT(empty_summary_rect.h == selected_summary_rect.h);
    TEST_ASSERT(empty_details_rect.y == selected_details_rect.y);
    TEST_ASSERT(empty_details_rect.h == selected_details_rect.h);
    TEST_ASSERT(empty_actions_rect.y == selected_actions_rect.y);
    TEST_ASSERT(empty_prism_rect.y == selected_prism_rect.y);
    TEST_ASSERT(empty_gizmo_rect.y == selected_gizmo_rect.y);
    TEST_ASSERT(empty_transform_rect.y == selected_transform_rect.y);
    TEST_ASSERT(selected_transform_rect.y + selected_transform_rect.h <= ui->rightBodyRect.y + ui->rightBodyRect.h);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_inspector_sections_fit_inside_pane(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    uint32_t object_id = 0u;
    bool adjusted = false;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect details_rect = {0, 0, 0, 0};
    SDL_Rect actions_rect = {0, 0, 0, 0};
    SDL_Rect prism_rect = {0, 0, 0, 0};
    SDL_Rect gizmo_rect = {0, 0, 0, 0};
    SDL_Rect transform_rect = {0, 0, 0, 0};

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    plane = (PlanePrimitiveCreateParams){
        .width = 10.0f,
        .height = 6.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 2.0f, 2.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetObjectPaneRects(ui,
                                           &summary_rect,
                                           &details_rect,
                                           &actions_rect,
                                           &prism_rect,
                                           &gizmo_rect,
                                           &transform_rect));

    TEST_ASSERT(summary_rect.x >= ui->rightBodyRect.x);
    TEST_ASSERT(summary_rect.y >= ui->rightBodyRect.y);
    TEST_ASSERT(transform_rect.y + transform_rect.h <= ui->rightBodyRect.y + ui->rightBodyRect.h);
    TEST_ASSERT(summary_rect.y + summary_rect.h <= details_rect.y);
    TEST_ASSERT(details_rect.y + details_rect.h <= actions_rect.y);
    TEST_ASSERT(actions_rect.y + actions_rect.h <= prism_rect.y);
    TEST_ASSERT(prism_rect.y + prism_rect.h <= gizmo_rect.y);
    TEST_ASSERT(gizmo_rect.y + gizmo_rect.h <= transform_rect.y);
    TEST_ASSERT(transform_rect.y + transform_rect.h == ui->rightBodyRect.y + ui->rightBodyRect.h);

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_RIGHT) continue;
        if (btn->bounds.w <= 0 || btn->bounds.h <= 0) continue;
        TEST_ASSERT(btn->bounds.h >= 0);
        TEST_ASSERT(btn->bounds.y >= ui->rightBodyRect.y);
        TEST_ASSERT(btn->bounds.y + btn->bounds.h <= ui->rightBodyRect.y + ui->rightBodyRect.h);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_inspector_lower_sections_anchor_to_bottom(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect actions_rect = {0, 0, 0, 0};
    SDL_Rect prism_rect = {0, 0, 0, 0};
    SDL_Rect gizmo_rect = {0, 0, 0, 0};
    SDL_Rect transform_rect = {0, 0, 0, 0};

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    state->editor.selectedObject3DId = 0u;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetObjectPaneRects(ui,
                                           NULL,
                                           NULL,
                                           &actions_rect,
                                           &prism_rect,
                                           &gizmo_rect,
                                           &transform_rect));

    TEST_ASSERT(transform_rect.y + transform_rect.h == ui->rightBodyRect.y + ui->rightBodyRect.h);
    TEST_ASSERT(gizmo_rect.y + gizmo_rect.h <= transform_rect.y);
    TEST_ASSERT(prism_rect.y + prism_rect.h <= gizmo_rect.y);
    TEST_ASSERT(actions_rect.y + actions_rect.h <= prism_rect.y);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_buttons_use_uniform_grid_rows(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* clear_button = NULL;
    const UIButton* delete_button = NULL;
    const UIButton* width_button = NULL;
    const UIButton* height_button = NULL;
    const UIButton* depth_button = NULL;
    const UIButton* unit_button = NULL;
    const UIButton* position_button = NULL;
    const UIButton* rot_x_button = NULL;
    const UIButton* rot_y_button = NULL;
    const UIButton* rot_z_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->id == UI_BTN_OBJECT_CLEAR_SELECTION) clear_button = btn;
        else if (btn->id == UI_BTN_OBJECT_DELETE_SELECTED) delete_button = btn;
        else if (btn->id == UI_BTN_EDIT_PRISM_WIDTH) width_button = btn;
        else if (btn->id == UI_BTN_EDIT_PRISM_HEIGHT) height_button = btn;
        else if (btn->id == UI_BTN_EDIT_PRISM_DEPTH) depth_button = btn;
        else if (btn->id == UI_BTN_CYCLE_DISPLAY_UNITS) unit_button = btn;
        else if (btn->id == UI_BTN_EDIT_OBJECT_POSITION) position_button = btn;
        else if (btn->id == UI_BTN_EDIT_OBJECT_ROTATION_X) rot_x_button = btn;
        else if (btn->id == UI_BTN_EDIT_OBJECT_ROTATION_Y) rot_y_button = btn;
        else if (btn->id == UI_BTN_EDIT_OBJECT_ROTATION_Z) rot_z_button = btn;
    }

    TEST_ASSERT(clear_button && delete_button);
    TEST_ASSERT(width_button && height_button && depth_button && unit_button);
    TEST_ASSERT(position_button && rot_x_button && rot_y_button && rot_z_button);
    TEST_ASSERT(clear_button->bounds.w == delete_button->bounds.w);
    TEST_ASSERT(width_button->bounds.w == height_button->bounds.w);
    TEST_ASSERT(height_button->bounds.w == depth_button->bounds.w);
    TEST_ASSERT(depth_button->bounds.w == unit_button->bounds.w);
    TEST_ASSERT(rot_x_button->bounds.w == rot_y_button->bounds.w);
    TEST_ASSERT(rot_y_button->bounds.w == rot_z_button->bounds.w);
    TEST_ASSERT(position_button->bounds.x == ui->objectPane.transformRect.x);
    TEST_ASSERT(position_button->bounds.w == ui->objectPane.transformRect.w);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_edit_tab_exposes_selection_modes(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect workspace_rect = {0, 0, 0, 0};
    SDL_Rect selection_rect = {0, 0, 0, 0};
    const UIButton* body_button = NULL;
    const UIButton* edge_button = NULL;
    const UIButton* vertex_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_EDIT);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    TEST_ASSERT(UIPanel_GetEditPaneRects(ui,
                                         &summary_rect,
                                         &workspace_rect,
                                         &selection_rect));
    TEST_ASSERT(summary_rect.w > 0);
    TEST_ASSERT(summary_rect.h > 0);
    TEST_ASSERT(selection_rect.w > 0);
    TEST_ASSERT(selection_rect.h > 0);
    TEST_ASSERT(selection_rect.y >= workspace_rect.y + workspace_rect.h);

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_OBJECT_EDIT_BODY_MODE) {
            body_button = &ui->buttons[i];
        } else if (ui->buttons[i].id == UI_BTN_OBJECT_EDIT_EDGE_MODE) {
            edge_button = &ui->buttons[i];
        } else if (ui->buttons[i].id == UI_BTN_OBJECT_EDIT_VERTEX_MODE) {
            vertex_button = &ui->buttons[i];
        }
    }
    TEST_ASSERT(body_button != NULL);
    TEST_ASSERT(edge_button != NULL);
    TEST_ASSERT(vertex_button != NULL);
    TEST_ASSERT(body_button->bounds.x >= selection_rect.x);
    TEST_ASSERT(vertex_button->bounds.x + vertex_button->bounds.w <=
                selection_rect.x + selection_rect.w);
    TEST_ASSERT(edge_button->bounds.y >= selection_rect.y);
    TEST_ASSERT(edge_button->bounds.y + edge_button->bounds.h <=
                selection_rect.y + selection_rect.h);

    TEST_ASSERT(UIPanel_HandleClick(edge_button->bounds.x + edge_button->bounds.w / 2,
                                    edge_button->bounds.y + edge_button->bounds.h / 2));
    TEST_ASSERT(state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_EDGE);
    TEST_ASSERT(UIPanel_HandleClick(vertex_button->bounds.x + vertex_button->bounds.w / 2,
                                    vertex_button->bounds.y + vertex_button->bounds.h / 2));
    TEST_ASSERT(state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_VERTEX);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_light_selection_uses_authoring_inspector_controls(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect details_rect = {0, 0, 0, 0};
    SDL_Rect actions_rect = {0, 0, 0, 0};
    SDL_Rect prism_rect = {0, 0, 0, 0};
    SDL_Rect gizmo_rect = {0, 0, 0, 0};
    const UIButton* edit_button = NULL;
    const UIButton* enabled_button = NULL;
    const UIButton* kind_button = NULL;
    const UIButton* path_button = NULL;
    const UIButton* position_mode_button = NULL;
    const UIButton* color_button = NULL;
    const UIButton* intensity_button = NULL;
    const UIButton* size_button = NULL;
    const UIButton* cone_button = NULL;
    const UIButton* falloff_button = NULL;
    const UIButton* path_kind_button = NULL;
    const UIButton* material_color_button = NULL;
    const UIButton* object_clear_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);

    TEST_ASSERT(Layout_SceneAuthoringState_Select(&state->layout.sceneAuthoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  0u));
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    TEST_ASSERT(UIPanel_SceneAuthoringInspectorHasSelection());
    TEST_ASSERT(UIPanel_SceneAuthoringInspectorReservedHeight(ui) > 0);
    TEST_ASSERT(UIPanel_SceneAuthoringInspectorDetailsHeight(ui) > 0);
    TEST_ASSERT(UIPanel_GetObjectPaneRects(ui,
                                           &summary_rect,
                                           &details_rect,
                                           &actions_rect,
                                           &prism_rect,
                                           &gizmo_rect,
                                           NULL));
    TEST_ASSERT(summary_rect.h == UIPanel_SceneAuthoringInspectorReservedHeight(ui));
    TEST_ASSERT(details_rect.h <= UIPanel_SceneAuthoringInspectorDetailsHeight(ui));

    edit_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_EDIT_MODE);
    enabled_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_ENABLED);
    kind_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_KIND);
    path_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_PATH);
    position_mode_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_POSITION_MODE);
    color_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_COLOR);
    intensity_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_INTENSITY);
    size_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_SIZE);
    cone_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_CONE);
    falloff_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_LIGHT_FALLOFF);
    object_clear_button = ld_test_find_button(ui, UI_BTN_OBJECT_CLEAR_SELECTION);
    TEST_ASSERT(edit_button && enabled_button && kind_button && path_button &&
                position_mode_button && color_button && intensity_button && size_button &&
                cone_button && falloff_button && object_clear_button);
    TEST_ASSERT(edit_button->bounds.y >= actions_rect.y);
    TEST_ASSERT(enabled_button->bounds.y >= actions_rect.y);
    TEST_ASSERT(kind_button->bounds.y >= prism_rect.y);
    TEST_ASSERT(path_button->bounds.y >= gizmo_rect.y);
    TEST_ASSERT(position_mode_button->bounds.w > 0 && color_button->bounds.w > 0 &&
                intensity_button->bounds.w > 0 && size_button->bounds.w > 0 &&
                cone_button->bounds.w > 0 && falloff_button->bounds.w > 0);
    TEST_ASSERT(object_clear_button->bounds.w == 0);

    TEST_ASSERT(ld_test_click_button_center(edit_button));
    TEST_ASSERT(state->editor.sceneAuthoringEditMode == SCENE_AUTHORING_EDIT_MODE_LIGHT);
    TEST_ASSERT(ld_test_click_button_center(edit_button));
    TEST_ASSERT(state->editor.sceneAuthoringEditMode == SCENE_AUTHORING_EDIT_MODE_NONE);

    TEST_ASSERT(state->layout.sceneAuthoring.lights[0].enabled);
    TEST_ASSERT(ld_test_click_button_center(enabled_button));
    TEST_ASSERT(!state->layout.sceneAuthoring.lights[0].enabled);

    TEST_ASSERT(state->layout.sceneAuthoring.lights[0].kind ==
                LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL);
    TEST_ASSERT(ld_test_click_button_center(kind_button));
    TEST_ASSERT(state->layout.sceneAuthoring.lights[0].kind ==
                LINE_DRAWING_SCENE_LIGHT_POINT);
    {
        const size_t undo_before = Editor_UndoCount(&state->editor);
        TEST_ASSERT(ld_test_click_button_center(position_mode_button));
        TEST_ASSERT(state->layout.sceneAuthoring.lights[0].position_mode ==
                    LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START);
        TEST_ASSERT(ld_test_click_button_center(color_button));
        TEST_ASSERT(ld_test_nearly_equal(state->layout.sceneAuthoring.lights[0].color_rgb[1],
                                         0.78f));
        TEST_ASSERT(ld_test_click_button_center(intensity_button));
        TEST_ASSERT(ld_test_nearly_equal(state->layout.sceneAuthoring.lights[0].intensity, 2.0f));
        TEST_ASSERT(ld_test_click_button_center(size_button));
        TEST_ASSERT(ld_test_nearly_equal(state->layout.sceneAuthoring.lights[0].radius, 0.5f));
        TEST_ASSERT(ld_test_click_button_center(cone_button));
        TEST_ASSERT(ld_test_nearly_equal(
            state->layout.sceneAuthoring.lights[0].outer_cone_degrees, 60.0f));
        TEST_ASSERT(ld_test_click_button_center(falloff_button));
        TEST_ASSERT(state->layout.sceneAuthoring.lights[0].falloff ==
                    LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR);
        TEST_ASSERT(Editor_UndoCount(&state->editor) == undo_before + 6u);
    }
    TEST_ASSERT(ld_test_click_button_center(path_button));
    TEST_ASSERT(state->layout.sceneAuthoring.lights[0].path_id[0] == '\0');
    TEST_ASSERT(state->layout.sceneAuthoring.lights[0].position_mode ==
                LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT);
    TEST_ASSERT(state->layout.sceneAuthoring.paths[1].bound_light_id[0] == '\0');

    TEST_ASSERT(Layout_SceneAuthoringState_Select(&state->layout.sceneAuthoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    path_kind_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_PATH_KIND);
    TEST_ASSERT(path_kind_button && path_kind_button->bounds.w > 0);
    TEST_ASSERT(ld_test_click_button_center(path_kind_button));
    TEST_ASSERT(strcmp(state->layout.sceneAuthoring.paths[0].curve_type, "linear") == 0);

    TEST_ASSERT(Layout_SceneAuthoringState_Select(&state->layout.sceneAuthoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL,
                                                  0u));
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    material_color_button = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_MATERIAL_COLOR);
    TEST_ASSERT(material_color_button && material_color_button->bounds.w > 0);
    TEST_ASSERT(ld_test_click_button_center(material_color_button));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.sceneAuthoring.materials[0].rgba[0], 0.45f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_camera_path_exposes_camera_controls(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* orientation = NULL;
    const UIButton* roll = NULL;
    const UIButton* fov = NULL;
    const UIButton* clip = NULL;
    size_t undo_before = 0u;
    ld_test_init_runtime();
    state = Global_Get();
    ui = UIPanel_Get();
    TEST_ASSERT(state && ui);
    TEST_ASSERT(Layout_SceneAuthoringState_Select(&state->layout.sceneAuthoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    orientation = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_CAMERA_ORIENTATION);
    roll = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_CAMERA_ROLL);
    fov = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_CAMERA_FOV);
    clip = ld_test_find_button(ui, UI_BTN_SCENE_AUTHORING_CAMERA_CLIP);
    TEST_ASSERT(orientation && roll && fov && clip);
    TEST_ASSERT(orientation->bounds.w > 0 && roll->bounds.w > 0 &&
                fov->bounds.w > 0 && clip->bounds.w > 0);
    undo_before = Editor_UndoCount(&state->editor);
    TEST_ASSERT(ld_test_click_button_center(orientation));
    TEST_ASSERT(state->layout.sceneAuthoring.cameras[0].orientation_mode ==
                LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET);
    TEST_ASSERT(ld_test_click_button_center(roll));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.sceneAuthoring.cameras[0].roll_degrees,
                                     15.0f));
    TEST_ASSERT(ld_test_click_button_center(fov));
    TEST_ASSERT(ld_test_nearly_equal(
        state->layout.sceneAuthoring.cameras[0].vertical_fov_degrees, 65.0f));
    TEST_ASSERT(ld_test_click_button_center(clip));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.sceneAuthoring.cameras[0].near_clip, 0.5f));
    TEST_ASSERT(Editor_UndoCount(&state->editor) == undo_before + 4u);
    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_object_inspector_run_tests(void) {
    const TestCase cases[] = {
        { "object_inspector_reserves_space_for_selected_object",
          test_object_inspector_reserves_space_for_selected_object },
        { "object_inspector_layout_stays_stable_when_selection_changes",
          test_object_inspector_layout_stays_stable_when_selection_changes },
        { "object_inspector_sections_fit_inside_pane",
          test_object_inspector_sections_fit_inside_pane },
        { "object_inspector_lower_sections_anchor_to_bottom",
          test_object_inspector_lower_sections_anchor_to_bottom },
        { "object_buttons_use_uniform_grid_rows",
          test_object_buttons_use_uniform_grid_rows },
        { "object_edit_tab_exposes_selection_modes",
          test_object_edit_tab_exposes_selection_modes },
        { "scene_authoring_light_selection_uses_authoring_inspector_controls",
          test_scene_authoring_light_selection_uses_authoring_inspector_controls },
        { "scene_authoring_camera_path_exposes_camera_controls",
          test_scene_authoring_camera_path_exposes_camera_controls },
    };
    return run_test_cases("UIPanelObjectInspector", cases, sizeof(cases) / sizeof(cases[0]));
}
