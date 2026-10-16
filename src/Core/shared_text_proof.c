#include "Core/shared_text_proof.h"
#include "UI/shared_text.h"
#include "UI/ui_panel.h"
#include <string.h>
#include <stdio.h>

bool SharedTextProof_Stage(const char* mode,LineDrawingHostMenuState* menu) {
    if(!mode || strncmp(mode,"shared-text-",12))return true;
    SDL_Event event={.type=SDL_KEYDOWN};event.key.keysym.sym=SDLK_a;event.key.keysym.mod=KMOD_CTRL;
    if(!strcmp(mode,"shared-text-filter")) {
        menu->selected_section=LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS;
        menu->focus_region=LINE_DRAWING_HOST_MENU_FOCUS_FILTER;menu->filter_editing=true;
        SDL_StartTextInput();LineDrawingHostMenuCommand command;
        event.type=SDL_TEXTINPUT;strcpy(event.text.text,"café");
        (void)LineDrawingHostMenu_HandleEvent(menu,NULL,&event,&command);return !strcmp(menu->filter_query,"café");
    }
    UIPanel_BeginInputRootDialog();(void)UIPanel_SharedTextEvent(&event);
    (void)UIPanel_SharedTextInsert("/draft/café/very-long-root-component/retained-authoring-draft/very-long-root-component/scene-library/extra-long-library-component/extra-long-library-component/extra-long-library-component/visible-caret-at-the-end");
    if(!strcmp(mode,"shared-text-preedit")) {
        (void)UIPanel_SharedTextEvent(&event);
        event=(SDL_Event){.type=SDL_TEXTEDITING};strcpy(event.edit.text,"é");event.edit.length=1;
        (void)UIPanel_SharedTextEvent(&event);
    }
    return UIPanel_Get()->rootDialog.active;
}

bool SharedTextProof_Check(const char* mode,const LineDrawingHostMenuState* menu) {
    if(!mode || strncmp(mode,"shared-text-",12))return true;
    const KitUiTextPresentation* view=!strcmp(mode,"shared-text-filter")?&menu->filter_text.view:UIPanel_SharedTextPresentation();
    bool ok=view && view->options.active && view->count==1 &&
        view->caret.x>=view->options.viewport.x && view->caret.x+view->caret.width<=view->options.viewport.x+view->options.viewport.width+0.01f;
    if(ok && !strcmp(mode,"shared-text-scroll"))ok=view->scroll_x>0;
    if(ok && !strcmp(mode,"shared-text-preedit"))ok=view->composition_bytes>0 && strstr(UIPanel_Get()->rootDialog.buffer,"retained-authoring-draft");
    fprintf(stdout,"SHARED_TEXT mode=%s status=%s scroll_x=%.2f preedit_bytes=%zu caret_contained=%d\n",mode,ok?"pass":"fail",view?view->scroll_x:0,view?view->composition_bytes:0,ok);
    return ok;
}
