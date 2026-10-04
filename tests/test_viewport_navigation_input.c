#include "test_layout_internal.h"

#include "Core/line_drawing_pane_host.h"
#include "Input/input_mouse.h"
#include "Input/input_handler.h"
#include "Input/input_keyboard.h"
#include "Input/input_editor_actions.h"
#include "Input/input_mouse_drag.h"
#include "Input/input_viewport_navigation.h"
#include "Core/viewport_zoom.h"
#include "UI/input_ui_panel.h"
#include "Layout/layout_engineering.h"

#include <SDL2/SDL.h>

static bool test_alt_lmb_orbit_requires_button_and_preserves_target(void) {
    GlobalState* state = NULL;
    CorePaneRect viewport = {0};
    SDL_Event event = {0};
    SDL_Keymod saved_mods = SDL_GetModState();
    Vec3 target_before = {0};
    float yaw_before = 0.0f;
    int cx = 400;
    int cy = 300;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.target = (Vec3){ 7.0f, 8.0f, 9.0f };
    target_before = state->freeViewCamera.target;
    yaw_before = state->freeViewCamera.yawDeg;
    if (LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport)) {
        cx = (int)(viewport.x + viewport.width * 0.5f);
        cy = (int)(viewport.y + viewport.height * 0.5f);
    }

    SDL_SetModState(KMOD_ALT);
    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx;
    event.motion.y = cy;
    event.motion.xrel = 20;
    event.motion.yrel = 10;
    event.motion.state = 0;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(ld_test_nearly_equal(state->freeViewCamera.yawDeg, yaw_before));

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = cx;
    event.button.y = cy;
    event.button.clicks = 1;
    Input_MouseHandle(NULL, &event);

    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx + 20;
    event.motion.y = cy + 10;
    event.motion.xrel = 20;
    event.motion.yrel = 10;
    event.motion.state = SDL_BUTTON_LMASK;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(!ld_test_nearly_equal(state->freeViewCamera.yawDeg, yaw_before));
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->freeViewCamera.target, target_before));

    event.type = SDL_MOUSEBUTTONUP;
    event.button.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = cx + 20;
    event.button.y = cy + 10;
    Input_MouseHandle(NULL, &event);
    SDL_SetModState(saved_mods);
    InputViewportNavigation_ResetGesture();
    ld_test_shutdown_runtime();
    return true;
}

static bool test_middle_drag_pans_free_view_target_and_release_stops(void) {
    GlobalState* state = NULL;
    CorePaneRect viewport = {0};
    SDL_Event event = {0};
    Vec3 target_before = {0};
    Vec3 target_after = {0};
    int cx = 400;
    int cy = 300;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.target = (Vec3){ 3.0f, 4.0f, 5.0f };
    target_before = state->freeViewCamera.target;
    if (LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport)) {
        cx = (int)(viewport.x + viewport.width * 0.5f);
        cy = (int)(viewport.y + viewport.height * 0.5f);
    }

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_MIDDLE;
    event.button.x = cx;
    event.button.y = cy;
    Input_MouseHandle(NULL, &event);

    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx + 24;
    event.motion.y = cy - 12;
    event.motion.xrel = 24;
    event.motion.yrel = -12;
    event.motion.state = SDL_BUTTON_MMASK;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(!ld_test_vec3_nearly_equal(state->freeViewCamera.target, target_before));
    target_after = state->freeViewCamera.target;

    event.type = SDL_MOUSEBUTTONUP;
    event.button.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_MIDDLE;
    event.button.x = cx + 24;
    event.button.y = cy - 12;
    Input_MouseHandle(NULL, &event);

    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx + 36;
    event.motion.y = cy;
    event.motion.xrel = 12;
    event.motion.yrel = 12;
    event.motion.state = 0;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->freeViewCamera.target, target_after));

    InputViewportNavigation_ResetGesture();
    ld_test_shutdown_runtime();
    return true;
}

static bool test_fit_scene_button_physical_zoom_read_only(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Global_SetWindowSize(1400, 1000);
    UIPanel_OnWindowResized(1400, 1000);
    state->freeViewCamera.enabled = false;
    state->activePlane.axis = VIEW_PLANE_XY;
    state->grid.gridSize = .1f;
    state->layout.gridSize = .1f;
    RectPrismPrimitiveCreateParams p = {
        .width = 1.85f,
        .height = 3.65f,
        .depth = 1.9f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = {3, 4, .95f}, .axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    uint32_t id;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &p, &id, NULL));
    Editor_ClearHistory(&state->editor);
    state->layoutDirtySinceSave = false;
    char* before = Layout_SaveToString(&state->layout);
    UIPanel_SetActiveRightTab(UIPanel_Get(), UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(1400, 1000);
    const UIButton* fit = NULL;
    for (int i = 0; i < UIPanel_Get()->count; ++i)
        if (UIPanel_Get()->buttons[i].id == UI_BTN_FIT_SCENE)
            fit = &UIPanel_Get()->buttons[i];
    TEST_ASSERT(fit && fit->bounds.w > 0 && fit->bounds.h > 0);
    SDL_Event e = {.type = SDL_MOUSEBUTTONDOWN};
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = fit->bounds.x + fit->bounds.w / 2;
    e.button.y = fit->bounds.y + fit->bounds.h / 2;
    TEST_ASSERT(UIPanel_HandleClick(e.button.x, e.button.y));
    CorePaneRect r;
    TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&state->paneHost, &r));
    SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
    Vec3 low, high;
    TEST_ASSERT(Layout_Object3D_ComputeWorldAABB(&state->layout.objectStore.items[0], &low, &high));
    for (int i = 0; i < 8; ++i) {
        Vec2 screen = WorldToScreen(
            SpaceAdapter_ProjectToView(
                (Vec3){i & 1 ? high.x : low.x, i & 2 ? high.y : low.y, i & 4 ? high.z : low.z}, &view),
            &state->grid);
        TEST_ASSERT(screen.x > r.x && screen.x < r.x + r.width && screen.y > r.y &&
                    screen.y < r.y + r.height);
    }
    float fitted = state->grid.scale;
    TEST_ASSERT(fitted > 100);
    TEST_ASSERT(LineDrawingViewportZoom_Apply(state, 1.1f, r.x + r.width * .5f, r.y + r.height * .5f) &&
                state->grid.scale > fitted);
    char* after = Layout_SaveToString(&state->layout);
    TEST_ASSERT(!strcmp(before, after) && !state->layoutDirtySinceSave && !Editor_UndoCount(&state->editor));
    Layout_FreeString(before);
    Layout_FreeString(after);
    state->layout.objectStore.items[0].coreMeta.flags.visible = false;
    float scale = state->grid.scale;
    TEST_ASSERT(!LineDrawingViewportZoom_FitVisibleGeometry(state) && state->grid.scale == scale);
    state->layout.objectStore.items[0].coreMeta.flags.visible = true;
    state->freeViewCamera.enabled = true;
    TEST_ASSERT(LineDrawingViewportZoom_FitVisibleGeometry(state));
    TEST_ASSERT(fabs(state->freeViewCamera.target.x - 3) < 1e-5 &&
                fabs(state->freeViewCamera.target.y - 4) < 1e-5);
    ld_test_shutdown_runtime();
    return true;
}

static bool click_tool(int id, UIPanelRightTab tab) {
    UIPanel_SetActiveRightTab(UIPanel_Get(), tab);
    UIPanel_OnWindowResized(Global_Get()->screenWidth, Global_Get()->screenHeight);
    for (int i = 0; i < UIPanel_Get()->count; ++i) {
        const UIButton* b = &UIPanel_Get()->buttons[i];
        if (b->id == id && b->bounds.w > 0 && b->bounds.h > 0)
            return UIPanel_HandleClick(b->bounds.x + b->bounds.w / 2,
                                      b->bounds.y + b->bounds.h / 2);
    }
    return false;
}

static void viewport_click(int x, int y, Uint8 button) {
    SDL_Event e = {.type = SDL_MOUSEBUTTONDOWN};
    e.button.button = button;
    e.button.x = x;
    e.button.y = y;
    e.button.clicks = 1;
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
}

static bool test_line_creation_is_explicit_cancelable_and_deletable(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Editor_ClearHistory(&state->editor);
    Global_SetWindowSize(1600, 1000);
    UIPanel_OnWindowResized(1600, 1000);
    CorePaneRect r;
    TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&state->paneHost, &r));
    int x = (int)(r.x + r.width * .35f), y = (int)(r.y + r.height * .4f);
    char* before = Layout_SaveToString(&state->layout);
    viewport_click(x, y, SDL_BUTTON_RIGHT);
    viewport_click(x, y, SDL_BUTTON_LEFT);
    TEST_ASSERT(state->editor.mode == TOOL_IDLE && state->layout.anchorCount == 0 &&
                state->layout.wallCount == 0 && Editor_UndoCount(&state->editor) == 0);
    TEST_ASSERT(click_tool(UI_BTN_DRAW_LINE, UI_PANEL_RIGHT_TAB_CREATE));
    viewport_click(x, y, SDL_BUTTON_LEFT);
    TEST_ASSERT(state->editor.mode == TOOL_PLACING_WALL);
    TEST_ASSERT(state->layout.anchorCount == 0 && Editor_UndoCount(&state->editor) == 0);
    TEST_ASSERT(click_tool(UI_BTN_STOP_DRAWING, UI_PANEL_RIGHT_TAB_CREATE));
    TEST_ASSERT(state->editor.mode == TOOL_IDLE && state->editor.viewportTool == VIEWPORT_TOOL_SELECT);
    TEST_ASSERT(click_tool(UI_BTN_DRAW_LINE, UI_PANEL_RIGHT_TAB_CREATE));
    viewport_click(x, y, SDL_BUTTON_LEFT);
    viewport_click(x, y, SDL_BUTTON_RIGHT);
    TEST_ASSERT(state->editor.mode == TOOL_IDLE && state->editor.viewportTool == VIEWPORT_TOOL_SELECT);
    TEST_ASSERT(click_tool(UI_BTN_DRAW_LINE, UI_PANEL_RIGHT_TAB_CREATE));
    viewport_click(x, y, SDL_BUTTON_LEFT);
    SDL_Event escape = {.type = SDL_KEYDOWN};
    escape.key.keysym.sym = SDLK_ESCAPE;
    AppContext ctx = {0};
    Input_Handle(&ctx, &escape);
    TEST_ASSERT(!ctx.quit && state->editor.mode == TOOL_IDLE && state->editor.viewportTool == VIEWPORT_TOOL_SELECT);
    TEST_ASSERT(click_tool(UI_BTN_DRAW_LINE, UI_PANEL_RIGHT_TAB_CREATE));
    viewport_click(x, y, SDL_BUTTON_LEFT);
    UIPanel_SetActiveRightTab(UIPanel_Get(), UI_PANEL_RIGHT_TAB_MEASURE);
    TEST_ASSERT(state->editor.mode == TOOL_IDLE && state->editor.viewportTool == VIEWPORT_TOOL_SELECT);
    TEST_ASSERT(click_tool(UI_BTN_DRAW_LINE, UI_PANEL_RIGHT_TAB_CREATE));
    viewport_click(x, y, SDL_BUTTON_LEFT);
    TEST_ASSERT(click_tool(UI_BTN_CREATE_CATEGORY_PATHS, UI_PANEL_RIGHT_TAB_CREATE));
    TEST_ASSERT(state->editor.mode == TOOL_IDLE && state->editor.viewportTool == VIEWPORT_TOOL_SELECT);
    TEST_ASSERT(click_tool(UI_BTN_CREATE_CATEGORY_GEOMETRY, UI_PANEL_RIGHT_TAB_CREATE));
    char* canceled = Layout_SaveToString(&state->layout);
    TEST_ASSERT(!strcmp(before, canceled));
    Layout_FreeString(before);
    Layout_FreeString(canceled);
    TEST_ASSERT(click_tool(UI_BTN_DRAW_LINE, UI_PANEL_RIGHT_TAB_CREATE));
    viewport_click(x, y, SDL_BUTTON_LEFT);
    viewport_click(x + 160, y + 120, SDL_BUTTON_LEFT);
    TEST_ASSERT(state->layout.wallCount == 1 && state->layout.anchorCount == 2 &&
                Editor_UndoCount(&state->editor) == 1);
    TEST_ASSERT(click_tool(UI_BTN_STOP_DRAWING, UI_PANEL_RIGHT_TAB_CREATE));
    Editor_SelectAnchor(&state->editor, 0, false);
    TEST_ASSERT(click_tool(UI_BTN_SCENE_DELETE_SELECTED, UI_PANEL_RIGHT_TAB_VIEW));
    TEST_ASSERT(state->layout.anchors[0].isDeleted && state->layout.walls[0].isDeleted);
    TEST_ASSERT(Editor_UndoCount(&state->editor) == 2);
    TEST_ASSERT(InputEditorAction_Undo());
    TEST_ASSERT(!state->layout.anchors[0].isDeleted && !state->layout.walls[0].isDeleted);
    state->editor.selectedWallIndex = 0;
    TEST_ASSERT(click_tool(UI_BTN_SCENE_DELETE_SELECTED, UI_PANEL_RIGHT_TAB_VIEW));
    TEST_ASSERT(state->layout.walls[0].isDeleted && !state->layout.anchors[0].isDeleted);
    InputViewportNavigation_ResetGesture();
    ld_test_shutdown_runtime();
    return true;
}

static bool test_option_over_handles_at_physical_zoom_and_explicit_camera_tools(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Editor_ClearHistory(&state->editor);
    Global_SetWindowSize(1600, 1000);
    UIPanel_OnWindowResized(1600, 1000);
    CorePaneRect r;
    TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&state->paneHost, &r));
    const int x = (int)(r.x + r.width / 2), y = (int)(r.y + r.height / 2);
    state->grid.gridSize = .1f;
    state->grid.scale = 1800;
    state->freeViewCamera.enabled = false;
    state->editor.primitivePlacementPreview = PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM;
    UIPanel_SetActiveRightTab(UIPanel_Get(), UI_PANEL_RIGHT_TAB_MEASURE);
    UIPanel_Get()->measurement.picking = 1;
    char* before = Layout_SaveToString(&state->layout);
    float yaw = state->freeViewCamera.yawDeg;
    SDL_Keymod mods = SDL_GetModState();
    SDL_SetModState(KMOD_ALT);
    SDL_Event e = {.type = SDL_MOUSEBUTTONDOWN};
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x;
    e.button.y = y;
    Input_Handle(NULL, &e);
    TEST_ASSERT(state->freeViewCamera.enabled);
    e.type = SDL_MOUSEMOTION;
    e.motion.x = x + 36;
    e.motion.y = y + 14;
    e.motion.xrel = 36;
    e.motion.yrel = 14;
    e.motion.state = SDL_BUTTON_LMASK;
    Input_Handle(NULL, &e);
    TEST_ASSERT(!ld_test_nearly_equal(yaw, state->freeViewCamera.yawDeg) &&
                state->grid.scale == 1800 && state->layout.objectStore.count == 0 &&
                state->editor.primitivePlacementPreview == PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM);
    e.type = SDL_MOUSEBUTTONUP;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x + 36;
    e.button.y = y + 14;
    Input_Handle(NULL, &e);
    SDL_SetModState(KMOD_NONE);
    TEST_ASSERT(click_tool(UI_BTN_VIEW_ORBIT, UI_PANEL_RIGHT_TAB_VIEW));
    yaw = state->freeViewCamera.yawDeg;
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x;
    e.button.y = y;
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEMOTION;
    e.motion.state = SDL_BUTTON_LMASK;
    e.motion.x = (int)(r.x + r.width + 15); /* Captured gesture crosses pane boundary. */
    e.motion.y = y;
    e.motion.xrel = 12;
    e.motion.yrel = 0;
    Input_Handle(NULL, &e);
    TEST_ASSERT(!ld_test_nearly_equal(yaw, state->freeViewCamera.yawDeg));
    e.type = SDL_MOUSEBUTTONUP;
    e.button.button = SDL_BUTTON_LEFT;
    Input_Handle(NULL, &e);
    TEST_ASSERT(click_tool(UI_BTN_VIEW_PAN, UI_PANEL_RIGHT_TAB_VIEW));
    Vec3 target = state->freeViewCamera.target;
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x;
    e.button.y = y;
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEMOTION;
    e.motion.state = SDL_BUTTON_LMASK;
    e.motion.x = x + 18;
    e.motion.y = y - 12;
    e.motion.xrel = 18;
    e.motion.yrel = -12;
    Input_Handle(NULL, &e);
    TEST_ASSERT(!ld_test_vec3_nearly_equal(target, state->freeViewCamera.target) && state->grid.scale == 1800);
    e.type = SDL_MOUSEBUTTONUP;
    e.button.button = SDL_BUTTON_LEFT;
    Input_Handle(NULL, &e);
    char* after = Layout_SaveToString(&state->layout);
    TEST_ASSERT(!strcmp(before, after) && Editor_UndoCount(&state->editor) == 0);
    Layout_FreeString(before);
    Layout_FreeString(after);
    TEST_ASSERT(click_tool(UI_BTN_VIEW_SELECT, UI_PANEL_RIGHT_TAB_VIEW));
    yaw = state->freeViewCamera.yawDeg;
    SDL_SetModState(KMOD_ALT);
    e.type = SDL_MOUSEMOTION;
    e.motion.state = SDL_BUTTON_LMASK;
    Input_Handle(NULL, &e);
    TEST_ASSERT(state->freeViewCamera.yawDeg == yaw); /* No gesture start, no orbit. */
    SDL_SetModState(mods);
    InputViewportNavigation_ResetGesture();
    ld_test_shutdown_runtime();
    return true;
}

static bool test_option_drag_over_selected_point_does_not_edit_it(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Global_SetWindowSize(1600, 1000);
    UIPanel_OnWindowResized(1600, 1000);
    state->freeViewCamera.enabled = true;
    state->grid.gridSize = state->layout.gridSize;
    state->layout.scene3d.bounds.enabled = false;
    state->editor.sceneBoundsHandlesVisible = false;
    int index = Layout_AddAnchor3(&state->layout, (Vec3){0, 0, 0});
    state->layout.anchors[index].isPersistent = true;
    Editor_SelectAnchor(&state->editor, index, false);
    Editor_ClearHistory(&state->editor);
    TEST_ASSERT(LineDrawingViewportZoom_FitVisibleGeometry(state));
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
    Vec2 point = WorldToScreen(SpaceAdapter_ProjectToView((Vec3){0, 0, 0}, &view), &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)point.x, (int)point.y);
    TEST_ASSERT(hit.type == HITBOX_POINT || hit.type == HITBOX_GIZMO_AXIS);
    TEST_ASSERT(HitboxSystem_GetHitAtOfType((int)point.x + 30, (int)point.y,
                                         HITBOX_POINT).type == HITBOX_NONE);
    char* before = Layout_SaveToString(&state->layout);
    SDL_Keymod mods = SDL_GetModState();
    SDL_SetModState(KMOD_ALT);
    float yaw = state->freeViewCamera.yawDeg;
    SDL_Event e = {.type = SDL_MOUSEBUTTONDOWN};
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = (int)point.x;
    e.button.y = (int)point.y;
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEMOTION;
    e.motion.x = (int)point.x + 30;
    e.motion.y = (int)point.y + 10;
    e.motion.xrel = 30;
    e.motion.yrel = 10;
    e.motion.state = SDL_BUTTON_LMASK;
    Input_Handle(NULL, &e);
    TEST_ASSERT(state->freeViewCamera.yawDeg != yaw);
    TEST_ASSERT(state->editor.selectedAnchorIndex == index && !state->editor.isDraggingAnchor);
    e.type = SDL_MOUSEBUTTONUP;
    e.button.button = SDL_BUTTON_LEFT;
    Input_Handle(NULL, &e);
    char* after = Layout_SaveToString(&state->layout);
    TEST_ASSERT(!strcmp(before, after) && !Editor_UndoCount(&state->editor));
    Layout_FreeString(before);
    Layout_FreeString(after);
    SDL_SetModState(mods);
    InputViewportNavigation_ResetGesture();
    ld_test_shutdown_runtime();
    return true;
}

bool viewport_navigation_input_run_tests(void) {
    const TestCase cases[] = {
        { "OptionOverSelectedPointIsCameraOnly", test_option_drag_over_selected_point_does_not_edit_it },
        { "ExplicitLineCancelDeleteUndo", test_line_creation_is_explicit_cancelable_and_deletable },
        { "OptionAndVisibleCameraToolsAtPhysicalZoom", test_option_over_handles_at_physical_zoom_and_explicit_camera_tools },
        { "FitSceneButtonPhysicalZoomReadOnly",test_fit_scene_button_physical_zoom_read_only },
        { "AltLmbOrbitRequiresButtonAndPreservesTarget",
          test_alt_lmb_orbit_requires_button_and_preserves_target },
        { "MiddleDragPansFreeViewTargetAndReleaseStops",
          test_middle_drag_pans_free_view_target_and_release_stops }
    };
    return run_test_cases("ViewportNavigationInput",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
