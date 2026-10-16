#include "Input/input_handler.h"
#include "UI/font_manager.h"
#include "UI/object_workspace_viewport_hud.h"
#include "UI/shared_controls.h"
#include "UI/shared_overlay.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include "kit_workspace_authoring_ui.h"
#include "test_layout_internal.h"
static SDL_Event pointer(Uint32 type, SDL_Rect b) {
    SDL_Event e = {.type = type};
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = b.x + b.w / 2;
    e.button.y = b.y + b.h / 2;
    return e;
}
static bool setup_hud(void) {
    ld_test_init_runtime();
    LDViewControls_Close();
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    Global_SetWindowSize(1280, 900);
    UIPanel_OnWindowResized(1280, 900);
    RectPrismPrimitiveCreateParams p;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);
    uint32_t id = 0;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&Global_Get()->layout, &p, &id, NULL));
    Global_Get()->editor.selectedObject3DId = id;
    return true;
}
static bool hud_release_cancel_modal(void) {
    TEST_ASSERT(setup_hud());
    GlobalState *s = Global_Get();
    SDL_Rect box;
    TEST_ASSERT(LineDrawingObjectWorkspaceViewportHud_Rect(s, 1, &box));
    char *before = Layout_SaveToString(&s->layout);
    size_t undo = Editor_UndoCount(&s->editor);
    SDL_Event e = pointer(SDL_MOUSEBUTTONDOWN, box);
    Input_Handle(NULL, &e);
    TEST_ASSERT(s->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    Input_CancelWindowCapture();
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    TEST_ASSERT(s->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    char *after = Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && !strcmp(before, after));
    Layout_FreeString(before);
    Layout_FreeString(after);
    e = pointer(SDL_MOUSEBUTTONDOWN, box);
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    TEST_ASSERT(s->editor.selectedObjectAssetFace == OBJECT3D_FACE_RECT_PRISM_POS_N);
    TEST_ASSERT(Editor_UndoCount(&s->editor) == undo);
    UIPanel_BeginInputRootDialog();
    strcpy(UIPanel_Get()->rootDialog.buffer, "retained-draft");
    TEST_ASSERT(!LineDrawingObjectWorkspaceViewportHud_Rect(s, 1, &box));
    Input_CancelWindowCapture();
    TEST_ASSERT(!strcmp(UIPanel_Get()->rootDialog.buffer, "retained-draft"));
    ld_test_shutdown_runtime();
    return true;
}
static bool viewport_notice_and_hud_fit(void) {
    TEST_ASSERT(setup_hud());
    GlobalState *s = Global_Get();
    strcpy(s->layout.geometryMessage,
           "A deliberately long bounded notice with accented café text; the model retains this "
           "diagnostic while the viewport wraps and clips it.");
    SDL_Rect notice, panel;
    TEST_ASSERT(LDOverlay_NoticeRect(s, &notice));
    TEST_ASSERT(LineDrawingObjectWorkspaceViewportHud_Rect(s, -1, &panel));
    TEST_ASSERT(panel.y + panel.h <= notice.y);
    CorePaneRect view;
    TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&s->paneHost, &view));
    TEST_ASSERT(panel.x >= view.x && panel.x + panel.w <= view.x + view.width);
    TEST_ASSERT(notice.x >= view.x && notice.x + notice.w <= view.x + view.width &&
                notice.y + notice.h <= view.y + view.height);
    Global_SetWindowSize(500, 260);
    UIPanel_OnWindowResized(500, 260);
    TEST_ASSERT(!LineDrawingObjectWorkspaceViewportHud_Rect(s, -1, &panel));
    TEST_ASSERT(s->layout.geometryMessage[0]);
    ld_test_shutdown_runtime();
    return true;
}
static bool setup_authoring(void) {
    ld_test_init_runtime();
    TEST_ASSERT(FontManager_Init());
    LDViewControls_Close();
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_Enter(Global_Get()).code == CORE_OK);
    return true;
}
static SDL_Rect author_button(const GlobalState *s, int id) {
    KitWorkspaceAuthoringOverlayButton b[4];
    uint32_t n = kit_workspace_authoring_ui_build_overlay_buttons(
        s->screenWidth, 1, LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(s), b, 4);
    for (uint32_t i = 0; i < n; ++i)
        if ((int)b[i].id == id)
            return (SDL_Rect){b[i].rect.x, b[i].rect.y, b[i].rect.width, b[i].rect.height};
    return (SDL_Rect){0};
}
static bool authoring_release_resize_cancel(void) {
    TEST_ASSERT(setup_authoring());
    GlobalState *s = Global_Get();
    SDL_Rect box = author_button(s, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE);
    SDL_Event e = pointer(SDL_MOUSEBUTTONDOWN, box);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(s));
    Input_CancelWindowCapture();
    e.type = SDL_MOUSEBUTTONUP;
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(s));
    e = pointer(SDL_MOUSEBUTTONDOWN, box);
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    s->screenWidth += 80;
    e.type = SDL_MOUSEBUTTONUP;
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(s));
    box = author_button(s, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE);
    e = pointer(SDL_MOUSEBUTTONDOWN, box);
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    e.type = SDL_MOUSEBUTTONUP;
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(s));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_Cancel(s).code == CORE_OK);
    FontManager_Quit();
    ld_test_shutdown_runtime();
    return true;
}
static bool authoring_disabled_space_and_draft(void) {
    TEST_ASSERT(setup_authoring());
    GlobalState *s = Global_Get();
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_CycleOverlay(s).code == CORE_OK);
    KitWorkspaceAuthoringFontThemeLayout l = {0};
    TEST_ASSERT(LDOverlay_FontThemeLayout(s, &l));
    KitRenderRect r = l.font_preset_buttons[2];
    SDL_Rect b = {r.x, r.y, r.width, r.height};
    SDL_Event e = pointer(SDL_MOUSEBUTTONDOWN, b);
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    e.type = SDL_MOUSEBUTTONUP;
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    TEST_ASSERT(!s->workspaceAuthoring.font_theme_button_click_count);
    e = (SDL_Event){.type = SDL_KEYDOWN};
    e.key.keysym.sym = SDLK_RIGHT;
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    int zoom = FontManager_GetZoomStep();
    e.key.keysym.sym = SDLK_SPACE;
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    TEST_ASSERT(FontManager_GetZoomStep() == zoom);
    e.type = SDL_KEYUP;
    (void)LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(s, &e);
    TEST_ASSERT(FontManager_GetZoomStep() == zoom - 1);
    Input_CancelWindowCapture();
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_Active(s) &&
                FontManager_GetZoomStep() == zoom - 1);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_Cancel(s).code == CORE_OK);
    TEST_ASSERT(FontManager_GetZoomStep() == zoom);
    FontManager_Quit();
    ld_test_shutdown_runtime();
    return true;
}
bool shared_hud_run_tests(void) {
    const TestCase cases[] = {
        {"hud_release_cancel_modal", hud_release_cancel_modal},
        {"viewport_notice_and_hud_fit", viewport_notice_and_hud_fit},
        {"authoring_release_resize_cancel", authoring_release_resize_cancel},
        {"authoring_disabled_space_and_draft", authoring_disabled_space_and_draft}};
    return run_test_cases("SharedHud", cases, sizeof(cases) / sizeof(*cases));
}
