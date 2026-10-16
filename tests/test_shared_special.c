#include "UI/ui_panel_camera.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "Input/input_handler.h"
#include "UI/panel/ui_panel_visibility.h"
#include "UI/shared_special.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel_routes.h"
#include "UI/ui_panel_section.h"
#include "test_layout_internal.h"
static SDL_Event pointer(Uint32 type, SDL_Rect r) {
    SDL_Event e = {.type = type};
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = r.x + r.w / 2;
    e.button.y = r.y + r.h / 2;
    return e;
}
static void key(SDL_Keycode code, Uint16 mods) {
    SDL_Event e = {.type = SDL_KEYDOWN};
    e.key.keysym.sym = code;
    e.key.keysym.mod = mods;
    Input_Handle(NULL, &e);
}
static void text(const char *value) {
    SDL_Event e = {.type = SDL_TEXTINPUT};
    snprintf(e.text.text, sizeof(e.text.text), "%s", value);
    Input_Handle(NULL, &e);
}
static bool parts_click(int action) {
    SDL_Rect r;
    TEST_ASSERT(UIPanel_PartsControlRect(action, &r));
    SDL_Event e = pointer(SDL_MOUSEBUTTONDOWN, r);
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    return true;
}
static bool setup_parts(void) {
    ld_test_init_runtime();
    LDViewControls_Close();
    Global_SetWindowSize(1800, 1800);
    UIPanel_OnWindowResized(1800, 1800);
    UIPanel_PartsEnterMode(1);
    TEST_ASSERT(parts_click(PARTS_NEW));
    return true;
}
static bool release_and_window_cancel(void) {
    TEST_ASSERT(setup_parts());
    UIPanelState *u = UIPanel_Get();
    SDL_Rect r;
    TEST_ASSERT(UIPanel_PartsControlRect(PARTS_NAME, &r));
    SDL_Event e = pointer(SDL_MOUSEBUTTONDOWN, r);
    Input_Handle(NULL, &e);
    TEST_ASSERT(!u->parts.input);
    Input_CancelWindowCapture();
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    TEST_ASSERT(!u->parts.input);
    e = pointer(SDL_MOUSEBUTTONDOWN, r);
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    TEST_ASSERT(u->parts.input == PARTS_NAME);
    size_t count = Global_Get()->layout.objectStore.assembly_count,
           undo = Editor_UndoCount(&Global_Get()->editor);
    text("Unsaved draft");
    TEST_ASSERT(!strcmp(u->parts.draft.label, "Unsaved draft"));
    Input_CancelWindowCapture();
    TEST_ASSERT(!strcmp(u->parts.draft.label, "Unsaved draft"));
    key(SDLK_ESCAPE, 0);
    TEST_ASSERT(!u->parts.input && !strcmp(u->parts.draft.label, "Unsaved draft"));
    TEST_ASSERT(Global_Get()->layout.objectStore.assembly_count == count &&
                Editor_UndoCount(&Global_Get()->editor) == undo);
    ld_test_shutdown_runtime();
    return true;
}
static bool utf8_selection_preedit_capacity(void) {
    TEST_ASSERT(setup_parts());
    TEST_ASSERT(parts_click(PARTS_NAME));
    UIPanelState *u = UIPanel_Get();
    text("café/光");
    key(SDLK_LEFT, 0);
    key(SDLK_BACKSPACE, 0);
    TEST_ASSERT(!strcmp(u->parts.draft.label, "café光"));
    key(SDLK_HOME, 0);
    key(SDLK_RIGHT, KMOD_SHIFT);
    text("C");
    TEST_ASSERT(!strcmp(u->parts.draft.label, "Café光"));
    key(SDLK_a, KMOD_CTRL);
    char huge[32];
    memset(huge, 'x', 31);
    huge[31] = 0;
    text(huge);
    text(huge);
    text(huge);
    char saved[96];
    strcpy(saved, u->parts.draft.label);
    text("four");
    TEST_ASSERT(!strcmp(saved, u->parts.draft.label));
    SDL_Event e = {.type = SDL_TEXTEDITING};
    strcpy(e.edit.text, "é");
    e.edit.length = 1;
    Input_Handle(NULL, &e);
    key(SDLK_RETURN, 0);
    TEST_ASSERT(u->parts.input == PARTS_NAME && !strcmp(saved, u->parts.draft.label));
    key(SDLK_ESCAPE, 0);
    TEST_ASSERT(u->parts.input == PARTS_NAME);
    key(SDLK_ESCAPE, 0);
    TEST_ASSERT(!u->parts.input);
    ld_test_shutdown_runtime();
    return true;
}
static bool draft_save_history(void) {
    TEST_ASSERT(setup_parts());
    TEST_ASSERT(parts_click(PARTS_NAME));
    text("Panel draft");
    key(SDLK_RETURN, 0);
    GlobalState *s = Global_Get();
    TEST_ASSERT(s->layout.objectStore.assembly_count == 0);
    size_t undo = Editor_UndoCount(&s->editor);
    TEST_ASSERT(parts_click(PARTS_SAVE));
    TEST_ASSERT(s->layout.objectStore.assembly_count == 1 &&
                Editor_UndoCount(&s->editor) == undo + 1);
    TEST_ASSERT(!strcmp(s->layout.objectStore.assemblies[0].info.label, "Panel draft"));
    TEST_ASSERT(Editor_Undo(&s->editor, &s->layout) && !s->layout.objectStore.assembly_count);
    ld_test_shutdown_runtime();
    return true;
}
static bool visibility_occlusion_and_stale_row(void) {
    ld_test_init_runtime();
    LDViewControls_Close();
    Global_SetWindowSize(1280, 900);
    UIPanel_OnWindowResized(1280, 900);
    LayoutAssembly a = {.frame = {.axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    strcpy(a.info.label, "Same label");
    strcpy(a.info.entity_type, "Assembly");
    TEST_ASSERT(Layout_EditAssembly(&Global_Get()->layout, &a, NULL, NULL, NULL));
    SDL_Rect r;
    TEST_ASSERT(LDViewControls_Rect(LD_VIEW_VISIBILITY, &r));
    TEST_ASSERT(LDViewControls_Click(r.x + 2, r.y + 2));
    TEST_ASSERT(UIPanel_VisibilityRect(LD_VIEW_ROW, &r));
    SDL_Event e = pointer(SDL_MOUSEBUTTONDOWN, r);
    Input_Handle(NULL, &e);
    strcpy(Global_Get()->layout.objectStore.assemblies[0].id, "replacement");
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    KitUiSurface surface = {0};
    kit_ui_surface_begin(&surface, 1);
    LDSpecial_Collect(&surface);
    TEST_ASSERT(kit_ui_surface_end(&surface).code == CORE_OK && surface.count > 0);
    for (uint32_t i = 0; i < surface.count; ++i)
        TEST_ASSERT(surface.keys[i].domain == 27);
    TEST_ASSERT(UIPanel_VisibilityRect(LD_VIEW_SEARCH, &r));
    e = pointer(SDL_MOUSEBUTTONDOWN, r);
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    text("Same");
    TEST_ASSERT(LDViewControls_MatchingCount() == 1);
    key(SDLK_a, KMOD_CTRL);
    text("absent");
    TEST_ASSERT(LDViewControls_MatchingCount() == 0);
    key(SDLK_ESCAPE, 0);
    TEST_ASSERT(!LDViewControls_ActivePopup());
    ld_test_shutdown_runtime();
    return true;
}
static bool section_discrete_and_drag_ownership(void) {
    ld_test_init_runtime();
    Global_SetWindowSize(1400, 1200);
    UIPanel_SetActiveRightTab(UIPanel_Get(), UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(1400, 1200);
    SDL_Rect r;
    TEST_ASSERT(UIPanel_SectionControlRect(UI_SECTION_EXACT, &r));
    SDL_Event e = pointer(SDL_MOUSEBUTTONDOWN, r);
    Input_Handle(NULL, &e);
    TEST_ASSERT(Global_Get()->sectionView.mode == LAYOUT_SECTION_OFF);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    TEST_ASSERT(Global_Get()->sectionView.mode == LAYOUT_SECTION_EXACT);
    TEST_ASSERT(UIPanel_SectionControlRect(UI_SECTION_POSITION, &r));
    e = pointer(SDL_MOUSEBUTTONDOWN, r);
    Input_Handle(NULL, &e);
    e.type = SDL_MOUSEBUTTONUP;
    Input_Handle(NULL, &e);
    TEST_ASSERT(UIPanel_Get()->sectionInput);
    text("0.2 m");
    TEST_ASSERT(!strcmp(UIPanel_Get()->sectionText, "0.2 m"));
    TEST_ASSERT(UIPanel_IsCapturingKeyboard());
    key(SDLK_ESCAPE, 0);
    TEST_ASSERT(!UIPanel_Get()->sectionInput);
    ld_test_shutdown_runtime();
    return true;
}
static bool repeated_actions_custom_query(void) {
    ld_test_init_runtime();
    LDViewControls_Close();
    UIPanel_Get()->parts.custom_filter_open=false;
    Global_SetWindowSize(1800, 1800);
    UIPanel_OnWindowResized(1800, 1800);
    UIPanel_PartsEnterMode(2);
    TEST_ASSERT(parts_click(PARTS_CUSTOM_FILTER));
    KitUiSurface surface = {0};
    kit_ui_surface_begin(&surface, 1);
    LDSpecial_Collect(&surface);
    TEST_ASSERT(kit_ui_surface_end(&surface).code == CORE_OK);
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(parts_click(PARTS_KEY));
    text("subsystem");
    key(SDLK_RETURN, 0);
    TEST_ASSERT(parts_click(PARTS_VALUE));
    text("electrical");
    key(SDLK_RETURN, 0);
    TEST_ASSERT(parts_click(PARTS_APPLY_FILTER));
    TEST_ASSERT(!strcmp(Global_Get()->layout.objectStore.view_query.property_key, "subsystem") &&
                !strcmp(Global_Get()->layout.objectStore.view_query.property_value, "electrical"));
    ld_test_shutdown_runtime();
    return true;
}
static bool popup_preserves_underlying_camera_draft(void) {
    ld_test_init_runtime();LDViewControls_Close();Global_SetWindowSize(1800,2000);
    RectPrismPrimitiveCreateParams params;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);uint32_t id;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&Global_Get()->layout,&params,&id,NULL));
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_VIEW);TEST_ASSERT(CameraView_Enter(Global_Get(),false));UIPanel_OnWindowResized(1800,2000);
    SDL_Rect box;if(!UIPanel_CameraControlRect(UI_CAMERA_X,&box)){TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_DETAILS,&box));TEST_ASSERT(UIPanel_CameraClick(box.x+2,box.y+2));}
    TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_X,&box));TEST_ASSERT(UIPanel_CameraClick(box.x+2,box.y+2));text("23 mm");
    LineDrawingEditorTopbar_UpdateControls();TEST_ASSERT(LDViewControls_Rect(LD_VIEW_MORE,&box));TEST_ASSERT(LDViewControls_Click(box.x+2,box.y+2));
    key(SDLK_ESCAPE,0);TEST_ASSERT(!LDViewControls_ActivePopup());LDInlineField draft={0};TEST_ASSERT(LDInline_Camera(&draft) && !strcmp(draft.text,"23 mm"));
    ld_test_shutdown_runtime();return true;
}
bool shared_special_run_tests(void) {
    const TestCase cases[] = {{"popup_preserves_underlying_camera_draft", popup_preserves_underlying_camera_draft},
        {"repeated_actions_custom_query", repeated_actions_custom_query},
        {"release_and_window_cancel", release_and_window_cancel},
        {"utf8_selection_preedit_capacity", utf8_selection_preedit_capacity},
        {"draft_save_history", draft_save_history},
        {"visibility_occlusion_and_stale_row", visibility_occlusion_and_stale_row},
        {"section_discrete_and_drag_ownership", section_discrete_and_drag_ownership}};
    return run_test_cases("SharedSpecial", cases, sizeof(cases) / sizeof(*cases));
}
