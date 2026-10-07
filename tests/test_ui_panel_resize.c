#include "test_layout_internal.h"
#include "Input/input_mouse.h"
#include "Input/input_keyboard.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_routes.h"
#include "Layout/layout_saved_views.h"

/* Exercise the native input dispatcher: changing a solved pane alone does not
 * prove that the controls and their hit targets received the new bounds. */
static bool drag_side(bool right, int delta) {
    GlobalState* state = Global_Get();
    CorePaneRect pane = {0};
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&state->paneHost,
        right ? LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS : LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS,
        &pane));
    int x = (int)(right ? pane.x : pane.x + pane.width);
    int y = (int)(pane.y + pane.height * .5f);
    SDL_Event event = {.type = SDL_MOUSEBUTTONDOWN};
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(LineDrawingPaneHost_IsSplitterDragActive(&state->paneHost));
    event = (SDL_Event){.type = SDL_MOUSEMOTION};
    event.motion.x = x + delta;
    event.motion.y = y;
    event.motion.state = SDL_BUTTON_LMASK;
    Input_MouseHandle(NULL, &event);
    /* Identical motion must not invalidate layout/picking again. */
    state->hitboxDirty = false;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(!state->hitboxDirty);
    event = (SDL_Event){.type = SDL_MOUSEBUTTONUP};
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x + delta;
    event.button.y = y;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(!LineDrawingPaneHost_IsSplitterDragActive(&state->paneHost));
    return true;
}

static bool rect_inside(SDL_Rect inner, SDL_Rect outer) {
    return inner.w > 0 && inner.h > 0 && inner.x >= outer.x && inner.y >= outer.y &&
        inner.x + inner.w <= outer.x + outer.w && inner.y + inner.h <= outer.y + outer.h;
}

static bool test_left_resize_selection_and_file_controls(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Global_SetWindowSize(1600, 1200);
    UIPanelState* ui = UIPanel_Get();
    RectPrismPrimitiveCreateParams p = {
        .width = 1, .height = 1, .depth = 1, .useExplicitFrame = true,
        .explicitFrame = {.axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    uint32_t id = 0;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &p, &id, NULL));
    Editor_ClearHistory(&state->editor);
    state->layoutDirtySinceSave = false;
    char* before = Layout_SaveToString(&state->layout);
    SDL_Rect old_list = ui->scenePane.listRect;
    TEST_ASSERT(drag_side(false, 190));
    TEST_ASSERT(ui->scenePane.listRect.w > old_list.w + 150);
    TEST_ASSERT(rect_inside(ui->scenePane.listRect, ui->leftBodyRect));
    /* Click in the newly exposed part of the first row, beyond the old width. */
    TEST_ASSERT(UIPanel_HandleSceneListClick(ui->scenePane.listRect.x + ui->scenePane.listRect.w - 20,
                                           ui->scenePane.listRect.y + 15));
    TEST_ASSERT(state->editor.selectedObject3DId == id);
    UIPanel_SetActiveLeftTab(ui, UI_PANEL_LEFT_TAB_FILE);
    Global_RefreshPaneLayout(NULL);
    int file_width = ui->filePane.summaryRect.w;
    TEST_ASSERT(drag_side(false, 80));
    TEST_ASSERT(ui->filePane.summaryRect.w > file_width + 60);
    TEST_ASSERT(rect_inside(ui->filePane.summaryRect, ui->leftBodyRect));
    TEST_ASSERT(drag_side(false, -200));
    TEST_ASSERT(ui->filePane.summaryRect.w < file_width);
    char* after = Layout_SaveToString(&state->layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    TEST_ASSERT(!state->layoutDirtySinceSave && !Editor_UndoCount(&state->editor));
    Layout_FreeString(before);
    Layout_FreeString(after);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_right_resize_all_tabs_and_controls(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Global_SetWindowSize(1900, 1200);
    RectPrismPrimitiveCreateParams p = {
        .width = 1, .height = 1, .depth = 1, .useExplicitFrame = true,
        .explicitFrame = {.axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    uint32_t id;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &p, &id, NULL));
    p.explicitFrame.origin.x = 2;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &p, &id, NULL));
    UIPanelState* ui = UIPanel_Get();
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.target = (Vec3){2, 3, 4};
    char* before = Layout_SaveToString(&state->layout);
    Editor_ClearHistory(&state->editor);
    state->layoutDirtySinceSave = false;
    for (int tab = 0; tab < UI_PANEL_RIGHT_TAB_COUNT; ++tab) {
        UIPanel_SetActiveRightTab(ui, (UIPanelRightTab)tab);
        if (tab == UI_PANEL_RIGHT_TAB_MEASURE) TEST_ASSERT(UIPanel_BeginMeasurement());
        Global_RefreshPaneLayout(NULL);
        SDL_Rect body = ui->rightBodyRect;
        CorePaneRect previous_viewport;
        TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&state->paneHost, &previous_viewport));
        SpaceViewContext previous_view = SpaceAdapter_BuildViewContext(state);
        Vec2 previous_screen = WorldToScreen(SpaceAdapter_ProjectToView((Vec3){1, 2, 3}, &previous_view), &state->grid);
        TEST_ASSERT(drag_side(true, -110));
        TEST_ASSERT(ui->rightBodyRect.w > body.w + 85);
        CorePaneRect pane;
        TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&state->paneHost,
                    LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &pane));
        TEST_ASSERT(abs(ui->rightPaneRect.x - (int)pane.x) <= 1);
        TEST_ASSERT(abs(ui->rightPaneRect.w - (int)pane.width) <= 1);
        int previous_end = ui->rightPaneRect.x;
        for (int i = 0; i < UI_PANEL_RIGHT_TAB_COUNT; ++i) {
            SDL_Rect r = ui->rightTabs[i].bounds;
            if (!r.w) continue;
            TEST_ASSERT(rect_inside(r, ui->rightPaneRect));
            TEST_ASSERT(r.x >= previous_end);
            previous_end = r.x + r.w;
        }
        SDL_Rect control = {0};
        if (tab == UI_PANEL_RIGHT_TAB_MEASURE) {
            TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_UNITS, &control));
            TEST_ASSERT(rect_inside(control, ui->rightBodyRect));
            TEST_ASSERT(control.w > body.w / 2);
        } else if (tab == UI_PANEL_RIGHT_TAB_ROUTES) {
            TEST_ASSERT(UIPanel_RoutesControlRect(ROUTES_NEW, &control));
            TEST_ASSERT(rect_inside(control, ui->rightBodyRect));
            TEST_ASSERT(control.w > body.w / 2);
        } else if (tab == UI_PANEL_RIGHT_TAB_CREATE) {
            TEST_ASSERT(rect_inside(ui->createPane.summaryRect, ui->rightBodyRect));
            TEST_ASSERT(ui->createPane.summaryRect.w > body.w);
        }
        CorePaneRect viewport;
        TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport));
        SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
        Vec2 screen = WorldToScreen(SpaceAdapter_ProjectToView((Vec3){1, 2, 3}, &view), &state->grid);
        TEST_ASSERT(fabsf((screen.x - viewport.x - viewport.width * .5f) -
                          (previous_screen.x - previous_viewport.x - previous_viewport.width * .5f)) < .01f);
        TEST_ASSERT(fabsf((screen.y - viewport.y - viewport.height * .5f) -
                          (previous_screen.y - previous_viewport.y - previous_viewport.height * .5f)) < .01f);
        TEST_ASSERT(drag_side(true, 110));
        TEST_ASSERT(abs(ui->rightBodyRect.w - body.w) <= 2);
    }
    char* after = Layout_SaveToString(&state->layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    TEST_ASSERT(!state->layoutDirtySinceSave && !Editor_UndoCount(&state->editor));
    Layout_FreeString(before);
    Layout_FreeString(after);
    ld_test_shutdown_runtime();
    return true;
}

static bool click_part(int action) {
    SDL_Rect rect;
    TEST_ASSERT(UIPanel_PartsControlRect(action, &rect));
    TEST_ASSERT(rect_inside(rect, UIPanel_Get()->rightBodyRect));
    TEST_ASSERT(UIPanel_PartsClick(rect.x + rect.w / 2, rect.y + rect.h / 2));
    return true;
}

static bool test_visibility_name_space_management_and_undo(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Global_SetWindowSize(1600, 1200);
    LayoutSavedView view = {.id = "furniture", .name = "Furniture and construction", .query_count = 1};
    snprintf(view.queries[0].entity_type, sizeof(view.queries[0].entity_type), "Panel");
    TEST_ASSERT(Layout_EditSavedView(&state->layout, &view, NULL, Layout_GeometryHistory, NULL));
    UIPanel_PartsEnterMode(2);
    UIPanelState* ui = UIPanel_Get();
    ui->parts.views_manage_open = false;
    ui->parts.custom_filter_open = false;
    Global_RefreshPaneLayout(NULL);
    TEST_ASSERT(drag_side(true, -230));
    SDL_Rect name, only, removed;
    TEST_ASSERT(UIPanel_PartsControlRect(8000, &name));
    TEST_ASSERT(UIPanel_PartsControlRect(8100, &only));
    TEST_ASSERT(name.w > only.w * 3 && name.x + name.w < only.x);
    TEST_ASSERT(rect_inside(name, ui->rightBodyRect) && rect_inside(only, ui->rightBodyRect));
    TEST_ASSERT(!UIPanel_PartsControlRect(8200, &removed));
    TEST_ASSERT(click_part(8000) && Layout_ViewHidden(&state->layout.objectStore, view.id));
    TEST_ASSERT(click_part(8100) && !strcmp(state->layout.objectStore.isolated_view_id, view.id));
    int old_name_width = name.w;
    TEST_ASSERT(drag_side(true, -90));
    TEST_ASSERT(UIPanel_PartsControlRect(8000, &name) && name.w > old_name_width + 70);
    TEST_ASSERT(UIPanel_PartsControlRect(8100, &removed) && removed.w == only.w);
    TEST_ASSERT(click_part(PARTS_MANAGE_VIEWS));
    TEST_ASSERT(UIPanel_PartsControlRect(8200, &removed) && removed.w > name.w);
    Editor_ClearHistory(&state->editor);
    TEST_ASSERT(click_part(8200) && !state->layout.objectStore.saved_view_count);
    TEST_ASSERT(Editor_UndoCount(&state->editor) == 1);
    TEST_ASSERT(Editor_Undo(&state->editor, &state->layout));
    TEST_ASSERT(state->layout.objectStore.saved_view_count == 1);
    TEST_ASSERT(!strcmp(state->layout.objectStore.saved_views[0].id, view.id));
    ld_test_shutdown_runtime();
    return true;
}

static bool test_window_resize_and_font_metrics_refresh(void) {
    TEST_ASSERT(FontManager_Init());
    TEST_ASSERT(FontManager_LoadFonts());
    int saved_step = FontManager_GetZoomStep();
    TEST_ASSERT(FontManager_SetZoomStep(0));
    ld_test_init_runtime();
    Global_SetWindowSize(1600, 1200);
    UIPanelState* ui = UIPanel_Get();
    int tab_height = ui->rightTabs[UI_PANEL_RIGHT_TAB_VIEW].bounds.h;
    int font_height = TTF_FontHeight(FontManager_GetUIPanelFont());
    SDL_Keymod saved_mods = SDL_GetModState();
    SDL_SetModState(KMOD_GUI);
    AppContext ctx = {0};
    SDL_Event event = {.type = SDL_KEYDOWN};
    event.key.keysym.sym = SDLK_EQUALS;
    Input_KeyboardHandle(&ctx, &event);
    Input_KeyboardHandle(&ctx, &event);
    Input_KeyboardHandle(&ctx, &event);
    SDL_SetModState(saved_mods);
    TEST_ASSERT(FontManager_GetZoomStep() == 3);
    TEST_ASSERT(TTF_FontHeight(FontManager_GetUIPanelFont()) > font_height);
    TEST_ASSERT(ui->rightTabs[UI_PANEL_RIGHT_TAB_VIEW].bounds.h > tab_height);
    TEST_ASSERT(drag_side(false, 80) && drag_side(true, -80));
    Global_SetWindowSize(1300, 900);
    CorePaneRect pane;
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&Global_Get()->paneHost,
                LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &pane));
    TEST_ASSERT(abs(ui->rightPaneRect.x - (int)pane.x) <= 1);
    TEST_ASSERT(abs(ui->rightPaneRect.w - (int)pane.width) <= 1);
    Global_Get()->hitboxDirty = false;
    Global_SetWindowSize(1300, 900);
    TEST_ASSERT(!Global_Get()->hitboxDirty);
    ld_test_shutdown_runtime();
    TEST_ASSERT(FontManager_SetZoomStep(saved_step));
    FontManager_Quit();
    return true;
}

bool ui_panel_resize_run_tests(void) {
    const TestCase cases[] = {
        {"LeftDividerReflowsSceneAndFileHitTargets", test_left_resize_selection_and_file_controls},
        {"RightDividerReflowsEveryTabReadOnly", test_right_resize_all_tabs_and_controls},
        {"VisibilityUsesNameSpaceAndSeparateUndoableManagement", test_visibility_name_space_management_and_undo},
        {"WindowResizeAndFontSizeExplicitlyRefreshControls", test_window_resize_and_font_metrics_refresh}
    };
    return run_test_cases("UIPanelResize", cases, sizeof(cases) / sizeof(cases[0]));
}
