#include "test_layout_internal.h"
#include "Layout/layout_section.h"
#include "Layout/layout_engineering.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "Editor/editor_numeric_edit.h"
#include "UI/ui_panel_section.h"
#include "UI/input_ui_panel.h"
#include "UI/ui_panel_internal.h"

static uint32_t panel(GlobalState* state, Vec3 center, float thickness) {
    RectPrismPrimitiveCreateParams p = {
        .width = 1,
        .height = 1,
        .depth = thickness,
        .useExplicitFrame = true,
        .explicitFrame = {.origin = center, .axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    uint32_t id = 0;
    if (!Layout_CreateRectPrismPrimitive(&state->layout, &p, &id, NULL))
        return 0;
    Object3D* o = Layout_ObjectStore_Find(&state->layout.objectStore, id);
    snprintf(o->info.entity_type, sizeof(o->info.entity_type), "Panel");
    return id;
}
static float triangle_area(LayoutSurfaceTriangle t) {
    return .5f * Vec3_Length(Vec3_Cross(Vec3_Sub(t.b, t.a), Vec3_Sub(t.c, t.a)));
}
static bool test_section_geometry_winding_and_units(void) {
    ld_test_init_runtime();
    GlobalState* s = Global_Get();
    uint32_t id = panel(s, (Vec3){0, 0, 0}, .2f);
    const Object3D* o = Layout_ObjectStore_FindConst(&s->layout.objectStore, id);
    LayoutSurfaceTriangle t[LAYOUT_SURFACE_MAX_TRIANGLES];
    TEST_ASSERT(Layout_BuildNativeSurface(o, NULL, 1, t) == 12);
    for (int i = 0; i < 12; ++i) {
        Vec3 normal = Vec3_Cross(Vec3_Sub(t[i].b, t[i].a), Vec3_Sub(t[i].c, t[i].a));
        Vec3 center = Vec3_Scale(Vec3_Add(Vec3_Add(t[i].a, t[i].b), t[i].c), 1.0f / 3);
        TEST_ASSERT(Vec3_Dot(normal, center) > 0);
    }
    LayoutSectionView v = {.mode = LAYOUT_SECTION_EXACT, .axis = 2, .position_meters = .01};
    TEST_ASSERT(Layout_BuildNativeSurface(o, &v, .1, t) == 2);
    TEST_ASSERT(fabs(triangle_area(t[0]) + triangle_area(t[1]) - 1) < 1e-6);
    TEST_ASSERT(fabs(t[0].a.z - .1) < 1e-6 && t[0].cap);
    v.position_meters = .011;
    TEST_ASSERT(Layout_BuildNativeSurface(o, &v, .1, t) == 0);
    v = (LayoutSectionView){.mode = LAYOUT_SECTION_CUTAWAY, .axis = 0};
    for (int flipped = 0; flipped < 2; ++flipped) {
        v.flipped = flipped;
        size_t n = Layout_BuildNativeSurface(o, &v, 1, t), caps = 0;
        TEST_ASSERT(n > 2);
        for (size_t i = 0; i < n; ++i) {
            TEST_ASSERT((flipped ? -1 : 1) * t[i].a.x <= 1e-6);
            TEST_ASSERT((flipped ? -1 : 1) * t[i].b.x <= 1e-6);
            TEST_ASSERT((flipped ? -1 : 1) * t[i].c.x <= 1e-6);
            if (t[i].cap) {
                ++caps;
                TEST_ASSERT((flipped ? -1 : 1) *
                                Vec3_Cross(Vec3_Sub(t[i].b, t[i].a), Vec3_Sub(t[i].c, t[i].a)).x >
                            0);
            }
        }
        TEST_ASSERT(caps == 2);
    }
    ld_test_shutdown_runtime();
    return true;
}
static bool deny_history(const Layout* layout, void* context) {
    (void)layout;
    (void)context;
    return false;
}
static bool test_thickness_anchored_rotated_undo_atomicity(void) {
    ld_test_init_runtime();
    GlobalState* s = Global_Get();
    uint32_t id = panel(s, (Vec3){1, 2, 3}, .018f);
    Object3D* o = Layout_ObjectStore_Find(&s->layout.objectStore, id);
    o->rectPrism.frame.axisU = (Vec3){0, 0, 1};
    o->rectPrism.frame.axisV = (Vec3){0, 1, 0};
    o->rectPrism.frame.normal = (Vec3){-1, 0, 0};
    s->layout.metersPerWorldUnit = .1;
    Editor_ClearHistory(&s->editor);
    TEST_ASSERT(
        Layout_SetPanelThickness(&s->layout, id, .0024, 1, Editor_ReserveGeometryHistory, &s->editor));
    o = Layout_ObjectStore_Find(&s->layout.objectStore, id);
    TEST_ASSERT(fabs(o->rectPrism.depth - .024) < 1e-6);
    TEST_ASSERT(fabs(o->transform.position.x - 1.003) < 1e-6);
    TEST_ASSERT(fabs(o->rectPrism.frame.origin.x - 1.003) < 1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 1);
    TEST_ASSERT(Editor_Undo(&s->editor, &s->layout));
    TEST_ASSERT(fabs(s->layout.objectStore.items[0].rectPrism.depth - .018) < 1e-6);
    TEST_ASSERT(Editor_Redo(&s->editor, &s->layout));
    char* before = Layout_SaveToString(&s->layout);
    TEST_ASSERT(!Layout_SetPanelThickness(&s->layout, id, .003, 0, deny_history, NULL));
    TEST_ASSERT(!Layout_SetPanelThickness(&s->layout, id, .1, 0, NULL, NULL));
    char* after = Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    Layout reopened;
    Layout_Init(&reopened, 1);
    TEST_ASSERT(Layout_LoadFromString(&reopened, after));
    TEST_ASSERT(fabs(reopened.objectStore.items[0].rectPrism.depth - .024) < 1e-6);
    Layout_Free(&reopened);
    free(before);
    free(after);
    ld_test_shutdown_runtime();
    return true;
}
static bool test_native_depth_is_order_independent(void) {
    enum { W = 128, H = 128 };
    uint8_t rgba[W * H * 4];
    float depth[W * H];
    int32_t owner[W * H];
    ld_test_init_runtime();
    GlobalState* s = Global_Get();
    uint32_t near_id = panel(s, (Vec3){0, 0, -1}, .1f);
    TEST_ASSERT(near_id && panel(s, (Vec3){0, 0, 1}, .1f));
    SpaceViewContext view = {.plane = {.axis = VIEW_PLANE_XY}};
    Grid grid = {.gridSize = 1, .scale = 100, .offsetX = -.64f, .offsetY = -.64f};
    for (int order = 0; order < 2; ++order) {
        memset(rgba, 0, sizeof(rgba));
        for (size_t i = 0; i < W * H; ++i) {
            depth[i] = INFINITY;
            owner[i] = -1;
        }
        LayoutMeshSolidPreviewFrameStats stats = {0};
        TEST_ASSERT(Layout_RasterNativeSurfaces(&s->layout, NULL, &view, &grid, (SDL_Rect){0, 0, W, H}, 1, W,
                                                H, true, rgba, depth, owner, &stats));
        int32_t index = owner[64 * W + 64];
        TEST_ASSERT(index >= 0 && s->layout.objectStore.items[index].objectId == near_id);
        TEST_ASSERT(rgba[(64 * W + 64) * 4 + 3] == 255 && fabs(depth[64 * W + 64] + 1.05) < 1e-5);
        Object3D tmp = s->layout.objectStore.items[0];
        s->layout.objectStore.items[0] = s->layout.objectStore.items[1];
        s->layout.objectStore.items[1] = tmp;
    }
    ld_test_shutdown_runtime();
    return true;
}
static bool click_section(int action) {
    SDL_Rect r = {0};
    return UIPanel_SectionControlRect(action, &r) && UIPanel_SectionClick(r.x + r.w / 2, r.y + r.h / 2);
}
static bool test_existing_cabinet_section_controls_and_empty_interior(void) {
    enum { W = 240, H = 720 };
    uint8_t* rgba = calloc(W * H, 4);
    float* depth = malloc(W * H * sizeof(float));
    int32_t* owner = malloc(W * H * sizeof(int32_t));
    TEST_ASSERT(rgba && depth && owner);
    ld_test_init_runtime();
    GlobalState* s = Global_Get();
    UIPanelState* ui = UIPanel_Get();
    TEST_ASSERT(Layout_LoadFromFile(&s->layout, "config/examples/van_connected_sections_s5.layout.json"));
    const Object3D* cabinet = NULL;
    for (size_t i = 0; i < s->layout.objectStore.count; ++i)
        if (!strcmp(s->layout.objectStore.items[i].coreMeta.object_id, "storage_driver"))
            cabinet = &s->layout.objectStore.items[i];
    TEST_ASSERT(cabinet);
    s->editor.selectedObject3DId = cabinet->objectId;
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(1280, 1080);
    Editor_ClearHistory(&s->editor);
    char* before = Layout_SaveToString(&s->layout);
    TEST_ASSERT(click_section(UI_SECTION_SELECTED));
    TEST_ASSERT(!strcmp(s->layout.objectStore.view_query.assembly_id, "storage_driver_unit"));
    TEST_ASSERT(click_section(UI_SECTION_ALONG) && click_section(UI_SECTION_EXACT));
    TEST_ASSERT(s->sectionView.axis == 1 && s->sectionView.mode == LAYOUT_SECTION_EXACT);
    TEST_ASSERT(click_section(UI_SECTION_NEXT));
    TEST_ASSERT(click_section(UI_SECTION_PREVIOUS));
    SDL_Rect track;
    TEST_ASSERT(UIPanel_SectionControlRect(UI_SECTION_TRACK, &track));
    TEST_ASSERT(UIPanel_SectionClick(track.x + 10, track.y + track.h / 2));
    SDL_Event drag = {.type = SDL_MOUSEMOTION};
    drag.motion.x = track.x + track.w - 10;
    TEST_ASSERT(UIPanel_SectionEvent(&drag));
    double lo, hi;
    TEST_ASSERT(Layout_SectionRange(&s->layout, 1, &lo, &hi));
    TEST_ASSERT(fabs(s->sectionView.position_meters - hi) < 1e-6);
    SDL_Event release = {.type = SDL_MOUSEBUTTONUP};
    release.button.button = SDL_BUTTON_LEFT;
    TEST_ASSERT(UIPanel_SectionEvent(&release));
    TEST_ASSERT(click_section(UI_SECTION_POSITION));
    SDL_Event text = {.type = SDL_TEXTINPUT};
    snprintf(text.text.text, sizeof(text.text.text), "-718.1 mm");
    TEST_ASSERT(UIPanel_SectionEvent(&text) && click_section(UI_SECTION_APPLY));
    TEST_ASSERT(fabs(s->sectionView.position_meters + .7181) < 1e-8);
    SpaceViewContext view = SpaceAdapter_BuildViewContext(s);
    Grid grid = {.gridSize = 1, .scale = 400, .offsetX = -.95f, .offsetY = -1.76f};
    for (size_t i = 0; i < W * H; ++i) {
        depth[i] = INFINITY;
        owner[i] = -1;
    }
    LayoutMeshSolidPreviewFrameStats stats = {0};
    TEST_ASSERT(Layout_RasterNativeSurfaces(&s->layout, &s->sectionView, &view, &grid, (SDL_Rect){0, 0, W, H},
                                            1, W, H, true, rgba, depth, owner, &stats));
    /* Actual driver wardrobe: six shell panels plus a shelf. Void stays empty. */
    TEST_ASSERT(owner[200 * W + 120] == -1);
    TEST_ASSERT(owner[358 * W + 120] >= 0); /* shelf, Z ~= 865 mm */
    TEST_ASSERT(owner[200 * W + 21] >= 0);  /* 18 mm backing */
    TEST_ASSERT(stats.meshCount == 5);      /* front/rear ends do not intersect this slice */
    char* after = Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 0);
    const char* output = getenv("LINE_DRAWING_SECTION_ACCEPTANCE_PPM");
    if (output) {
        FILE* f = fopen(output, "wb");
        TEST_ASSERT(f);
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (size_t i = 0; i < W * H; ++i) {
            uint8_t pixel[3] = {15, 19, 23};
            if (owner[i] >= 0)
                memcpy(pixel, rgba + i * 4, 3);
            TEST_ASSERT(fwrite(pixel, 1, 3, f) == 3);
        }
        TEST_ASSERT(fclose(f) == 0);
    }
    TEST_ASSERT(click_section(UI_SECTION_CUTAWAY) && click_section(UI_SECTION_FLIP));
    TEST_ASSERT(!s->sectionView.flipped);
    TEST_ASSERT(click_section(UI_SECTION_OFF));
    TEST_ASSERT(s->sectionView.mode == LAYOUT_SECTION_OFF);
    free(before);
    free(after);
    free(rgba);
    free(depth);
    free(owner);
    ld_test_shutdown_runtime();
    return true;
}
static bool test_panel_thickness_visible_form_and_undo(void) {
    ld_test_init_runtime();
    GlobalState* s = Global_Get();
    UIPanelState* ui = UIPanel_Get();
    uint32_t id = panel(s, (Vec3){0, 0, 0}, .018f);
    TEST_ASSERT(id);
    s->editor.selectedObject3DId = id;
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
    UIPanel_OnWindowResized(1280, 1080);
    Editor_ClearHistory(&s->editor);
    const UIButton* thickness = NULL;
    for (int i = 0; i < ui->count; ++i)
        if (ui->buttons[i].id == UI_BTN_PANEL_THICKNESS)
            thickness = &ui->buttons[i];
    TEST_ASSERT(thickness && thickness->bounds.w > 0 && thickness->bounds.h > 0);
    TEST_ASSERT(UIPanel_HandleClick(thickness->bounds.x + 4, thickness->bounds.y + 4));
    TEST_ASSERT(ui->prismDimensionDialog.active &&
                ui->prismDimensionDialog.target == UI_PRISM_DIMENSION_TARGET_THICKNESS);
    snprintf(ui->prismDimensionDialog.buffer, sizeof(ui->prismDimensionDialog.buffer), "12.7 mm");
    TEST_ASSERT(UIPanel_ApplyPrismDimensionDialog(ui));
    TEST_ASSERT(fabs(Layout_ObjectStore_FindConst(&s->layout.objectStore, id)->rectPrism.depth - .0127) <
                1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 1);
    TEST_ASSERT(Editor_Undo(&s->editor, &s->layout));
    TEST_ASSERT(fabs(Layout_ObjectStore_FindConst(&s->layout.objectStore, id)->rectPrism.depth - .018) <
                1e-6);
    ld_test_shutdown_runtime();
    return true;
}
bool sections_run_tests(void) {
    const TestCase cases[] = {
        {"geometry_winding_units_caps", test_section_geometry_winding_and_units},
        {"anchored_thickness_undo_atomicity", test_thickness_anchored_rotated_undo_atomicity},
        {"native_depth_order_independent", test_native_depth_is_order_independent},
        {"panel_thickness_visible_form_undo", test_panel_thickness_visible_form_and_undo},
        {"existing_cabinet_ui_void_readonly", test_existing_cabinet_section_controls_and_empty_interior}};
    return run_test_cases("Sections", cases, sizeof(cases) / sizeof(cases[0]));
}
