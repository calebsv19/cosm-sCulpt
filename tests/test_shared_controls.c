#include "test_layout_internal.h"
#include "UI/shared_controls.h"
#include "UI/ui_panel.h"
#include "Input/input_handler.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "Menu/line_drawing_host_menu_internal.h"
#include <stdlib.h>
#include <string.h>

static SDL_Event button(Uint32 type, int x, int y) {
    SDL_Event e={.type=type}; e.button.button=SDL_BUTTON_LEFT; e.button.x=x; e.button.y=y; return e;
}
static bool test_menu_release_and_cancel(void) {
    ld_test_init_runtime(); LDControls_CancelAll();
    LineDrawingHostMenuState* menu=calloc(1,sizeof(*menu)); TEST_ASSERT(menu);
    LineDrawingHostMenu_Init(menu);
    LineDrawingHostMenuLayout l;
    line_drawing_host_menu_layout(&l,Global_GetScreenWidth(),Global_GetScreenHeight());
    int x=l.list_view_rect.x+20,y=l.list_view_rect.y+20;
    SDL_Event e=button(SDL_MOUSEBUTTONDOWN,x,y);
    LineDrawingHostMenuCommand command;
    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command));
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_NONE);
    e=button(SDL_MOUSEBUTTONUP,0,0);
    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command));
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_NONE);
    e=button(SDL_MOUSEBUTTONDOWN,x,y); (void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    Input_CancelWindowCapture();
    e=button(SDL_MOUSEBUTTONUP,x,y); (void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_NONE);
    e=button(SDL_MOUSEBUTTONDOWN,x,y); (void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    e=button(SDL_MOUSEBUTTONUP,x,y); (void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR);
    free(menu);ld_test_shutdown_runtime(); return true;
}
static bool test_shell_release_keyboard_modal(void) {
    ld_test_init_runtime(); LDControls_CancelAll();
    UIPanelState* ui=UIPanel_Get();
    SDL_Rect rect=ui->leftTabs[UI_PANEL_LEFT_TAB_FILE].bounds;
    TEST_ASSERT(rect.w>0 && rect.h>0);
    int x=rect.x+rect.w/2,y=rect.y+rect.h/2;
    char* before=Layout_SaveToString(&Global_Get()->layout); TEST_ASSERT(before);
    SDL_Event e=button(SDL_MOUSEBUTTONDOWN,x,y);
    TEST_ASSERT(LDControls_EditorEvent(&e)); TEST_ASSERT(ui->activeLeftTab==UI_PANEL_LEFT_TAB_SCENE);
    e=button(SDL_MOUSEBUTTONUP,x,y); TEST_ASSERT(LDControls_EditorEvent(&e));
    TEST_ASSERT(ui->activeLeftTab==UI_PANEL_LEFT_TAB_FILE);
    e=button(SDL_MOUSEBUTTONDOWN,x,y);(void)LDControls_EditorEvent(&e);
    e.type=SDL_MOUSEBUTTONUP;(void)LDControls_EditorEvent(&e);
    /* Pointer focus admits Enter only once on key release, including repeats. */
    e=(SDL_Event){.type=SDL_KEYDOWN};e.key.keysym.sym=SDLK_RETURN;
    TEST_ASSERT(LDControls_EditorEvent(&e)); e.key.repeat=1; TEST_ASSERT(LDControls_EditorEvent(&e));
    e.type=SDL_KEYUP; e.key.repeat=0;TEST_ASSERT(LDControls_EditorEvent(&e));
    e=button(SDL_MOUSEBUTTONDOWN,x,y); (void)LDControls_EditorEvent(&e);
    ui->saveDialog.active=true;strcpy(ui->saveDialog.buffer,"preserved-control-draft");
    e=button(SDL_MOUSEBUTTONUP,x,y); (void)LDControls_EditorEvent(&e);
    TEST_ASSERT(ui->saveDialog.active && !strcmp(ui->saveDialog.buffer,"preserved-control-draft"));
    char* after=Layout_SaveToString(&Global_Get()->layout); TEST_ASSERT(after && !strcmp(before,after));
    Layout_FreeString(before);Layout_FreeString(after);
    ld_test_shutdown_runtime(); return true;
}
static bool test_semantic_rows_and_disabled(void) {
    KitUiSurface surface={0};SDL_Point activated;
    kit_ui_surface_begin(&surface,1);LDControls_Add(&surface,12,42,(SDL_Rect){0,0,80,20},NULL,true);
    TEST_ASSERT(kit_ui_surface_end(&surface).code==CORE_OK);
    SDL_Event e=button(SDL_MOUSEBUTTONDOWN,10,10);TEST_ASSERT(LDControls_Route(&surface,&e,&activated));
    kit_ui_surface_begin(&surface,1);LDControls_Add(&surface,12,99,(SDL_Rect){0,0,80,20},NULL,true);
    TEST_ASSERT(kit_ui_surface_end(&surface).code==CORE_OK);
    e=button(SDL_MOUSEBUTTONUP,10,10);TEST_ASSERT(LDControls_Route(&surface,&e,&activated));TEST_ASSERT(activated.x<0);
    kit_ui_surface_begin(&surface,1);LDControls_Add(&surface,12,99,(SDL_Rect){0,0,80,20},NULL,false);
    TEST_ASSERT(kit_ui_surface_end(&surface).code==CORE_OK);
    e=button(SDL_MOUSEBUTTONDOWN,10,10);TEST_ASSERT(LDControls_Route(&surface,&e,&activated));
    e.type=SDL_MOUSEBUTTONUP;TEST_ASSERT(LDControls_Route(&surface,&e,&activated));TEST_ASSERT(activated.x<0);
    e=button(SDL_MOUSEBUTTONDOWN,150,100);TEST_ASSERT(!LDControls_Route(&surface,&e,&activated));
    return true;
}
static bool test_menu_key_release_and_popup(void) {
    ld_test_init_runtime();LDViewControls_Close();
    LineDrawingHostMenuState* menu=calloc(1,sizeof(*menu));TEST_ASSERT(menu);
    LineDrawingHostMenu_Init(menu);menu->focus_region=LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
    SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=SDLK_RETURN;
    LineDrawingHostMenuCommand command;
    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command));
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_NONE);
    e.key.repeat=1;(void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_NONE);
    e.type=SDL_KEYUP;e.key.repeat=0;(void)LineDrawingHostMenu_HandleEvent(menu,NULL,&e,&command);
    TEST_ASSERT(command.type==LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR);free(menu);
    LDControls_CancelAll();LineDrawingEditorTopbar_UpdateControls();SDL_Rect rect;
    TEST_ASSERT(LDViewControls_Rect(LD_VIEW_MORE,&rect));
    e=button(SDL_MOUSEBUTTONDOWN,rect.x+2,rect.y+2);Input_Handle(NULL,&e);
    TEST_ASSERT(!LDViewControls_ActivePopup());
    e.type=SDL_MOUSEBUTTONUP;Input_Handle(NULL,&e);TEST_ASSERT(LDViewControls_ActivePopup()==LD_VIEW_MORE);
    Global_Get()->layout.sceneAuthoring.camera_count=0;
    TEST_ASSERT(!Global_Get()->cameraView.active);
    TEST_ASSERT(LDViewControls_Rect(LD_VIEW_SAVED,&rect));
    e=button(SDL_MOUSEBUTTONDOWN,rect.x+2,rect.y+2);Input_Handle(NULL,&e);
    e.type=SDL_MOUSEBUTTONUP;Input_Handle(NULL,&e);
    TEST_ASSERT(!Global_Get()->cameraView.active && LDViewControls_ActivePopup()==LD_VIEW_MORE);
    e=(SDL_Event){.type=SDL_KEYDOWN};e.key.keysym.sym=SDLK_ESCAPE;Input_Handle(NULL,&e);
    TEST_ASSERT(!LDViewControls_ActivePopup());
    LDViewControls_Close();LDControls_CancelAll();ld_test_shutdown_runtime();return true;
}
bool shared_controls_run_tests(void) {
    const TestCase cases[]={
        {"LauncherReleaseOutsideAndFocusCancel",test_menu_release_and_cancel},
        {"MenuKeyReleaseAndPopupDisabledOwner",test_menu_key_release_and_popup},
        {"ShellReleaseKeyboardAndModalDraft",test_shell_release_keyboard_modal},
        {"SemanticReorderDisabledAndCanvasPassThrough",test_semantic_rows_and_disabled}
    };
    return run_test_cases("SharedControls",cases,sizeof(cases)/sizeof(cases[0]));
}
