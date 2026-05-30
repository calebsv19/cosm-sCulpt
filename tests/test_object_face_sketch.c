#include "test_layout_internal.h"

#include "Core/workspace/line_drawing_object_workspace_view.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"
#include "Editor/object_face_sketch_edit.h"
#include "Input/input_keyboard.h"
#include "Input/input_mouse.h"
#include "UI/input_ui_panel.h"

#include <string.h>

static bool seed_object_face_sketch_state(GlobalState** out_state) {
    GlobalState* state = NULL;
    Layout* layout = NULL;
    RectPrismPrimitiveCreateParams params;
    uint32_t object_id = 0u;
    bool adjusted = false;

    if (out_state) *out_state = NULL;
    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    layout = &state->layout;

    params = (RectPrismPrimitiveCreateParams){
        .width = 4.0f,
        .height = 6.0f,
        .depth = 8.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 0.0f, 0.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(LineDrawingObjectWorkspaceView_FocusFace(state,
                                                        object_id,
                                                        OBJECT3D_FACE_RECT_PRISM_POS_N));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_SELECT;
    state->editor.selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_BODY;
    Global_FlagHitboxesDirty();

    if (out_state) *out_state = state;
    return true;
}

static void shutdown_object_face_sketch_state(void) {
    ld_test_shutdown_runtime();
}

static void sketch_screen_point(const GlobalState* state, float u, float v, int* out_x, int* out_y) {
    SpaceViewContext view_ctx = {0};
    const Vec3 axis_u = Vec3_Normalize(state->editor.objectFaceSketchFrame.axisU);
    const Vec3 axis_v = Vec3_Normalize(state->editor.objectFaceSketchFrame.axisV);
    const Vec3 world = Vec3_Add(state->editor.objectFaceSketchFrame.origin,
                                Vec3_Add(Vec3_Scale(axis_u, u), Vec3_Scale(axis_v, v)));
    Vec2 screen = {0};
    view_ctx = SpaceAdapter_BuildViewContext((GlobalState*)state);
    screen = WorldToScreen(SpaceAdapter_ProjectToView(world, &view_ctx), &state->grid);
    if (out_x) *out_x = (int)lroundf(screen.x);
    if (out_y) *out_y = (int)lroundf(screen.y);
}

static const UIButton* find_button_by_id(const UIPanelState* ui, int button_id) {
    if (!ui) return NULL;
    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == button_id) return &ui->buttons[i];
    }
    return NULL;
}

static bool test_object_face_sketch_hitboxes_prioritize_committed_sketch(void) {
    GlobalState* state = NULL;
    Hitbox hit = {0};
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    Global_RebuildHitboxesIfDirty();

    sketch_screen_point(state, 0.0f, 0.0f, &x, &y);
    hit = HitboxSystem_GetHitAt(x, y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT_FACE_SKETCH_BODY);
    TEST_ASSERT(hit.subIndex == OBJECT_FACE_SKETCH_HANDLE_BODY);

    sketch_screen_point(state, 1.0f, 1.0f, &x, &y);
    hit = HitboxSystem_GetHitAt(x, y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT_FACE_SKETCH_HANDLE);
    TEST_ASSERT(hit.subIndex == OBJECT_FACE_SKETCH_HANDLE_CORNER_POS_U_POS_V);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_hitboxes_survive_face_select_for_reselect(void) {
    GlobalState* state = NULL;
    Hitbox hit = {0};
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    Editor_ObjectFaceSketchDeselect(&state->editor);
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_FACE_SELECT;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    sketch_screen_point(state, 0.0f, 0.0f, &x, &y);
    hit = HitboxSystem_GetHitAt(x, y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT_FACE_SKETCH_BODY);
    TEST_ASSERT(hit.subIndex == OBJECT_FACE_SKETCH_HANDLE_BODY);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_draw_starts_from_face_focused_camera(void) {
    GlobalState* state = NULL;
    Layout* layout = NULL;
    RectPrismPrimitiveCreateParams params;
    uint32_t object_id = 0u;
    bool adjusted = false;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    layout = &state->layout;

    params = (RectPrismPrimitiveCreateParams){
        .width = 4.0f,
        .height = 6.0f,
        .depth = 8.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 0.0f, 0.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(LineDrawingObjectWorkspaceView_FocusFace(state,
                                                        object_id,
                                                        OBJECT3D_FACE_RECT_PRISM_POS_N));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));

    sketch_screen_point(state, 0.0f, 0.0f, &x0, &y0);
    sketch_screen_point(state, 0.75f, 0.5f, &x1, &y1);
    TEST_ASSERT(Editor_ObjectFaceSketchHandleLeftMouseDown(state, x0, y0));
    TEST_ASSERT(state->editor.objectFaceSketchDragging);

    Editor_ObjectFaceSketchHandleMouseMotion(state, x1, y1);
    Editor_ObjectFaceSketchHandleLeftMouseUp(state, x1, y1);
    TEST_ASSERT(state->editor.objectFaceSketchHasRectangle);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_body_drag_moves_committed_rectangle(void) {
    GlobalState* state = NULL;
    Vec2 min_uv = {0};
    Vec2 max_uv = {0};
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    sketch_screen_point(state, 0.0f, 0.0f, &x0, &y0);
    sketch_screen_point(state, 0.5f, -0.25f, &x1, &y1);

    TEST_ASSERT(Editor_ObjectFaceSketchBeginEditDrag(state,
                                                     OBJECT_FACE_SKETCH_HANDLE_BODY,
                                                     x0,
                                                     y0));
    Editor_ObjectFaceSketchEndEditDrag(state, x1, y1);
    Editor_ObjectFaceSketchGetRectangleUV(&state->editor, &min_uv, &max_uv);
    TEST_ASSERT(ld_test_nearly_equal(min_uv.x, -0.5f));
    TEST_ASSERT(ld_test_nearly_equal(min_uv.y, -1.25f));
    TEST_ASSERT(ld_test_nearly_equal(max_uv.x, 1.5f));
    TEST_ASSERT(ld_test_nearly_equal(max_uv.y, 0.75f));

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_select_helpers_drive_operation_gating(void) {
    GlobalState* state = NULL;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));
    TEST_ASSERT(Editor_ObjectFaceExtrudeArm(state, OBJECT_FACE_EXTRUDE_MODE_ADD));

    Editor_ObjectFaceExtrudeClear(&state->editor);
    Editor_ObjectFaceSketchDeselect(&state->editor);
    TEST_ASSERT(!Editor_ObjectFaceSketchIsSelected(&state->editor));
    TEST_ASSERT(!Editor_ObjectFaceExtrudeArm(state, OBJECT_FACE_EXTRUDE_MODE_ADD));

    TEST_ASSERT(Editor_ObjectFaceSketchSelect(&state->editor, OBJECT_FACE_SKETCH_HANDLE_BODY));
    TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));
    TEST_ASSERT(Editor_ObjectFaceExtrudeArm(state, OBJECT_FACE_EXTRUDE_MODE_ADD));

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_authoring_labels_follow_mode_transitions(void) {
    GlobalState* state = NULL;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    TEST_ASSERT(seed_object_face_sketch_state(&state));

    TEST_ASSERT(strcmp(Editor_ObjectAuthoringStageLabel(&state->editor), "Sketch Select") == 0);
    TEST_ASSERT(strstr(Editor_ObjectAuthoringPromptLabel(&state->editor), "Selected sketch is live") != NULL);

    Editor_ObjectFaceSketchDeselect(&state->editor);
    TEST_ASSERT(strcmp(Editor_ObjectAuthoringStageLabel(&state->editor), "Face Select") == 0);
    TEST_ASSERT(strstr(Editor_ObjectAuthoringPromptLabel(&state->editor), "Use Face / Rect / Select") != NULL);

    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    TEST_ASSERT(strcmp(Editor_ObjectAuthoringStageLabel(&state->editor), "Sketch Draw Armed") == 0);
    TEST_ASSERT(strstr(Editor_ObjectAuthoringPromptLabel(&state->editor), "Click-drag in the viewport") != NULL);

    sketch_screen_point(state, -0.25f, -0.25f, &x0, &y0);
    sketch_screen_point(state, 0.25f, 0.25f, &x1, &y1);
    TEST_ASSERT(Editor_ObjectFaceSketchHandleLeftMouseDown(state, x0, y0));
    TEST_ASSERT(strcmp(Editor_ObjectAuthoringStageLabel(&state->editor), "Sketch Draw Active") == 0);
    TEST_ASSERT(strstr(Editor_ObjectAuthoringPromptLabel(&state->editor), "Release to commit") != NULL);
    Editor_ObjectFaceSketchHandleLeftMouseUp(state, x1, y1);

    TEST_ASSERT(strcmp(Editor_ObjectAuthoringStageLabel(&state->editor), "Sketch Select") == 0);
    TEST_ASSERT(strstr(Editor_ObjectAuthoringPromptLabel(&state->editor), "Selected sketch is live") != NULL);

    TEST_ASSERT(Editor_ObjectFaceExtrudeArm(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(strcmp(Editor_ObjectAuthoringStageLabel(&state->editor), "Extrude Preview") == 0);
    TEST_ASSERT(strstr(Editor_ObjectAuthoringPromptLabel(&state->editor), "press the same operation again to commit") != NULL);

    TEST_ASSERT(Editor_ObjectFaceExtrudeHandleLeftMouseDown(state, x0, y0));
    TEST_ASSERT(strcmp(Editor_ObjectAuthoringStageLabel(&state->editor), "Extrude Depth Drag") == 0);
    TEST_ASSERT(strstr(Editor_ObjectAuthoringPromptLabel(&state->editor), "Release in the viewport") != NULL);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_focus_face_routes_shape_tab_visible(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    RectPrismPrimitiveCreateParams params;
    uint32_t object_id = 0u;
    bool adjusted = false;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);

    params = (RectPrismPrimitiveCreateParams){
        .width = 4.0f,
        .height = 6.0f,
        .depth = 8.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 0.0f, 0.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &params, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));

    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_OBJECT);
    TEST_ASSERT(LineDrawingObjectWorkspaceView_FocusFace(state,
                                                        object_id,
                                                        OBJECT3D_FACE_RECT_PRISM_POS_N));
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_click_routes_shape_tab_visible(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Event click = {0};
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_OBJECT);

    sketch_screen_point(state, 0.0f, 0.0f, &x, &y);
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = x;
    click.button.y = y;
    Input_MouseHandle(NULL, &click);

    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_panel_buttons_keep_shape_tab_and_mode_routing(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* rect_button = NULL;
    const UIButton* face_button = NULL;
    const UIButton* select_button = NULL;
    const UIButton* clear_button = NULL;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_CREATE);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    rect_button = find_button_by_id(ui, UI_BTN_CREATE_PLANE);
    face_button = find_button_by_id(ui, UI_BTN_OBJECT_FACE_SELECT);
    select_button = find_button_by_id(ui, UI_BTN_OBJECT_SKETCH_SELECT);
    clear_button = find_button_by_id(ui, UI_BTN_OBJECT_SKETCH_CLEAR);
    TEST_ASSERT(rect_button != NULL);
    TEST_ASSERT(face_button != NULL);
    TEST_ASSERT(select_button != NULL);
    TEST_ASSERT(clear_button != NULL);

    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    ui->objectActiveRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    TEST_ASSERT(UIPanel_HandleClick(rect_button->bounds.x + rect_button->bounds.w / 2,
                                    rect_button->bounds.y + rect_button->bounds.h / 2));
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(ui->objectActiveRightTab == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_DRAW);
    TEST_ASSERT(state->editor.objectFaceSketchToolArmed);

    shutdown_object_face_sketch_state();

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_CREATE);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    face_button = find_button_by_id(ui, UI_BTN_OBJECT_FACE_SELECT);
    select_button = find_button_by_id(ui, UI_BTN_OBJECT_SKETCH_SELECT);
    clear_button = find_button_by_id(ui, UI_BTN_OBJECT_SKETCH_CLEAR);
    TEST_ASSERT(face_button != NULL);
    TEST_ASSERT(select_button != NULL);
    TEST_ASSERT(clear_button != NULL);
    Editor_ObjectFaceSketchDeselect(&state->editor);
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_FACE_SELECT;
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    ui->objectActiveRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    TEST_ASSERT(UIPanel_HandleClick(select_button->bounds.x + select_button->bounds.w / 2,
                                    select_button->bounds.y + select_button->bounds.h / 2));
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(ui->objectActiveRightTab == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT);
    TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));

    TEST_ASSERT(UIPanel_HandleClick(face_button->bounds.x + face_button->bounds.w / 2,
                                    face_button->bounds.y + face_button->bounds.h / 2));
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(ui->objectActiveRightTab == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_FACE_SELECT);
    TEST_ASSERT(!Editor_ObjectFaceSketchIsSelected(&state->editor));

    shutdown_object_face_sketch_state();

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_CREATE);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    clear_button = find_button_by_id(ui, UI_BTN_OBJECT_SKETCH_CLEAR);
    TEST_ASSERT(clear_button != NULL);
    TEST_ASSERT(UIPanel_HandleClick(clear_button->bounds.x + clear_button->bounds.w / 2,
                                    clear_button->bounds.y + clear_button->bounds.h / 2));
    TEST_ASSERT(!state->editor.objectFaceSketchHasRectangle);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_FACE_SELECT);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_panel_extrude_button_requires_selected_sketch(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* extrude_button = NULL;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    extrude_button = find_button_by_id(ui, UI_BTN_EXTRUDE_ADD);
    TEST_ASSERT(extrude_button != NULL);

    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    ui->objectActiveRightTab = UI_PANEL_RIGHT_TAB_OBJECT;
    TEST_ASSERT(UIPanel_HandleClick(extrude_button->bounds.x + extrude_button->bounds.w / 2,
                                    extrude_button->bounds.y + extrude_button->bounds.h / 2));
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(ui->objectActiveRightTab == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_OPERATION_PREVIEW);
    Editor_ObjectFaceExtrudeClear(&state->editor);
    Editor_ObjectFaceSketchDeselect(&state->editor);
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_FACE_SELECT;
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    ui->objectActiveRightTab = UI_PANEL_RIGHT_TAB_OBJECT;

    TEST_ASSERT(UIPanel_HandleClick(extrude_button->bounds.x + extrude_button->bounds.w / 2,
                                    extrude_button->bounds.y + extrude_button->bounds.h / 2));
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_FACE_SELECT);
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);
    TEST_ASSERT(ui->objectActiveRightTab == UI_PANEL_RIGHT_TAB_OBJECT);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_keyboard_plus_minus_arm_operations(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    AppContext ctx = {0};
    SDL_Event event = {0};

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);

    event.type = SDL_KEYDOWN;
    event.key.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_KP_PLUS;
    Input_KeyboardHandle(&ctx, &event);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_OPERATION_PREVIEW);
    TEST_ASSERT(state->editor.objectFaceExtrudeMode == OBJECT_FACE_EXTRUDE_MODE_ADD);
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);

    Editor_ObjectFaceExtrudeClear(&state->editor);
    TEST_ASSERT(Editor_ObjectFaceSketchSelect(&state->editor, OBJECT_FACE_SKETCH_HANDLE_BODY));
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    event.key.keysym.sym = SDLK_MINUS;
    Input_KeyboardHandle(&ctx, &event);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_OPERATION_PREVIEW);
    TEST_ASSERT(state->editor.objectFaceExtrudeMode == OBJECT_FACE_EXTRUDE_MODE_CUT);
    TEST_ASSERT(UIPanel_GetActiveRightTab(ui) == UI_PANEL_RIGHT_TAB_CREATE);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_extrude_trigger_arms_default_preview(void) {
    GlobalState* state = NULL;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_OPERATION_PREVIEW);
    TEST_ASSERT(state->editor.objectFaceExtrudeToolArmed);
    TEST_ASSERT(state->editor.objectFaceExtrudeHasPreview);
    TEST_ASSERT(state->editor.objectFaceExtrudeDepth > 0.0f);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == state->editor.objectFaceSketchBodyId);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == state->editor.objectFaceSketchFace);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_extrude_trigger_second_press_commits_add_and_preserves_source_sketch(void) {
    GlobalState* state = NULL;
    size_t object_count_before = 0u;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    object_count_before = Layout_ObjectStore_LiveCount(&state->layout.objectStore);
    {
        uint32_t body_id = state->editor.objectFaceSketchBodyId;
        Object3DFaceKind face = state->editor.objectFaceSketchFace;
        TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
        TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
        TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == object_count_before + 1u);
        TEST_ASSERT(state->editor.objectFaceSketchHasRectangle);
        TEST_ASSERT(state->editor.selectedObjectAssetBodyId == body_id);
        TEST_ASSERT(state->editor.selectedObjectAssetFace == face);
        TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT);
        TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));
    }

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_refocus_same_face_preserves_committed_rectangle(void) {
    GlobalState* state = NULL;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    TEST_ASSERT(LineDrawingObjectWorkspaceView_EnterFreeView(state, state->editor.selectedObject3DId));
    TEST_ASSERT(state->editor.objectFaceSketchHasRectangle);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);

    TEST_ASSERT(LineDrawingObjectWorkspaceView_FocusFace(state,
                                                        state->editor.selectedObject3DId,
                                                        OBJECT3D_FACE_RECT_PRISM_POS_N));
    TEST_ASSERT(state->editor.objectFaceSketchHasRectangle);
    TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_keyboard_reselect_shortcuts_work(void) {
    GlobalState* state = NULL;
    AppContext ctx = {0};
    SDL_Event event = {0};

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    Editor_ObjectFaceSketchDeselect(&state->editor);
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_FACE_SELECT;

    event.type = SDL_KEYDOWN;
    event.key.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_s;
    Input_KeyboardHandle(&ctx, &event);
    TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));

    event.key.keysym.sym = SDLK_f;
    Input_KeyboardHandle(&ctx, &event);
    TEST_ASSERT(!Editor_ObjectFaceSketchIsSelected(&state->editor));
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_FACE_SELECT);

    event.key.keysym.sym = SDLK_r;
    Input_KeyboardHandle(&ctx, &event);
    TEST_ASSERT(state->editor.objectFaceSketchToolArmed);
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_DRAW);

    shutdown_object_face_sketch_state();
    return true;
}

static bool test_object_face_sketch_empty_click_preserves_selected_sketch_lane(void) {
    GlobalState* state = NULL;
    Hitbox hit = {0};
    SDL_Event click = {0};
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_face_sketch_state(&state));
    sketch_screen_point(state, 3.0f, 3.0f, &x, &y);
    Global_RebuildHitboxesIfDirty();
    hit = HitboxSystem_GetHitAt(x, y);
    TEST_ASSERT(hit.type == HITBOX_NONE);

    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = x;
    click.button.y = y;
    Input_MouseHandle(NULL, &click);

    TEST_ASSERT(Editor_ObjectFaceSketchIsSelected(&state->editor));
    TEST_ASSERT(state->editor.objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT);

    shutdown_object_face_sketch_state();
    return true;
}

bool object_face_sketch_run_tests(void) {
    const TestCase cases[] = {
        { "object_face_sketch_hitboxes_prioritize_committed_sketch",
          test_object_face_sketch_hitboxes_prioritize_committed_sketch },
        { "object_face_sketch_hitboxes_survive_face_select_for_reselect",
          test_object_face_sketch_hitboxes_survive_face_select_for_reselect },
        { "object_face_sketch_draw_starts_from_face_focused_camera",
          test_object_face_sketch_draw_starts_from_face_focused_camera },
        { "object_face_sketch_body_drag_moves_committed_rectangle",
          test_object_face_sketch_body_drag_moves_committed_rectangle },
        { "object_face_sketch_select_helpers_drive_operation_gating",
          test_object_face_sketch_select_helpers_drive_operation_gating },
        { "object_face_sketch_authoring_labels_follow_mode_transitions",
          test_object_face_sketch_authoring_labels_follow_mode_transitions },
        { "object_face_sketch_focus_face_routes_shape_tab_visible",
          test_object_face_sketch_focus_face_routes_shape_tab_visible },
        { "object_face_sketch_click_routes_shape_tab_visible",
          test_object_face_sketch_click_routes_shape_tab_visible },
        { "object_face_sketch_panel_buttons_keep_shape_tab_and_mode_routing",
          test_object_face_sketch_panel_buttons_keep_shape_tab_and_mode_routing },
        { "object_face_sketch_panel_extrude_button_requires_selected_sketch",
          test_object_face_sketch_panel_extrude_button_requires_selected_sketch },
        { "object_face_sketch_keyboard_plus_minus_arm_operations",
          test_object_face_sketch_keyboard_plus_minus_arm_operations },
        { "object_face_sketch_extrude_trigger_arms_default_preview",
          test_object_face_sketch_extrude_trigger_arms_default_preview },
        { "object_face_sketch_extrude_trigger_second_press_commits_add_and_preserves_source_sketch",
          test_object_face_sketch_extrude_trigger_second_press_commits_add_and_preserves_source_sketch },
        { "object_face_sketch_refocus_same_face_preserves_committed_rectangle",
          test_object_face_sketch_refocus_same_face_preserves_committed_rectangle },
        { "object_face_sketch_keyboard_reselect_shortcuts_work",
          test_object_face_sketch_keyboard_reselect_shortcuts_work },
        { "object_face_sketch_empty_click_preserves_selected_sketch_lane",
          test_object_face_sketch_empty_click_preserves_selected_sketch_lane },
    };
    return run_test_cases("ObjectFaceSketch", cases, sizeof(cases) / sizeof(cases[0]));
}
