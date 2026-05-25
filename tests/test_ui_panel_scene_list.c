#include "test_layout_internal.h"

#include "UI/input_ui_panel.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_scene_layout.h"
#include "UI/ui_panel_scene_summary.h"
#include "UI/ui_panel_scene_list.h"

static void ld_test_scene_list_first_row_point(const UIPanelState* ui, int* out_x, int* out_y) {
    SDL_Rect list_rect = {0};
    int row_h = 0;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int font_h = 14;
    if (font) font_h = TTF_FontHeight(font);
    if (font_h < 12) font_h = 12;
    row_h = (font_h * 3) + 16;
    if (row_h < 50) row_h = 50;
    if (!UIPanel_GetScenePaneRects(ui, NULL, &list_rect, NULL, NULL)) {
        if (out_x) *out_x = 0;
        if (out_y) *out_y = 0;
        return;
    }
    if (out_x) *out_x = list_rect.x + 16;
    if (out_y) *out_y = list_rect.y + (row_h / 2);
}

static bool ld_test_find_button_center(const UIPanelState* ui,
                                       int button_id,
                                       int* out_x,
                                       int* out_y) {
    if (!ui) return false;
    for (int i = 0; i < ui->count; ++i) {
        const UIButton* button = &ui->buttons[i];
        if (button->id != button_id) continue;
        if (button->bounds.w <= 0 || button->bounds.h <= 0) return false;
        if (out_x) *out_x = button->bounds.x + (button->bounds.w / 2);
        if (out_y) *out_y = button->bounds.y + (button->bounds.h / 2);
        return true;
    }
    return false;
}

static bool test_scene_list_click_selects_first_object(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    RectPrismPrimitiveCreateParams prism = {0};
    uint32_t plane_id = 0u;
    uint32_t prism_id = 0u;
    bool adjusted = false;
    int click_x = 0;
    int click_y = 0;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    plane = (PlanePrimitiveCreateParams){
        .width = 10.0f,
        .height = 8.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 2.0f, 3.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    prism = (RectPrismPrimitiveCreateParams){
        .width = 6.0f,
        .height = 5.0f,
        .depth = 4.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 12.0f, 4.0f, 1.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };

    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &plane_id, &adjusted));
    TEST_ASSERT(plane_id != 0u);
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &prism, &prism_id, &adjusted));
    TEST_ASSERT(prism_id != 0u);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    ld_test_scene_list_first_row_point(ui, &click_x, &click_y);

    TEST_ASSERT(UIPanel_HandleClick(click_x, click_y));
    TEST_ASSERT(state->editor.selectedObject3DId == plane_id);
    TEST_ASSERT(ui->sceneList.expandedObjectId == 0u);
    TEST_ASSERT(ui->sceneList.lastClickedObjectId == plane_id);
    TEST_ASSERT(state->editor.selectedWallIndex == -1);
    TEST_ASSERT(state->editor.selectedAnchorIndex == -1);

    {
        const UIButton* bounds_button = NULL;
        for (int i = 0; i < ui->count; ++i) {
            if (ui->buttons[i].id == UI_BTN_TOGGLE_SCENE_BOUNDS) {
                bounds_button = &ui->buttons[i];
                break;
            }
        }
        TEST_ASSERT(bounds_button != NULL);
        TEST_ASSERT(bounds_button->side == UI_PANEL_LEFT);
        TEST_ASSERT(bounds_button->group == UI_PANEL_GROUP_LEFT_SCENE_BOUNDS);
        TEST_ASSERT(bounds_button->bounds.y > ui->leftBodyRect.y);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_list_scrollbar_drag_updates_scroll_offset(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    bool adjusted = false;
    uint32_t object_id = 0u;
    int list_y = 0;
    int track_x = 0;
    int track_y = 0;
    int drag_y = 0;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    for (int i = 0; i < 24; ++i) {
        plane = (PlanePrimitiveCreateParams){
            .width = 8.0f,
            .height = 6.0f,
            .useExplicitFrame = true,
            .explicitFrame = {
                .origin = { 1.0f + (float)i, 2.0f + (float)i, 0.0f },
                .axisU = { 1.0f, 0.0f, 0.0f },
                .axisV = { 0.0f, 1.0f, 0.0f },
                .normal = { 0.0f, 0.0f, 1.0f }
            },
            .lockToConstructionPlane = true,
            .lockToBounds = false
        };
        TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
        TEST_ASSERT(object_id != 0u);
    }

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    {
        SDL_Rect list_rect = {0};
        TEST_ASSERT(UIPanel_GetScenePaneRects(ui, NULL, &list_rect, NULL, NULL));
        list_y = list_rect.y;
        track_x = list_rect.x + list_rect.w - 5;
    }
    track_y = list_y + 8;
    drag_y = ui->scenePane.listRect.y + ui->scenePane.listRect.h - 24;

    TEST_ASSERT(UIPanel_HandleClick(track_x, track_y));
    TEST_ASSERT(ui->sceneList.scrollbarDragging);
    UIPanel_HandleMouseMotion(track_x, drag_y);
    TEST_ASSERT(ui->sceneList.scrollOffsetPx > 0.0f);
    UIPanel_HandleSceneListMouseUp();
    TEST_ASSERT(!ui->sceneList.scrollbarDragging);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_pane_sections_reserve_visible_list_space(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect summary_rect = {0};
    SDL_Rect list_rect = {0};
    SDL_Rect selection_rect = {0};
    SDL_Rect bounds_rect = {0};

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    TEST_ASSERT(UIPanel_GetScenePaneRects(ui, &summary_rect, &list_rect, &selection_rect, &bounds_rect));
    TEST_ASSERT(summary_rect.h > 0);
    TEST_ASSERT(list_rect.h > 40);
    TEST_ASSERT(list_rect.y >= summary_rect.y + summary_rect.h);
    TEST_ASSERT(selection_rect.y >= list_rect.y + list_rect.h);
    TEST_ASSERT(bounds_rect.y >= selection_rect.y + selection_rect.h - 1);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_list_selection_buttons_clear_and_delete(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    bool adjusted = false;
    uint32_t object_id = 0u;
    int clear_x = 0;
    int clear_y = 0;
    int delete_x = 0;
    int delete_y = 0;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    plane = (PlanePrimitiveCreateParams){
        .width = 9.0f,
        .height = 7.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 5.0f, 6.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
    TEST_ASSERT(object_id != 0u);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    state->editor.selectedObject3DId = object_id;
    TEST_ASSERT(ld_test_find_button_center(ui, UI_BTN_SCENE_CLEAR_SELECTION, &clear_x, &clear_y));
    TEST_ASSERT(UIPanel_HandleClick(clear_x, clear_y));
    TEST_ASSERT(state->editor.selectedObject3DId == 0u);

    state->editor.selectedObject3DId = object_id;
    TEST_ASSERT(ld_test_find_button_center(ui, UI_BTN_SCENE_DELETE_SELECTED, &delete_x, &delete_y));
    TEST_ASSERT(UIPanel_HandleClick(delete_x, delete_y));
    TEST_ASSERT(state->editor.selectedObject3DId == 0u);
    TEST_ASSERT(Layout_ObjectStore_FindConst(&state->layout.objectStore, object_id) == NULL);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_list_double_click_toggles_expanded_object(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    bool adjusted = false;
    uint32_t object_id = 0u;
    int click_x = 0;
    int click_y = 0;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    plane = (PlanePrimitiveCreateParams){
        .width = 11.0f,
        .height = 9.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 3.0f, 2.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
    TEST_ASSERT(object_id != 0u);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    ld_test_scene_list_first_row_point(ui, &click_x, &click_y);

    TEST_ASSERT(UIPanel_HandleClick(click_x, click_y));
    TEST_ASSERT(ui->sceneList.expandedObjectId == 0u);
    TEST_ASSERT(UIPanel_HandleClick(click_x, click_y));
    TEST_ASSERT(ui->sceneList.expandedObjectId == object_id);
    TEST_ASSERT(UIPanel_HandleClick(click_x, click_y));
    TEST_ASSERT(ui->sceneList.expandedObjectId == object_id);
    TEST_ASSERT(UIPanel_HandleClick(click_x, click_y));
    TEST_ASSERT(ui->sceneList.expandedObjectId == 0u);
    TEST_ASSERT(state->editor.selectedObject3DId == object_id);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_scene_list_run_tests(void) {
    const TestCase cases[] = {
        { "scene_list_click_selects_first_object",
          test_scene_list_click_selects_first_object },
        { "scene_list_scrollbar_drag_updates_scroll_offset",
          test_scene_list_scrollbar_drag_updates_scroll_offset },
        { "scene_list_selection_buttons_clear_and_delete",
          test_scene_list_selection_buttons_clear_and_delete },
        { "scene_list_double_click_toggles_expanded_object",
          test_scene_list_double_click_toggles_expanded_object },
        { "scene_pane_sections_reserve_visible_list_space",
          test_scene_pane_sections_reserve_visible_list_space },
    };
    return run_test_cases("UIPanelSceneList", cases, sizeof(cases) / sizeof(cases[0]));
}
