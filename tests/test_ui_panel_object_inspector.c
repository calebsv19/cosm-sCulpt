#include "test_layout_internal.h"

#include "UI/ui_panel_object_inspector.h"
#include "UI/ui_panel_object_layout.h"

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
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_OBJECT;
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
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_OBJECT;
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
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_OBJECT;
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
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_OBJECT;
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
    };
    return run_test_cases("UIPanelObjectInspector", cases, sizeof(cases) / sizeof(cases[0]));
}
