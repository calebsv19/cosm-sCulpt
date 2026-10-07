#include "test_layout_internal.h"
#include "Core/camera_view.h"
#include "Core/space_mode_adapter.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "Layout/layout_engineering.h"
#include "UI/ui_panel_camera.h"
#include "UI/ui_panel_shell.h"
#include "Input/input_handler.h"
#include "Tools/canonical_scene_export.h"
#include <SDL2/SDL.h>

static bool camera_projection_roundtrip_and_clipping(void) {
    ld_test_init_runtime();
    GlobalState *s = Global_Get();
    Editor_ClearHistory(&s->editor);
    TEST_ASSERT(CameraView_Enter(s, false));
    s->cameraView.eye = (Vec3){0};
    s->cameraView.yaw = 90;
    s->cameraView.pitch = 0;
    SpaceViewContext v = SpaceAdapter_BuildViewContext(s);
    TEST_ASSERT(v.perspective.enabled);
    Vec3 p = {.3f, 2, .4f};
    Vec2 projected = SpaceAdapter_ProjectToView(p, &v);
    Ray3 ray = PerspectiveView_Ray(&v.perspective, projected);
    TEST_ASSERT(Vec3_Dot(ray.direction, Vec3_Normalize(p)) > .99999f);
    Vec2 p1 = SpaceAdapter_ProjectToView((Vec3){1, 2, 0}, &v);
    Vec2 p2 = SpaceAdapter_ProjectToView((Vec3){1, 4, 0}, &v);
    TEST_ASSERT(fabsf((p1.x - v.perspective.center.x) * .5f - (p2.x - v.perspective.center.x)) <
                .001f);
    Vec3 clipped[12];
    size_t n = PerspectiveView_ClipTriangle(&v.perspective, (Vec3){-.1f, -1, 0}, (Vec3){.1f, 1, 0},
                                            (Vec3){0, 1, .1f}, clipped);
    TEST_ASSERT(n >= 3);
    for (size_t i = 0; i < n; ++i) {
        float d = PerspectiveView_Depth(&v.perspective, clipped[i]);
        TEST_ASSERT(d >= v.perspective.near_clip - 1e-6f);
        TEST_ASSERT(isfinite(PerspectiveView_Project(&v.perspective, clipped[i]).x));
    }
    TEST_ASSERT(!PerspectiveView_ClipTriangle(&v.perspective, (Vec3){0, -2, 0}, (Vec3){1, -2, 0},
                                              (Vec3){0, -2, 1}, clipped));
    ld_test_shutdown_runtime();
    return true;
}
static bool camera_navigation_focus_alt_roll_and_idle(void) {
    ld_test_init_runtime();
    GlobalState *s = Global_Get();
    Editor_ClearHistory(&s->editor);
    char *before = Layout_SaveToString(&s->layout);
    TEST_ASSERT(CameraView_Enter(s, false));
    TEST_ASSERT(CameraView_NextUpdateDelayMs(s) == -1);
    CorePaneRect r;
    TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&s->paneHost, &r));
    SDL_Event e = {0};
    SDL_Keymod previous = SDL_GetModState();
    e.type = SDL_MOUSEMOTION;
    e.motion.x = (int)(r.x + r.width * .5f);
    e.motion.y = (int)(r.y + r.height * .5f);
    e.motion.xrel = 20;
    e.motion.yrel = 10;
    float yaw = s->cameraView.yaw, pitch = s->cameraView.pitch;
    SDL_SetModState(KMOD_ALT);
    Input_Handle(NULL, &e);
    TEST_ASSERT(fabsf(s->cameraView.yaw - yaw) > 1 && fabsf(s->cameraView.pitch - pitch) > 1);
    TEST_ASSERT(s->cameraView.roll == 0);
    SDL_SetModState(KMOD_ALT | KMOD_SHIFT);
    Input_Handle(NULL, &e);
    TEST_ASSERT(fabsf(s->cameraView.roll) > 1);
    SDL_SetModState(KMOD_NONE);
    e = (SDL_Event){0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_w;
    Input_Handle(NULL, &e);
    TEST_ASSERT(CameraView_NextUpdateDelayMs(s) == 16);
    Vec3 eye = s->cameraView.eye;
    TEST_ASSERT(CameraView_Step(s, 10));
    TEST_ASSERT(fabsf(Vec3_Distance(eye, s->cameraView.eye) - .025f) < 1e-6f);
    e.type = SDL_KEYUP;
    Input_Handle(NULL, &e);
    TEST_ASSERT(CameraView_NextUpdateDelayMs(s) == -1);
    e.type = SDL_KEYDOWN;
    Input_Handle(NULL, &e);
    e.type = SDL_WINDOWEVENT;
    e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    Input_Handle(NULL, &e);
    TEST_ASSERT(CameraView_NextUpdateDelayMs(s) == -1);
    char *after = Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 0);
    free(before);
    free(after);
    SDL_SetModState(previous);
    CameraView_Exit(s);
    ld_test_shutdown_runtime();
    return true;
}
static bool camera_saved_pose_undo_persistence_and_path_guard(void) {
    ld_test_init_runtime();
    GlobalState *s = Global_Get();
    Editor_ClearHistory(&s->editor);
    size_t count = s->layout.sceneAuthoring.camera_count;
    TEST_ASSERT(CameraView_Enter(s, false));
    s->cameraView.eye = (Vec3){.4f, -.7f, 1.5f};
    s->cameraView.yaw = 25;
    s->cameraView.pitch = 83;
    s->cameraView.roll = 23;
    Vec3 forward, up;
    CameraView_Basis(&s->cameraView, &forward, NULL, &up);
    TEST_ASSERT(CameraView_Save(s, false));
    TEST_ASSERT(s->layout.sceneAuthoring.camera_count == count + 1);
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 1);
    LineDrawingSceneCameraPose pose;
    const LineDrawingSceneCamera *c = &s->layout.sceneAuthoring.cameras[count];
    TEST_ASSERT(Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(c, NULL, 0, &pose));
    TEST_ASSERT(Vec3_Dot(pose.up, up) > .99999 && Vec3_Dot(pose.forward, forward) > .99999);
    TEST_ASSERT(CameraView_Enter(s, true));
    TEST_ASSERT(fabs(s->cameraView.eye.z - 1.5) < 1e-6 && fabs(s->cameraView.roll - 23) < .001);
    char *json = Layout_SaveToString(&s->layout);
    Layout loaded;
    Layout_Init(&loaded, 1);
    TEST_ASSERT(json && Layout_LoadFromString(&loaded, json));
    TEST_ASSERT(loaded.sceneAuthoring.camera_count == count + 1);
    TEST_ASSERT(fabs(loaded.sceneAuthoring.cameras[count].position.x - .4) < 1e-6);
    Layout_Free(&loaded);
    free(json);
    char *exported = LineDrawingCanonicalScene_ExportLayoutToString(&s->layout, "camera_proof");
    TEST_ASSERT(exported != NULL);
    cJSON *doc = cJSON_Parse(exported);
    cJSON *cameras = cJSON_GetObjectItem(doc, "cameras");
    bool found = false;
    for (int i = 0; i < cJSON_GetArraySize(cameras); ++i) {
        cJSON *item = cJSON_GetArrayItem(cameras, i);
        cJSON *id = cJSON_GetObjectItem(item, "camera_id");
        if (cJSON_IsString(id) && !strcmp(id->valuestring, c->camera_id)) {
            TEST_ASSERT(fabs(cJSON_GetObjectItem(item, "vertical_fov_degrees")->valuedouble - 65) <
                        .001);
            found = true;
        }
    }
    TEST_ASSERT(found);
    cJSON_Delete(doc);
    free(exported);
    TEST_ASSERT(Editor_Undo(&s->editor, &s->layout));
    TEST_ASSERT(s->layout.sceneAuthoring.camera_count == count);
    TEST_ASSERT(s->cameraView.active && !s->cameraView.source_id[0] && !s->cameraView.message[0]);
    TEST_ASSERT(s->cameraView.selected < count || count == 0);
    TEST_ASSERT(fabs(s->cameraView.eye.z - 1.5) < 1e-6);
    TEST_ASSERT(Editor_Redo(&s->editor, &s->layout));
    TEST_ASSERT(s->layout.sceneAuthoring.camera_count == count + 1);
    s->cameraView.selected = count;
    TEST_ASSERT(CameraView_Enter(s, true));
    snprintf(s->layout.sceneAuthoring.cameras[count].path_id, 64, "protected_path");
    size_t history = Editor_UndoCount(&s->editor);
    TEST_ASSERT(!CameraView_Save(s, true));
    TEST_ASSERT(Editor_UndoCount(&s->editor) == history);
    ld_test_shutdown_runtime();
    return true;
}
static bool camera_form_responsive_and_text_capture(void) {
    ld_test_init_runtime();
    GlobalState *s = Global_Get();
    Editor_ClearHistory(&s->editor);
    UIPanelState *ui = UIPanel_Get();
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_VIEW);
    TEST_ASSERT(CameraView_Enter(s, false));
    UIPanel_OnWindowResized(s->screenWidth, s->screenHeight);
    SDL_Rect r;
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_DETAILS, &r));
    TEST_ASSERT(UIPanel_CameraClick(r.x + 2, r.y + 2));
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_X, &r));
    int old = r.w;
    ui->viewPane.summaryRect.w += 180;
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_X, &r));
    TEST_ASSERT(r.w == old + 60);
    TEST_ASSERT(UIPanel_CameraClick(r.x + 2, r.y + 2));
    TEST_ASSERT(UIPanel_IsCapturingKeyboard());
    SDL_Event e = {0};
    e.type = SDL_TEXTINPUT;
    snprintf(e.text.text, sizeof(e.text.text), "25 cm");
    TEST_ASSERT(UIPanel_CameraEvent(&e));
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_RETURN;
    TEST_ASSERT(UIPanel_CameraEvent(&e));
    TEST_ASSERT(!UIPanel_IsCapturingKeyboard());
    TEST_ASSERT(fabs(s->cameraView.eye.x - .25) < 1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 0);
    CameraView_Exit(s);
    ld_test_shutdown_runtime();
    return true;
}
static bool camera_native_depth_order_and_near_clip(void) {
    ld_test_init_runtime();
    GlobalState *state = Global_Get();
    Layout_Free(&state->layout);
    Layout_Init(&state->layout, 1);
    uint32_t ids[2];
    for (int i = 0; i < 2; ++i) {
        RectPrismPrimitiveCreateParams p = {.width = 1,
                                            .height = 1,
                                            .depth = .2f,
                                            .useExplicitFrame = true,
                                            .explicitFrame = {.origin = {0, i ? 2 : 4, 0},
                                                              .axisU = {1, 0, 0},
                                                              .axisV = {0, 0, 1},
                                                              .normal = {0, 1, 0}}};
        TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &p, &ids[i], NULL));
    }
    SpaceViewContext v = {0};
    v.perspective = (PerspectiveView){.enabled = true,
                                      .eye = {0},
                                      .forward = {0, 1, 0},
                                      .right = {1, 0, 0},
                                      .up = {0, 0, 1},
                                      .center = {0},
                                      .focal = 1,
                                      .tan_half_fov = 1,
                                      .aspect = 1,
                                      .near_clip = .01f,
                                      .far_clip = 10};
    Grid grid = {.gridSize = 1, .scale = 32, .offsetX = -1, .offsetY = -1};
    uint8_t rgba[64 * 64 * 4] = {0};
    float depth[64 * 64];
    int32_t owner[64 * 64];
    size_t pixel = 32 * 64 + 32;
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 64 * 64; ++i) {
            depth[i] = INFINITY;
            owner[i] = -1;
        }
        LayoutMeshSolidPreviewFrameStats stats = {0};
        TEST_ASSERT(Layout_RasterNativeSurfaces(&state->layout, NULL, &v, &grid,
                                                (SDL_Rect){0, 0, 64, 64}, 1, 64, 64, true, rgba,
                                                depth, owner, &stats));
        TEST_ASSERT(owner[pixel] >= 0);
        TEST_ASSERT(state->layout.objectStore.items[owner[pixel]].objectId == ids[1]);
        TEST_ASSERT(fabsf(depth[pixel] - 1.9f) < 1e-5f);
        Object3D temp = state->layout.objectStore.items[0];
        state->layout.objectStore.items[0] = state->layout.objectStore.items[1];
        state->layout.objectStore.items[1] = temp;
    }
    /* Cutting through the front box must never rasterize behind-eye surfaces. */
    v.perspective.eye.y = 2;
    for (int i = 0; i < 64 * 64; ++i) {
        depth[i] = INFINITY;
        owner[i] = -1;
    }
    LayoutMeshSolidPreviewFrameStats stats = {0};
    TEST_ASSERT(Layout_RasterNativeSurfaces(&state->layout, NULL, &v, &grid,
                                            (SDL_Rect){0, 0, 64, 64}, 1, 64, 64, true, rgba, depth,
                                            owner, &stats));
    TEST_ASSERT(fabsf(depth[pixel] - .1f) < 1e-5f);
    ld_test_shutdown_runtime();
    return true;
}
static bool camera_guide_volumes_do_not_occlude_physical_panels(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout_Free(&state->layout);
    Layout_Init(&state->layout, 1);
    uint32_t ids[2];
    for (int i = 0; i < 2; ++i) {
        RectPrismPrimitiveCreateParams p = {
            .width = 1, .height = 1, .depth = .2f, .useExplicitFrame = true,
            .explicitFrame = {.origin = {0, i ? 4 : 2, 0}, .axisU = {1, 0, 0},
                              .axisV = {0, 0, 1}, .normal = {0, 1, 0}}};
        TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &p, &ids[i], NULL));
    }
    Object3D* guide = Layout_ObjectStore_Find(&state->layout.objectStore, ids[0]);
    Object3D* physical = Layout_ObjectStore_Find(&state->layout.objectStore, ids[1]);
    snprintf(physical->info.entity_type, sizeof(physical->info.entity_type), "Panel");
    /* Reference physical walls still occlude; reference is not a guide flag. */
    physical->info.reference = true;
    SpaceViewContext view = {0};
    view.perspective = (PerspectiveView){
        .enabled = true, .eye = {0}, .forward = {0, 1, 0}, .right = {1, 0, 0},
        .up = {0, 0, 1}, .center = {0}, .focal = 1, .tan_half_fov = 1,
        .aspect = 1, .near_clip = .01f, .far_clip = 10};
    Grid grid = {.gridSize = 1, .scale = 32, .offsetX = -1, .offsetY = -1};
    uint8_t rgba[64 * 64 * 4];
    float depth[64 * 64];
    int32_t owner[64 * 64];
    const char* types[] = {"RoutingCorridor", "ServiceVolume", "KeepoutVolume", "MotionEnvelope",
                           "Panel", "Panel"};
    for (size_t type = 0; type < sizeof(types) / sizeof(types[0]); ++type) {
        snprintf(guide->info.entity_type, sizeof(guide->info.entity_type), "%s", types[type]);
        guide->info.volume_role = type == 4 ? LAYOUT_VOLUME_KEEPOUT
                                 : type == 5 ? LAYOUT_VOLUME_SERVICE : LAYOUT_VOLUME_NONE;
        double lo, hi;
        TEST_ASSERT(Layout_SectionRange(&state->layout, 1, &lo, &hi));
        TEST_ASSERT(fabs(lo - 3.9) < 1e-6 && fabs(hi - 4.1) < 1e-6);
        LayoutEntityQuery query = {0};
        snprintf(query.entity_type, sizeof(query.entity_type), "%s", types[type]);
        char found[4][64];
        TEST_ASSERT(Layout_QueryEntities(&state->layout.objectStore, &query, found, 4) > 0);
        char* before = Layout_SaveToString(&state->layout);
        for (int mode = LAYOUT_SECTION_OFF; mode <= LAYOUT_SECTION_CUTAWAY; ++mode) {
            memset(rgba, 0, sizeof(rgba));
            for (size_t i = 0; i < 64 * 64; ++i) { depth[i] = INFINITY; owner[i] = -1; }
            LayoutSectionView section = {.mode = mode, .axis = 1, .position_meters = 4};
            LayoutMeshSolidPreviewFrameStats stats = {0};
            TEST_ASSERT(Layout_RasterNativeSurfaces(&state->layout, &section, &view, &grid,
                (SDL_Rect){0, 0, 64, 64}, 1, 64, 64, true, rgba, depth, owner, &stats));
            size_t pixel = 32 * 64 + 32;
            TEST_ASSERT(owner[pixel] >= 0 &&
                        state->layout.objectStore.items[owner[pixel]].objectId == ids[1]);
            TEST_ASSERT(stats.meshCount == 1 && rgba[pixel * 4 + 3] == 255);
        }
        char* after = Layout_SaveToString(&state->layout);
        TEST_ASSERT(before && after && !strcmp(before, after));
        free(before); free(after);
    }
    ld_test_shutdown_runtime();
    return true;
}
static bool camera_path_sample_and_editor_restore(void) {
    ld_test_init_runtime();
    GlobalState *s = Global_Get();
    size_t index = 0;
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultCameraPath(&s->layout.sceneAuthoring, &index));
    LineDrawingScenePath *path = &s->layout.sceneAuthoring.paths[index];
    LineDrawingSceneCamera *camera =
        Layout_SceneAuthoringState_FindCameraForPath(&s->layout.sceneAuthoring, path);
    TEST_ASSERT(camera != NULL);
    path->normalized_distance = .45f;
    path->playing = true;
    for (size_t i = 0; i < s->layout.sceneAuthoring.camera_count; ++i)
        if (&s->layout.sceneAuthoring.cameras[i] == camera)
            s->cameraView.selected = i;
    LineDrawingSceneCameraPose expected;
    TEST_ASSERT(Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(camera, path, .45f, &expected));
    Vec3 points[LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS];
    memcpy(points, path->control_points, sizeof(points));
    Grid grid = s->grid;
    FreeViewCamera editor = s->freeViewCamera;
    s->spaceMode = SPACE_MODE_2D;
    s->sectionView.mode = LAYOUT_SECTION_CUTAWAY;
    TEST_ASSERT(CameraView_Enter(s, true));
    TEST_ASSERT(ld_test_vec3_nearly_equal(s->cameraView.eye, expected.position));
    TEST_ASSERT(!path->playing && path->normalized_distance == .45f);
    TEST_ASSERT(!memcmp(points, path->control_points, sizeof(points)));
    TEST_ASSERT(!CameraView_Save(s, true));
    CameraView_Exit(s);
    TEST_ASSERT(s->spaceMode == SPACE_MODE_2D && s->sectionView.mode == LAYOUT_SECTION_CUTAWAY);
    TEST_ASSERT(!memcmp(&grid, &s->grid, sizeof(grid)) &&
                !memcmp(&editor, &s->freeViewCamera, sizeof(editor)));
    ld_test_shutdown_runtime();
    return true;
}
bool camera_view_run_tests(void) {
    const TestCase cases[] = {
        {"guides_never_occlude_physical_panels", camera_guide_volumes_do_not_occlude_physical_panels},
        {"native_perspective_depth_near_clipping", camera_native_depth_order_and_near_clip},
        {"path_sample_pause_editor_restore", camera_path_sample_and_editor_restore},
        {"perspective_rays_clipping", camera_projection_roundtrip_and_clipping},
        {"alt_no_click_focus_roll_sleep", camera_navigation_focus_alt_roll_and_idle},
        {"saved_pose_undo_roundtrip_path_guard", camera_saved_pose_undo_persistence_and_path_guard},
        {"responsive_form_units_capture", camera_form_responsive_and_text_capture}};
    return run_test_cases("CameraView", cases, sizeof(cases) / sizeof(cases[0]));
}
