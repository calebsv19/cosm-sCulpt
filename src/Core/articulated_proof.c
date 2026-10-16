#include "Core/articulated_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout_json.h"
#include "Layout/layout_motion.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_shell.h"
#include "UI/topbar/visibility_tree.h"
#include "core_time.h"
#include "vk_renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool shot(AppContext* app,const AppCallbacks* cb,const char* root,const char* name) {
    char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,name);
    Global_FlagHitboxesDirty();cb->handleUpdate(app);
    for(int i=0;i<4;++i)if(!App_RenderOnce(app,cb->handleRender))return false;
    return vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS && App_RenderOnce(app,cb->handleRender);
}
static void observer(GlobalState* s,Vec3 eye,Vec3 target) {
    Vec3 d=Vec3_Sub(target,eye);s->cameraView.eye=eye;s->cameraView.orbit_target=target;
    s->cameraView.yaw=atan2f(d.y,d.x)*57.295779513f;s->cameraView.pitch=atan2f(d.z,hypotf(d.x,d.y))*57.295779513f;s->cameraView.fov=55;
}
static bool isolate_review(LayoutObjectStore* store,const char* id) {
    if(!Layout_IsolateEntity(store,id))return false;
    /* Only can deliberately reveal authored-hidden geometry. This review keeps
     * the replaced legacy placeholders hidden while exposing the new parts. */
    for(size_t i=0;i<store->count;++i)if(!store->items[i].coreMeta.flags.visible &&
        !Layout_SetEntityHidden(store,store->items[i].coreMeta.object_id,true))return false;
    return true;
}
static bool button(int action) {
    UIPanel_LayoutMeasurementPane();SDL_Rect rect;
    return UIPanel_MeasurementControlRect(action,&rect) && UIPanel_MeasurementClick(rect.x+rect.w/2,rect.y+rect.h/2);
}
static const LayoutConstraint* rule(const Layout* l,const char* id) {
    for(size_t i=0;i<l->objectStore.constraintCount;++i)if(!strcmp(l->objectStore.constraints[i].id,id))return &l->objectStore.constraints[i];return NULL;
}
bool ArticulatedProof_Run(AppContext* app,const AppCallbacks* cb,const char* scene,const char* root) {
    GlobalState* s=Global_Get();if(!Layout_LoadFromFile(&s->layout,scene))return false;Global_OnLayoutLoaded(scene);
    size_t count=s->layout.objectStore.count;Object3D* baseline=malloc(count*sizeof(Object3D));if(!baseline)return false;memcpy(baseline,s->layout.objectStore.items,count*sizeof(Object3D));
    size_t initial_undo=Editor_UndoCount(&s->editor);
    char* before=Layout_SaveToString(&s->layout);bool ok=before!=NULL;size_t pick_hits=0;double worst_ms=0;size_t frames=0;
    Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);ok=ok&&CameraView_Enter(s,false);
    const char* ids[]={"af1_dresser_drawer","af1_sink_door"};
    for(int mechanism=0;mechanism<2 && ok;++mechanism) {
        const LayoutConstraint* c=rule(&s->layout,ids[mechanism]);if(!c){ok=false;break;}
        char moving[64];snprintf(moving,64,"%s",c->motion_assembly);
        ok=isolate_review(&s->layout.objectStore,mechanism?"kitchen_base_unit":"storage_driver_unit") && UIPanel_BeginEntityMotion(moving);
        observer(s,mechanism?(Vec3){-1.25,.18,1.1f}:(Vec3){.8f,-.78f,.65f},mechanism?(Vec3){.3f,.8f,.45f}:(Vec3){-.3f,-.62f,.14f});
        char name[64];snprintf(name,sizeof(name),"%s_closed",mechanism?"sink_door":"dresser");ok=ok&&shot(app,cb,root,name);
        UIPanel_LayoutMeasurementPane();SDL_Rect r;
        ok=ok&&UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&r);
        if(ok) {
            SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+r.w/2;e.button.y=r.y+r.h/2;cb->handleInput(app,&e);
            e.type=SDL_MOUSEBUTTONUP;cb->handleInput(app,&e);cb->handleUpdate(app);
            c=rule(&s->layout,ids[mechanism]);ok=fabs(c->target-(mechanism?45:.12))<(mechanism?2:.006);
        }
        snprintf(name,sizeof(name),"%s_half",mechanism?"sink_door":"dresser");ok=ok&&shot(app,cb,root,name);
        ok=ok&&button(MEASURE_TRAVEL_TO_MAX);c=rule(&s->layout,ids[mechanism]);ok=ok&&fabs(c->target-(mechanism?90:.24))<1e-6;
        snprintf(name,sizeof(name),"%s_open",mechanism?"sink_door":"dresser");ok=ok&&shot(app,cb,root,name);
        SpaceViewContext view=SpaceAdapter_BuildViewContext(s);
        for(int y=(int)s->cameraView.viewport[1]+8;y<s->cameraView.viewport[1]+s->cameraView.viewport[3];y+=24)
            for(int x=(int)s->cameraView.viewport[0]+8;x<s->cameraView.viewport[0]+s->cameraView.viewport[2];x+=24) {
                uint32_t id=0;if(Layout_SolidPreviewPick(&s->layout,&view,&s->grid,x,y,&id)&&id){const Object3D* o=Layout_ObjectStore_FindConst(&s->layout.objectStore,id);if(o&&o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE&&Layout_ObjectShown(&s->layout.objectStore,o))++pick_hits;}
            }
        if(!mechanism) {
            /* Inspect the box removed using visibility only, without detaching or
             * moving membership. Both runner halves remain at operating pose. */
            for(size_t i=0;i<s->layout.objectStore.count;++i) {
                Object3D* o=&s->layout.objectStore.items[i];
                if(Layout_MotionScopeContains(&s->layout.objectStore,c,o)&&!strstr(o->info.label,"runner"))ok=ok&&Layout_SetEntityHidden(&s->layout.objectStore,o->coreMeta.object_id,true);
            }
            ok=ok&&shot(app,cb,root,"dresser_box_removed_tracks");
            Layout_RestoreEntityVisibility(&s->layout.objectStore);ok=ok&&isolate_review(&s->layout.objectStore,"storage_driver_unit");
        }
        /* Native slider pose updates include rendering; report actual timings. */
        for(int i=0;i<21 && ok;++i) {
            double value=(mechanism?90:.24)*i/20.0;CoreTimeNs start=core_time_now_ns();
            ok=Layout_SetTravelPosition(&s->layout,ids[mechanism],value,NULL,NULL);cb->handleUpdate(app);ok=ok&&App_RenderOnce(app,cb->handleRender);
            double ms=core_time_diff_ns(core_time_now_ns(),start)*1e-6;if(ms>worst_ms)worst_ms=ms;++frames;
        }
        ok=ok&&UIPanel_BeginEntityMotion(moving)&&button(MEASURE_TRAVEL_RESET);c=rule(&s->layout,ids[mechanism]);ok=ok&&fabs(c->target)<1e-6;
        Layout_RestoreEntityVisibility(&s->layout.objectStore);
    }
    bool precise=true,fixed_unchanged=true;double reset_error=0;
    for(size_t i=0;i<count;++i) {
        const Object3D* current=&s->layout.objectStore.items[i];bool moving=false;
        for(int m=0;m<2;++m)moving=moving||Layout_MotionScopeContains(&s->layout.objectStore,rule(&s->layout,ids[m]),current);
        if(!moving){fixed_unchanged=fixed_unchanged&&!memcmp(current,&baseline[i],sizeof(*current));continue;}
        Vec3 a[8],b[8];bool mesh=current->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE;
        precise=precise&&(mesh?Layout_Object3D_ComputeMeshInstanceCorners(current,a):Layout_Object3D_ComputeRectPrismCorners(current,a));
        precise=precise&&(mesh?Layout_Object3D_ComputeMeshInstanceCorners(&baseline[i],b):Layout_Object3D_ComputeRectPrismCorners(&baseline[i],b));
        for(int k=0;k<8;++k){double error=Vec3_Distance(a[k],b[k])*Layout_WorldScale(&s->layout);if(error>reset_error)reset_error=error;precise=precise&&error<=1e-6;}
    }
    bool reset_passed=precise&&fixed_unchanged;
    while(Editor_UndoCount(&s->editor)>initial_undo&&ok)ok=Editor_Undo(&s->editor,&s->layout);
    char* after=Layout_SaveToString(&s->layout);bool restored=before&&after&&!strcmp(before,after);
    if(!restored) {
        char diagnostic[4096];snprintf(diagnostic,sizeof(diagnostic),"%s/undo_before.json",root);FILE* d=fopen(diagnostic,"w");if(d){fputs(before?before:"null",d);fclose(d);}
        snprintf(diagnostic,sizeof(diagnostic),"%s/undo_after.json",root);d=fopen(diagnostic,"w");if(d){fputs(after?after:"null",d);fclose(d);}
    }
    ok=ok&&reset_passed&&restored&&pick_hits>0;
    free(baseline);
    free(before);free(after);char path[4096];snprintf(path,sizeof(path),"%s/articulated.json",root);FILE* f=fopen(path,"wx");if(!f)return false;
    fprintf(f,"{\"passed\":%s,\"native_slider_min_max_reset\":%s,\"reset_within_one_micrometre\":%s,\"reset_max_corner_error_m\":%.9g,\"fixed_geometry_unchanged\":%s,\"undo_exact_document_restored\":%s,\"visible_mesh_pick_hits\":%zu,\"motion_render_frames\":%zu,\"motion_render_worst_ms\":%.3f}\n",ok?"true":"false",ok?"true":"false",reset_passed?"true":"false",reset_error,fixed_unchanged?"true":"false",restored?"true":"false",pick_hits,frames,worst_ms);return !fclose(f)&&ok;
}
