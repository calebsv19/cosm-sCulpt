#include "test_layout_internal.h"

#include "UI/input_ui_panel.h"
#include "UI/font_manager.h"

static void ld_test_scene_list_first_row_point(const UIPanelState* ui, int* out_x, int* out_y) {
    int font_h = 14;
    int line_gap = 4;
    int header_h = 0;
    int row_h = 0;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (font) font_h = TTF_FontHeight(font);
    if (font_h < 12) font_h = 12;
    line_gap = font_h / 3;
    if (line_gap < 4) line_gap = 4;
    header_h = (font_h * 2) + line_gap + 12;
    row_h = (font_h * 2) + 12;
    if (row_h < 34) row_h = 34;
    if (out_x) *out_x = ui->leftBodyRect.x + 16;
    if (out_y) *out_y = ui->leftBodyRect.y + header_h + (row_h / 2);
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

bool ui_panel_scene_list_run_tests(void) {
    const TestCase cases[] = {
        { "scene_list_click_selects_first_object",
          test_scene_list_click_selects_first_object },
    };
    return run_test_cases("UIPanelSceneList", cases, sizeof(cases) / sizeof(cases[0]));
}
