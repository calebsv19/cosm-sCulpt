#include "Core/scene_collections_proof.h"
#include "Core/collection_inspection_proof.h"
#include "Core/construction_reference_proof.h"
#include "Core/visibility_reliability_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_json.h"
#include "Layout/layout_saved_views.h"
#include "Layout/layout_coverings.h"
#include "Editor/editor.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/panel/ui_panel_visibility.h"
#include "core_time.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool click(AppContext* app,const AppCallbacks* cb,int action) {
    LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;
    if(!LDViewControls_Rect(action,&r))return false;
    SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+2;e.button.y=r.y+2;cb->handleInput(app,&e);return true;
}
static bool capture(AppContext* app,const AppCallbacks* cb,const char* root,const char* label) {
    char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,label);cb->handleUpdate(app);
    if(!App_RenderOnce(app,cb->handleRender))return false;
    return vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS&&App_RenderOnce(app,cb->handleRender);
}
static bool performance(AppContext* app,const AppCallbacks* cb,const char* root) {
    GlobalState* g=Global_Get();LayoutObjectStore* store=&g->layout.objectStore;
    char* before=Layout_SaveToString(&g->layout);size_t undo=Editor_UndoCount(&g->editor);
    char path[4096];snprintf(path,sizeof(path),"%s/performance.json",root);FILE* f=fopen(path,"wx");if(!f){free(before);return false;}
    const char* cases[]={"closed_static","closed_orbit","layers_static","layers_orbit","drawers_toggle"};
    bool ok=before&&CameraView_Enter(g,false)&&CameraView_Frame(g,false);Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    Layout_ShowAllViews(store);LDViewControls_Close();fprintf(f,"{\"cases\":[");
    for(int c=0;c<5;++c) {
        if(c==2)ok=ok&&click(app,cb,LD_VIEW_VISIBILITY)&&click(app,cb,UI_VIS_LAYERS);
        double input_ms=0,update_ms=0,render_ms=0;
        int frames=getenv("LINE_DRAWING_PERFORMANCE_FRAMES")?atoi(getenv("LINE_DRAWING_PERFORMANCE_FRAMES")):4;if(frames<4||frames>60)frames=4;
        for(int n=0;n<frames;++n) {
            CoreTimeNs start=core_time_now_ns();
            if(c==1||c==3){SDL_Keymod previous_mod=SDL_GetModState();SDL_SetModState(KMOD_ALT);SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=450;e.button.y=350;cb->handleInput(app,&e);e.type=SDL_MOUSEMOTION;e.motion.x=460;e.motion.y=354;e.motion.xrel=10;e.motion.yrel=4;e.motion.state=SDL_BUTTON_LMASK;cb->handleInput(app,&e);e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;cb->handleInput(app,&e);SDL_SetModState(previous_mod);}
            if(c==4){(void)Layout_SetEntityHidden(store,"@coverings.drawers",n%2==0);Global_FlagHitboxesDirty();}
            CoreTimeNs after_input=core_time_now_ns();cb->handleUpdate(app);CoreTimeNs after_update=core_time_now_ns();ok=App_RenderOnce(app,cb->handleRender)&&ok;CoreTimeNs after_render=core_time_now_ns();
            input_ms+=core_time_diff_ns(after_input,start)*1e-6/frames;update_ms+=core_time_diff_ns(after_update,after_input)*1e-6/frames;render_ms+=core_time_diff_ns(after_render,after_update)*1e-6/frames;
        }
        fprintf(f,"%s{\"name\":\"%s\",\"input_ms\":%.3f,\"update_ms\":%.3f,\"render_ms\":%.3f}",c?",":"",cases[c],input_ms,update_ms,render_ms);fflush(f);
    }
    char* after=Layout_SaveToString(&g->layout);bool unchanged=before&&after&&!strcmp(before,after)&&undo==Editor_UndoCount(&g->editor);
    fprintf(f,"],\"passed\":%s,\"document_unchanged\":%s}\n",ok&&unchanged?"true":"false",unchanged?"true":"false");fclose(f);free(before);free(after);return ok&&unchanged;
}
bool SceneCollectionsProof_Run(AppContext* app,const AppCallbacks* cb,const char* path,const char* root) {
    GlobalState* g=Global_Get();if(!Layout_LoadFromFile(&g->layout,path))return false;Global_OnLayoutLoaded(path);
    if(getenv("LINE_DRAWING_CONSTRUCTION_REFERENCE_PROOF"))return ConstructionReferenceProof_Run(app,cb,root);
    if(getenv("LINE_DRAWING_COLLECTION_INSPECTION_PROOF"))return CollectionInspectionProof_Run(app,cb,root);
    if(getenv("LINE_DRAWING_PASS1_PROOF")&&!VisibilityReliabilityProof_Run(app,cb,root))return false;
    if(getenv("LINE_DRAWING_VISIBILITY_PERFORMANCE_PROOF"))return performance(app,cb,root);
    char* before=Layout_SaveToString(&g->layout);size_t undo=Editor_UndoCount(&g->editor);bool ok=before!=NULL;
    ok=ok&&CameraView_Enter(g,false)&&CameraView_Frame(g,false);Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    ok=ok&&Layout_ApplyWorkingView(&g->layout.objectStore,"working_interior",&g->authoringGuides)&&click(app,cb,LD_VIEW_VISIBILITY)&&capture(app,cb,root,"scene_groups");
    ok=ok&&click(app,cb,LD_VIEW_ROW_EXPAND+2)&&capture(app,cb,root,"cabinet_children");
    ok=ok&&click(app,cb,UI_VIS_LAYERS)&&capture(app,cb,root,"inspection_layers");
    ok=ok&&click(app,cb,UI_VIS_VIEWS)&&capture(app,cb,root,"working_views");
    ok=ok&&Layout_ApplyWorkingView(&g->layout.objectStore,"working_sink",&g->authoringGuides)&&capture(app,cb,root,"sink_only");
    double durations[12];for(int i=0;i<12;++i){CoreTimeNs start=core_time_now_ns();cb->handleUpdate(app);ok=ok&&App_RenderOnce(app,cb->handleRender);durations[i]=core_time_diff_ns(core_time_now_ns(),start)*1e-6;}
    char* after=Layout_SaveToString(&g->layout);bool unchanged=before&&after&&!strcmp(before,after)&&undo==Editor_UndoCount(&g->editor);ok=ok&&unchanged;
    char report[4096];snprintf(report,sizeof(report),"%s/scene_collections.json",root);FILE* f=fopen(report,"wx");
    if(!f)ok=false;else{double mean=0;for(int i=0;i<12;++i)mean+=durations[i]/12;
        fprintf(f,"{\"passed\":%s,\"collections\":%zu,\"working_views\":%zu,\"document_unchanged\":%s,\"mean_frame_ms\":%.3f,\"width\":%d,\"height\":%d}\n",ok?"true":"false",g->layout.objectStore.scene_collection_count,g->layout.objectStore.working_view_count,unchanged?"true":"false",mean,g->screenWidth,g->screenHeight);fclose(f);}
    free(before);free(after);return ok;
}
