#include "Core/appearance_proof.h"
#include "Core/guide_groups_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_json.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/visibility_tree.h"
#include "core_time.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool click(AppContext* app,const AppCallbacks* c,int action){LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;if(!LDViewControls_Rect(action,&r))return false;SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+2;e.button.y=r.y+2;c->handleInput(app,&e);e.type=SDL_MOUSEBUTTONUP;c->handleInput(app,&e);return true;}
static bool capture(AppContext* app,const AppCallbacks* c,const char* root,const char* name){char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,name);return vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS&&App_RenderOnce(app,c->handleRender);}
static bool modes(AppContext* app,const AppCallbacks* c,const char* root,const char* name,FILE* report){
    GlobalState* s=Global_Get();Vec3 eye=s->cameraView.eye;float yaw=s->cameraView.yaw,pitch=s->cameraView.pitch;FreeViewCamera editor=s->freeViewCamera;bool ok=true;
    for(int mode=LD_VIEW_WIRE;mode<=LD_VIEW_MATERIAL;++mode){ok=ok&&click(app,c,LD_VIEW_APPEARANCE)&&click(app,c,mode);c->handleUpdate(app);CoreTimeNs first=core_time_now_ns();ok=ok&&App_RenderOnce(app,c->handleRender);double response_ms=core_time_diff_ns(core_time_now_ns(),first)*1e-6;SDL_Delay(180);
        CoreTimeNs start=core_time_now_ns();ok=ok&&App_RenderOnce(app,c->handleRender);double ms=core_time_diff_ns(core_time_now_ns(),start)*1e-6;
        LayoutMeshSolidPreviewFrameStats stats=Layout_MeshSolidPreviewReadStats();
        fprintf(report,"{\"view\":\"%s\",\"mode\":%d,\"response_ms\":%.3f,\"settled_frame_ms\":%.3f,\"raster_ms\":%.3f,\"upload_ms\":%.3f,\"raster_bytes\":%zu,\"triangles\":%zu,\"covered_pixels\":%zu}\n",name,mode-LD_VIEW_WIRE+1,response_ms,ms,stats.raster_ns*1e-6,stats.upload_ns*1e-6,stats.rasterBufferBytes,stats.submittedTriangles,stats.coveredPixels);
        char label[128];snprintf(label,sizeof(label),"%s_%s",name,mode==LD_VIEW_WIRE?"wire":mode==LD_VIEW_SOLID?"solid":"material");ok=ok&&capture(app,c,root,label);
        ok=ok&&Vec3_Distance(eye,s->cameraView.eye)==0&&s->cameraView.yaw==yaw&&s->cameraView.pitch==pitch&&!memcmp(&editor,&s->freeViewCamera,sizeof(editor));
    }
    return ok;
}
bool AppearanceProof_Run(AppContext* app,const AppCallbacks* c,const char* root){
    GlobalState* s=Global_Get();char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);bool inspection=s->inspectionView;s->inspectionView=false;LDViewControls_Close();
    char path[4096];snprintf(path,sizeof(path),"%s/appearance_frames.jsonl",root);FILE* f=fopen(path,"wx");if(!f){free(before);return false;}
    bool ok=CameraView_Enter(s,false)&&modes(app,c,root,"free_far",f);CameraView_Dolly(s,6);ok=ok&&modes(app,c,root,"free_near",f);
    if(s->layout.sceneAuthoring.camera_count){
        size_t selected=s->cameraView.selected;
        /* Choose an existing camera with coverage so blank views cannot stand in
         * for appearance proof. No authored pose or camera record is changed. */
        for(size_t i=0;i<s->layout.sceneAuthoring.camera_count;++i){
            s->cameraView.selected=i;ok=ok&&CameraView_Enter(s,true)&&App_RenderOnce(app,c->handleRender);
            if(Layout_MeshSolidPreviewReadStats().coveredPixels)break;
        }
        ok=ok&&modes(app,c,root,"saved",f);s->cameraView.selected=selected;
    }
    CameraView_Exit(s);s->spaceMode=SPACE_MODE_3D;s->freeViewCamera.enabled=true;
    ok=ok&&modes(app,c,root,"editor",f);
    ok=ok&&click(app,c,LD_VIEW_APPEARANCE)&&capture(app,c,root,"appearance_selector");LDViewControls_Close();
    ok=ok&&click(app,c,LD_VIEW_VISIBILITY)&&capture(app,c,root,"tree_collapsed");
    ok=ok&&click(app,c,LD_VIEW_ROW_EXPAND)&&capture(app,c,root,"tree_expanded");
    ok=ok&&click(app,c,LD_VIEW_SEARCH);SDL_Event e={.type=SDL_TEXTINPUT};snprintf(e.text.text,32,"roof");c->handleInput(app,&e);ok=ok&&capture(app,c,root,"tree_search");LDViewControls_Close();
    ok=ok&&CameraView_Enter(s,false);if(s->layout.objectStore.count)s->editor.selectedObject3DId=s->layout.objectStore.items[0].objectId;
    c->handleUpdate(app);ok=ok&&capture(app,c,root,"selected");s->editor.selectedObject3DId=0;
    if(s->layout.objectStore.count)s->editor.hoveredObject3DId=s->layout.objectStore.items[0].objectId;
    c->handleUpdate(app);ok=ok&&capture(app,c,root,"hovered");s->editor.hoveredObject3DId=0;
    SpaceViewContext v=SpaceAdapter_BuildViewContext(s);size_t hits=0;
    for(int y=(int)s->cameraView.viewport[1]+8;y<s->cameraView.viewport[1]+s->cameraView.viewport[3];y+=40)for(int x=(int)s->cameraView.viewport[0]+8;x<s->cameraView.viewport[0]+s->cameraView.viewport[2];x+=40){uint32_t id=0;if(Layout_SolidPreviewPick(&s->layout,&v,&s->grid,x,y,&id)&&id){const Object3D* o=Layout_ObjectStore_FindConst(&s->layout.objectStore,id);ok=ok&&o&&Layout_ObjectShown(&s->layout.objectStore,o);++hits;}}
    s->inspectionView=inspection;char* after=Layout_SaveToString(&s->layout);ok=ok&&before&&after&&!strcmp(before,after)&&history==Editor_UndoCount(&s->editor);free(before);free(after);fclose(f);
    snprintf(path,sizeof(path),"%s/appearance.json",root);f=fopen(path,"wx");if(!f)return false;fprintf(f,"{\"passed\":%s,\"same_pose_modes\":%s,\"pick_hits\":%zu,\"document_history_unchanged\":%s,\"legacy_quality\":%s}\n",ok?"true":"false",ok?"true":"false",hits,ok?"true":"false",getenv("LINE_DRAWING_PREVIEW_LEGACY_QUALITY")?"true":"false");bool written=!fclose(f);if(ok&&getenv("LINE_DRAWING_GUIDE_GROUP_PROOF"))ok=GuideGroupsProof_Run(app,c,root);return written&&ok;
}
