#include "Layout/layout_inspection.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "UI/ui_panel_camera.h"
#include "test_layout_internal.h"

static Object3D *named(Layout *layout, const char *id) {
    for (size_t i = 0; i < layout->objectStore.count; ++i)
        if (!strcmp(layout->objectStore.items[i].coreMeta.object_id, id))
            return &layout->objectStore.items[i];
    return NULL;
}
static bool inspection_full_van_inheritance_rotation_and_defaults(void) {
    Layout layout;
    Layout_Init(&layout, 1);
    TEST_ASSERT(Layout_LoadFromFile(&layout, "config/examples/van_construction_f4.layout.json"));
    SpaceViewContext v = {.inspection = true, .camera = {.enabled = true, .yawDeg = 0}};
    Object3D *driver = named(&layout, "storage_driver_door");
    Object3D *passenger = named(&layout, "storage_passenger_shelf");
    TEST_ASSERT(driver && passenger && layout.objectStore.count == 130);
    char *before = Layout_SaveToString(&layout);
    TEST_ASSERT(Layout_InspectionOutlined(&layout.objectStore, driver, &v));
    TEST_ASSERT(!Layout_InspectionOutlined(&layout.objectStore, passenger, &v));
    v.camera.yawDeg = 180;
    TEST_ASSERT(!Layout_InspectionOutlined(&layout.objectStore, driver, &v));
    TEST_ASSERT(Layout_InspectionOutlined(&layout.objectStore, passenger, &v));
    TEST_ASSERT(!Layout_InspectionOutlined(&layout.objectStore, named(&layout, "bed_deck"), &v));
    TEST_ASSERT(!Layout_InspectionOutlined(&layout.objectStore,
                                           named(&layout, "driver_feeder_region"), &v));
    v.inspection = false;
    TEST_ASSERT(!Layout_InspectionOutlined(&layout.objectStore, passenger, &v));
    char *after = Layout_SaveToString(&layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    free(before);
    free(after);
    v.inspection = true;
    v.camera.yawDeg = 90;
    TEST_ASSERT(
        !Layout_InspectionOutlined(&layout.objectStore, driver, &v)); /* Tangent stays physical. */
    /* Both camera projections retain slight backside views on either wall. */
    for (int perspective = 0; perspective < 2; ++perspective) {
        v.perspective.enabled = perspective != 0;
        const float yaw[] = {76, 74, 104, 106};
        Object3D *units[] = {driver, driver, passenger, passenger};
        for (int i = 0; i < 4; ++i) {
            v.camera.yawDeg = yaw[i];
            float radians = yaw[i] * 0.01745329252f;
            v.perspective.forward = (Vec3){cosf(radians), sinf(radians), 0};
            TEST_ASSERT(Layout_InspectionOutlined(&layout.objectStore, units[i], &v) ==
                        (i % 2 != 0)); /* 14 degrees stays solid; 16 becomes outline. */
        }
    }
    v.perspective.enabled = false;
    v.camera.yawDeg = 90;
    LayoutAssembly *a =
        (LayoutAssembly *)Layout_FindAssembly(&layout.objectStore, "storage_driver_unit");
    a->frame.axisU = (Vec3){0, 1, 0};
    a->frame.axisV = (Vec3){-1, 0, 0};
    TEST_ASSERT(
        Layout_InspectionOutlined(&layout.objectStore, driver, &v)); /* Declaring local frame. */
    LayoutEntityInfo info = driver->info;
    info.properties[info.property_count++] =
        (LayoutProperty){.key = "inspection_facing", .kind = LAYOUT_PROPERTY_TEXT, .text = "solid"};
    driver->info = info;
    TEST_ASSERT(
        !Layout_InspectionOutlined(&layout.objectStore, driver, &v)); /* Explicit override. */
    snprintf(driver->info.properties[info.property_count - 1].text, 128, "bad_direction");
    TEST_ASSERT(!Layout_EntityInfoValid(&driver->info));
    Layout_Free(&layout);
    return true;
}
static bool inspection_visible_controls_undo_roundtrip_and_no_navigation_history(void) {
    ld_test_init_runtime();
    GlobalState *s = Global_Get();
    TEST_ASSERT(Layout_LoadFromFile(&s->layout, "config/examples/van_construction_f4.layout.json"));
    Editor_ClearHistory(&s->editor);
    Object3D *o = named(&s->layout, "storage_driver_shelf");
    TEST_ASSERT(o);
    s->editor.selectedObject3DId = o->objectId;
    UIPanel_SetActiveRightTab(UIPanel_Get(), UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(s->screenWidth, s->screenHeight);
    SDL_Rect r;
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_INSPECTION, &r));
    char *before = Layout_SaveToString(&s->layout);
    TEST_ASSERT(UIPanel_CameraClick(r.x + 2, r.y + 2));
    TEST_ASSERT(s->inspectionView && SpaceAdapter_BuildViewContext(s).inspection);
    char *after = Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    free(before);
    free(after);
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 0);
    Object3D *full = named(&s->layout, "bed_deck");
    TEST_ASSERT(full && full->info.property_count == LAYOUT_MAX_PROPERTIES);
    LayoutEntityInfo protected_info = full->info;
    TEST_ASSERT(!Layout_SetInspectionFacing(&s->layout, "bed_deck", LAYOUT_INSPECTION_POS_X,
                                            Layout_GeometryHistory, NULL));
    TEST_ASSERT(
        !memcmp(&protected_info, &named(&s->layout, "bed_deck")->info, sizeof(protected_info)));
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 0);
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_FACING_DETAILS, &r));
    TEST_ASSERT(UIPanel_CameraClick(r.x + 2, r.y + 2));
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_FACING_NEG_X, &r));
    int w = r.w;
    UIPanel_Get()->viewPane.summaryRect.w += 180;
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_FACING_NEG_X, &r));
    TEST_ASSERT(r.w == w + 90);
    UIPanel_OnWindowResized(s->screenWidth, s->screenHeight);
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_FACING_NEG_X, &r));
    RectPrismPrimitive3D physical = o->rectPrism;
    TEST_ASSERT(UIPanel_CameraClick(r.x + 2, r.y + 2));
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 1);
    TEST_ASSERT(Layout_InspectionFacing(Layout_EntityInfo(
                    &s->layout.objectStore, "storage_driver_unit")) == LAYOUT_INSPECTION_NEG_X);
    TEST_ASSERT(!memcmp(&physical, &named(&s->layout, "storage_driver_shelf")->rectPrism,
                        sizeof(physical)));
    char *json = Layout_SaveToString(&s->layout);
    Layout readback;
    Layout_Init(&readback, 1);
    TEST_ASSERT(json && Layout_LoadFromString(&readback, json));
    free(json);
    TEST_ASSERT(Layout_InspectionFacing(Layout_EntityInfo(
                    &readback.objectStore, "storage_driver_unit")) == LAYOUT_INSPECTION_NEG_X);
    Layout_Free(&readback);
    TEST_ASSERT(Editor_Undo(&s->editor, &s->layout));
    TEST_ASSERT(Layout_InspectionFacing(Layout_EntityInfo(
                    &s->layout.objectStore, "storage_driver_unit")) == LAYOUT_INSPECTION_POS_X);
    TEST_ASSERT(Editor_Redo(&s->editor, &s->layout));
    TEST_ASSERT(Layout_InspectionFacing(Layout_EntityInfo(
                    &s->layout.objectStore, "storage_driver_unit")) == LAYOUT_INSPECTION_NEG_X);
    TEST_ASSERT(Layout_SetInspectionFacing(&s->layout, "storage_driver_shelf",
                                           LAYOUT_INSPECTION_SOLID, NULL, NULL));
    TEST_ASSERT(Layout_SetInspectionFacing(&s->layout, "storage_driver_shelf",
                                           LAYOUT_INSPECTION_INHERIT, NULL, NULL));
    TEST_ASSERT(Layout_InspectionFacing(&named(&s->layout, "storage_driver_shelf")->info) ==
                LAYOUT_INSPECTION_INHERIT);
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_FACING_DETAILS, &r));
    TEST_ASSERT(UIPanel_CameraClick(r.x + 2, r.y + 2)); /* Restore collapsed form. */
    Global_OnLayoutLoaded(NULL);
    TEST_ASSERT(!s->inspectionView);
    ld_test_shutdown_runtime();
    return true;
}
static bool inspection_raster_interior_picks_far_surface_and_edges_only(void) {
    Layout layout;
    Layout_Init(&layout, 1);
    uint32_t ids[2];
    for (int i = 0; i < 2; ++i) {
        RectPrismPrimitiveCreateParams p = {.width = 1,
                                            .height = 1,
                                            .depth = .2f,
                                            .useExplicitFrame = true,
                                            .explicitFrame = {.origin = {0, i ? 4 : 2, 0},
                                                              .axisU = {1, 0, 0},
                                                              .axisV = {0, 0, 1},
                                                              .normal = {0, 1, 0}}};
        TEST_ASSERT(Layout_CreateRectPrismPrimitive(&layout, &p, &ids[i], NULL));
        Object3D *o = Layout_ObjectStore_Find(&layout.objectStore, ids[i]);
        TEST_ASSERT(Layout_SetInspectionFacing(
            &layout, o->coreMeta.object_id, i ? LAYOUT_INSPECTION_NEG_Z : LAYOUT_INSPECTION_POS_Z,
            NULL, NULL));
    }
    SpaceViewContext v = {.inspection = true,
                          .perspective = {.enabled = true,
                                          .forward = {0, 1, 0},
                                          .right = {1, 0, 0},
                                          .up = {0, 0, 1},
                                          .focal = 1,
                                          .tan_half_fov = 1,
                                          .aspect = 1,
                                          .near_clip = .01f,
                                          .far_clip = 10}};
    Grid grid = {.gridSize = 1, .scale = 32, .offsetX = -1, .offsetY = -1};
    uint8_t rgba[64 * 64 * 4], near_rgba[64 * 64 * 4];
    float depth[64 * 64], near_depth[64 * 64];
    int32_t owner[64 * 64], near_owner[64 * 64];
    size_t center = 32 * 64 + 32;
    for (int side = 0; side < 2; ++side) {
        memset(rgba, 0, sizeof(rgba));
        memset(near_rgba, 0, sizeof(near_rgba));
        for (int i = 0; i < 64 * 64; ++i) {
            depth[i] = near_depth[i] = INFINITY;
            owner[i] = near_owner[i] = -1;
        }
        LayoutMeshSolidPreviewFrameStats stats = {0};
        TEST_ASSERT(Layout_RasterNativeSurfaces(&layout, NULL, &v, &grid, (SDL_Rect){0, 0, 64, 64},
                                                1, 64, 64, true, rgba, depth, owner, &stats));
        TEST_ASSERT(owner[center] == (side ? 0 : 1));
        TEST_ASSERT(Layout_RasterNativeSurfacePass(&layout, NULL, &v, &grid,
                                                   (SDL_Rect){0, 0, 64, 64}, 1, 64, 64, true, true,
                                                   near_rgba, near_depth, near_owner, &stats));
        TEST_ASSERT(near_owner[center] == (side ? 1 : 0));
        TEST_ASSERT(Layout_ComposeInspectionOutlines(rgba, depth, owner, near_rgba, near_depth,
                                                     near_owner, 64, 64, -1, -1) > 0);
        TEST_ASSERT(owner[center] ==
                    (side ? 0 : 1)); /* No invisible foreground body intercepts the click. */
        TEST_ASSERT(!near_rgba[4 * center + 3] && rgba[4 * center + 3] == 255);
        bool edge = false;
        for (size_t i = 0; i < 64 * 64; ++i)
            if (owner[i] == (side ? 1 : 0)) {
                TEST_ASSERT(near_rgba[4 * i + 3] == 88);
                edge = true;
            }
        TEST_ASSERT(edge);
        v.perspective.eye.y = 6;
        v.perspective.forward.y = -1;
    }
    v.inspection = false;
    memset(rgba, 0, sizeof(rgba));
    for (int i = 0; i < 64 * 64; ++i) {
        depth[i] = INFINITY;
        owner[i] = -1;
    }
    LayoutMeshSolidPreviewFrameStats stats = {0};
    TEST_ASSERT(Layout_RasterNativeSurfaces(&layout, NULL, &v, &grid, (SDL_Rect){0, 0, 64, 64}, 1,
                                            64, 64, true, rgba, depth, owner, &stats));
    TEST_ASSERT(owner[center] == 1); /* Switching off restores physical occlusion. */
    Layout_Free(&layout);
    return true;
}
bool inspection_view_run_tests(void) {
    const TestCase cases[] = {
        {"full_van_direction_inheritance_rotation_defaults",
         inspection_full_van_inheritance_rotation_and_defaults},
        {"visible_controls_undo_persistence_view_only",
         inspection_visible_controls_undo_roundtrip_and_no_navigation_history},
        {"two_sided_raster_far_surface_picking",
         inspection_raster_interior_picks_far_surface_and_edges_only}};
    return run_test_cases("InspectionView", cases, sizeof(cases) / sizeof(cases[0]));
}
