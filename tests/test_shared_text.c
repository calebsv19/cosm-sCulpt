#include "test_layout_internal.h"
#include "UI/shared_text.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"
#include "Input/input_handler.h"
#include "Input/input_routing_policy.h"
#include "Menu/line_drawing_host_menu_internal.h"
#include <stdlib.h>
#include <string.h>

static SDL_Event key(SDL_Keycode code,Uint16 mods) {
    SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=code;e.key.keysym.mod=mods;return e;
}
static bool test_root_utf8_selection_and_cancel(void) {
    ld_test_init_runtime();UIPanel_BeginInputRootDialog();UIPanelState* ui=UIPanel_Get();
    size_t undo=Editor_UndoCount(&Global_Get()->editor);
    char* before=Layout_SaveToString(&Global_Get()->layout);TEST_ASSERT(before);
    SDL_Event e=key(SDLK_a,KMOD_CTRL);TEST_ASSERT(UIPanel_HandleKeyEvent(&e));
    TEST_ASSERT(UIPanel_HandleTextInput("café/光"));TEST_ASSERT(!strcmp(ui->rootDialog.buffer,"café/光"));
    e=key(SDLK_LEFT,KMOD_NONE);TEST_ASSERT(UIPanel_HandleKeyEvent(&e));
    e=key(SDLK_BACKSPACE,KMOD_NONE);TEST_ASSERT(UIPanel_HandleKeyEvent(&e));
    TEST_ASSERT(!strcmp(ui->rootDialog.buffer,"café光"));
    e=key(SDLK_HOME,KMOD_NONE);(void)UIPanel_HandleKeyEvent(&e);
    e=key(SDLK_RIGHT,KMOD_SHIFT);(void)UIPanel_HandleKeyEvent(&e);
    TEST_ASSERT(UIPanel_HandleTextInput("C"));TEST_ASSERT(!strcmp(ui->rootDialog.buffer,"Café光"));
    Input_CancelWindowCapture();TEST_ASSERT(ui->rootDialog.active && !strcmp(ui->rootDialog.buffer,"Café光"));
    e=key(SDLK_ESCAPE,KMOD_NONE);TEST_ASSERT(UIPanel_HandleKeyEvent(&e));TEST_ASSERT(!ui->rootDialog.active);
    char* after=Layout_SaveToString(&Global_Get()->layout);TEST_ASSERT(after && !strcmp(before,after));
    TEST_ASSERT(undo==Editor_UndoCount(&Global_Get()->editor));Layout_FreeString(before);Layout_FreeString(after);
    ld_test_shutdown_runtime();return true;
}
static bool test_capacity_policy_and_preedit(void) {
    ld_test_init_runtime();UIPanel_BeginSaveAsDialog();UIPanelState* ui=UIPanel_Get();
    SDL_Event e=key(SDLK_a,KMOD_CTRL);(void)UIPanel_HandleKeyEvent(&e);TEST_ASSERT(UIPanel_HandleTextInput("safe/name.json"));
    TEST_ASSERT(!strcmp(ui->saveDialog.buffer,"safename.json"));
    char huge[300];memset(huge,'x',sizeof(huge)-1);huge[sizeof(huge)-1]=0;
    TEST_ASSERT(UIPanel_HandleTextInput(huge));TEST_ASSERT(!strcmp(ui->saveDialog.buffer,"safename.json"));
    e=(SDL_Event){.type=SDL_TEXTEDITING};strcpy(e.edit.text,"é");e.edit.length=1;
    TEST_ASSERT(LineDrawingInputPolicyNormalize(&e,true).route_policy==LINE_DRAWING_INPUT_ROUTE_FOCUSED);
    Input_Handle(NULL,&e);TEST_ASSERT(!strcmp(ui->saveDialog.buffer,"safename.json"));
    e=key(SDLK_v,KMOD_CTRL);Input_Handle(NULL,&e);
    TEST_ASSERT(!strcmp(ui->saveDialog.buffer,"safename.json"));
    e=key(SDLK_RETURN,KMOD_NONE);Input_Handle(NULL,&e);TEST_ASSERT(ui->saveDialog.active);
    e=key(SDLK_ESCAPE,KMOD_NONE);Input_Handle(NULL,&e);TEST_ASSERT(ui->saveDialog.active);
    e=key(SDLK_ESCAPE,KMOD_NONE);Input_Handle(NULL,&e);TEST_ASSERT(!ui->saveDialog.active);
    ld_test_shutdown_runtime();return true;
}
static bool test_numeric_rejection_keeps_draft_history(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);uint32_t id;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&id,NULL));s->editor.selectedObject3DId=id;
    TEST_ASSERT(UIPanel_BeginPrismWidthDialog());char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    size_t undo=Editor_UndoCount(&s->editor);SDL_Event e=key(SDLK_a,KMOD_CTRL);(void)UIPanel_HandleKeyEvent(&e);
    TEST_ASSERT(UIPanel_HandleTextInput("20 / 2 mm"));e=key(SDLK_RETURN,KMOD_NONE);TEST_ASSERT(UIPanel_HandleKeyEvent(&e));
    TEST_ASSERT(UIPanel_Get()->prismDimensionDialog.active && !strcmp(UIPanel_Get()->prismDimensionDialog.buffer,"20 / 2 mm"));
    TEST_ASSERT(UIPanel_Get()->prismDimensionDialog.validationMessage[0]);
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(after && !strcmp(before,after));TEST_ASSERT(undo==Editor_UndoCount(&s->editor));
    Layout_FreeString(before);Layout_FreeString(after);ld_test_shutdown_runtime();return true;
}
static bool test_menu_filter_utf8_caret_submit(void) {
    ld_test_init_runtime();LineDrawingHostMenuState* menu=calloc(1,sizeof(*menu));TEST_ASSERT(menu);
    LineDrawingHostMenu_Init(menu);menu->selected_section=LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS;
    menu->focus_region=LINE_DRAWING_HOST_MENU_FOCUS_FILTER;menu->filter_editing=true;
    SDL_Event e={.type=SDL_TEXTINPUT};strcpy(e.text.text,"café光");LineDrawingHostMenuCommand command;
    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command));TEST_ASSERT(!strcmp(menu->filter_query,"café光"));
    e=key(SDLK_LEFT,KMOD_NONE);(void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    e=key(SDLK_BACKSPACE,KMOD_NONE);(void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    TEST_ASSERT(!strcmp(menu->filter_query,"caf光"));
    e=key(SDLK_RETURN,KMOD_NONE);(void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    TEST_ASSERT(menu->focus_region==LINE_DRAWING_HOST_MENU_FOCUS_CONTENT && !menu->filter_editing);
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_NONE);free(menu);ld_test_shutdown_runtime();return true;
}
static bool test_footer_and_scalar_wrap_geometry(void) {
    LineDrawingHostMenuLayout layout;line_drawing_host_menu_layout(&layout,960,720);
    TEST_ASSERT(layout.output_root_rect.y+layout.output_root_rect.h<=layout.footer_rect.y+layout.footer_rect.h);
    TEST_ASSERT(LDText_CountLines(NULL,"é光ab",16)==2);
    TEST_ASSERT(LDText_CountLines(NULL,"construction plane",128)==2);
    return true;
}
bool shared_text_run_tests(void) {
    const TestCase cases[]={
        {"RootUtf8SelectionFocusAndCancel",test_root_utf8_selection_and_cancel},
        {"FilenamePolicyCapacityAndPreedit",test_capacity_policy_and_preedit},
        {"NumericValidationKeepsDraftAndHistory",test_numeric_rejection_keeps_draft_history},
        {"LauncherFilterUtf8CaretAndSubmit",test_menu_filter_utf8_caret_submit},
        {"FooterContainmentAndScalarWrap",test_footer_and_scalar_wrap_geometry}
    };
    return run_test_cases("SharedText",cases,sizeof(cases)/sizeof(cases[0]));
}
