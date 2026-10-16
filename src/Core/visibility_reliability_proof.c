#include "Core/visibility_reliability_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Editor/editor.h"
#include "Layout/layout_json.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_saved_views.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_guide_groups.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/panel/ui_panel_visibility.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool click(AppContext* app,const AppCallbacks* cb,int action) {
    LineDrawingEditorTopbar_UpdateControls();SDL_Rect box;
    if(!LDViewControls_Rect(action,&box))return false;
    SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;
    e.button.x=box.x+box.w/2;e.button.y=box.y+box.h/2;cb->handleInput(app,&e);return true;
}
bool VisibilityReliabilityProof_Run(AppContext* app,const AppCallbacks* cb,const char* root) {
    GlobalState* g=Global_Get();LayoutObjectStore* s=&g->layout.objectStore;
    char* before=Layout_SaveToString(&g->layout);size_t undo=Editor_UndoCount(&g->editor);
    bool ok=before&&Layout_FindSceneCollection(s,"@scene.driver")&&CameraView_Enter(g,false)&&CameraView_Frame(g,false);
    Layout_ShowAllViews(s);g->authoringGuides=false;Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    ok=ok&&Layout_SetEntityHidden(s,"@layer.guides",true)&&Layout_SetEntityHidden(s,"@coverings.drawers",true)&&
        Layout_SetEntityHidden(s,"@scene.driver",true)&&click(app,cb,LD_VIEW_VISIBILITY)&&click(app,cb,UI_VIS_SCENE);
    size_t row=SIZE_MAX;size_t count=UIPanel_VisibilityCount();LDVisibilityRow item;
    for(size_t i=0;i<count;++i)if(LDVisibilityTree_Row(i,&item)&&!strcmp(item.id,"@scene.driver")){row=i;break;}
    ok=ok&&row<32&&click(app,cb,400+(int)row)&&click(app,cb,UI_VIS_REVEAL);
    size_t visible=0,outside_hidden=0,guides_hidden=0;
    for(size_t i=0;i<s->count;++i) {
        const Object3D* o=&s->items[i];if(o->isDeleted)continue;
        if(Layout_CoveringInfoContains(&o->info,"@coverings.drawers")) {
            if(Layout_SceneCollectionContains(s,"@scene.driver",o->coreMeta.object_id)) {if(Layout_ObjectShown(s,o))++visible;else ok=false;}
            else {if(!Layout_ObjectShown(s,o))++outside_hidden;else ok=false;}
        }
        if(Layout_GuideCategory(s,o)){if(!Layout_ObjectShown(s,o))++guides_hidden;else ok=false;}
    }
    ok=ok&&visible>0&&outside_hidden>0&&guides_hidden>0;
    /* Let bounded background preparation populate the visible warm frame. */
    for(int i=0;i<24;++i){cb->handleUpdate(app);ok=App_RenderOnce(app,cb->handleRender)&&ok;SDL_Delay(10);}
    char path[4096];snprintf(path,sizeof(path),"%s/reveal_selected.bmp",root);cb->handleUpdate(app);
    ok=App_RenderOnce(app,cb->handleRender)&&ok;
    if(ok)ok=vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS&&App_RenderOnce(app,cb->handleRender);
    /* A later explicit hide supersedes the scoped Reveal. */
    ok=ok&&Layout_SetEntityHidden(s,"@scene.driver",true)&&Layout_EntityVisibilityState(s,"@scene.driver")==0;
    Layout_RestoreEntityVisibility(s);LDViewControls_Close();
    char* after=Layout_SaveToString(&g->layout);bool unchanged=before&&after&&!strcmp(before,after)&&undo==Editor_UndoCount(&g->editor);
    snprintf(path,sizeof(path),"%s/visibility_reliability.json",root);FILE* f=fopen(path,"wx");
    if(!f)ok=false;else {fprintf(f,"{\"passed\":%s,\"document_unchanged\":%s,\"revealed_drawer_parts\":%zu,\"outside_drawer_parts_hidden\":%zu,\"guides_hidden\":%zu}\n",ok&&unchanged?"true":"false",unchanged?"true":"false",visible,outside_hidden,guides_hidden);fclose(f);}
    free(before);free(after);return ok&&unchanged;
}
