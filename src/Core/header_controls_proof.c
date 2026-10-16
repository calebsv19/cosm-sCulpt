#include "Core/header_controls_proof.h"
#include "Core/global_state.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/visibility_tree.h"
#include "Layout/layout_json.h"
#include "Layout/layout_saved_views.h"
#include "Editor/editor.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool click(AppContext* app,const AppCallbacks* c,int command) {
    LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;
    if(!LDViewControls_Rect(command,&r))return false;
    SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+r.w/2;e.button.y=r.y+r.h/2;
    c->handleInput(app,&e);e.type=SDL_MOUSEBUTTONUP;c->handleInput(app,&e);return true;
}
static bool capture(AppContext* app,const AppCallbacks* c,const char* root,const char* name) {
    char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,name);c->handleUpdate(app);
    return vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS && App_RenderOnce(app,c->handleRender);
}
static bool reveal_proof(AppContext* app,const AppCallbacks* c,const char* root) {
    GlobalState* s=Global_Get();LayoutObjectStore* store=&s->layout.objectStore;Object3D* roof=NULL;
    for(size_t i=0;i<store->count;++i)if(strstr(store->items[i].info.label,"PROVISIONAL crowned vehicle roof skin")){roof=&store->items[i];break;}
    if(!roof || roof->coreMeta.flags.visible || Layout_ObjectShown(store,roof))return false;
    char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);
    bool ok=capture(app,c,root,"reveal_before") && click(app,c,LD_VIEW_VISIBILITY) && click(app,c,LD_VIEW_SEARCH);
    SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,sizeof(text.text.text),"crowned vehicle roof skin");c->handleInput(app,&text);
    size_t count=LDViewControls_MatchingCount(),row=0;LDVisibilityRow item;while(LDVisibilityTree_Row(row,&item)&&strcmp(item.id,roof->coreMeta.object_id))++row;
    ok=ok && count>0 && row<20 && click(app,c,LD_VIEW_ROW+(int)row) && Layout_ObjectShown(store,roof) && capture(app,c,root,"reveal_shown_popup");
    LDViewControls_Close();ok=ok && capture(app,c,root,"reveal_shown");
    ok=ok && click(app,c,LD_VIEW_VISIBILITY) && click(app,c,LD_VIEW_RESTORE) && !Layout_ObjectShown(store,roof);LDViewControls_Close();
    ok=ok && capture(app,c,root,"reveal_restored");
    char* after=Layout_SaveToString(&s->layout);ok=ok && before && after && !strcmp(before,after) && history==Editor_UndoCount(&s->editor);free(before);free(after);
    return ok;
}
bool HeaderControlsProof_Run(AppContext* app,const AppCallbacks* c,const char* root) {
    GlobalState* s=Global_Get();char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);
    bool ok=click(app,c,LD_VIEW_CAMERA) && s->cameraView.active && capture(app,c,root,"header_camera");
    if(getenv("LINE_DRAWING_VISIBILITY_REVEAL_PROOF"))ok=ok && reveal_proof(app,c,root);
    ok=ok && click(app,c,LD_VIEW_VISIBILITY) && click(app,c,LD_VIEW_SEARCH);
    SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,sizeof(text.text.text),"roof");c->handleInput(app,&text);
    ok=ok && LDViewControls_MatchingCount()>0 && capture(app,c,root,"header_visibility");
    ok=ok && click(app,c,LD_VIEW_ROW) && capture(app,c,root,"header_hidden");
    ok=ok && click(app,c,LD_VIEW_RESTORE);LDViewControls_Close();
    ok=ok && click(app,c,LD_VIEW_MORE) && capture(app,c,root,"header_more");LDViewControls_Close();
    ok=ok && click(app,c,LD_VIEW_CAMERA) && !s->cameraView.active;
    char* after=Layout_SaveToString(&s->layout);ok=ok && before && after && !strcmp(before,after) && history==Editor_UndoCount(&s->editor);
    free(before);free(after);char path[4096];snprintf(path,sizeof(path),"%s/header_controls.json",root);FILE* f=fopen(path,"wx");
    if(!f)return false;fprintf(f,"{\"passed\":%s,\"width\":%d,\"height\":%d,\"document_and_history_unchanged\":%s}\n",ok?"true":"false",s->screenWidth,s->screenHeight,ok?"true":"false");
    return !fclose(f) && ok;
}
