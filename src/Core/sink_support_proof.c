#include "Core/sink_support_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_json.h"
#include "Layout/layout_motion.h"
#include "Layout/layout_constraints.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_folding_brace.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_shell.h"
#include "vk_renderer.h"
#include "core_time.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void observer(GlobalState* s,Vec3 eye,Vec3 target,float fov) {
    Vec3 d=Vec3_Sub(target,eye);s->cameraView.eye=eye;s->cameraView.orbit_target=target;
    s->cameraView.yaw=atan2f(d.y,d.x)*57.295779513f;s->cameraView.pitch=atan2f(d.z,hypotf(d.x,d.y))*57.295779513f;s->cameraView.fov=fov;
}
static bool shot(AppContext* app,const AppCallbacks* cb,const char* root,const char* name) {
    char p[4096];snprintf(p,sizeof(p),"%s/%s.bmp",root,name);Global_FlagHitboxesDirty();cb->handleUpdate(app);
    for(int i=0;i<4;++i)if(!App_RenderOnce(app,cb->handleRender))return false;
    return vk_renderer_request_capture(app->renderer,p)==VK_SUCCESS&&App_RenderOnce(app,cb->handleRender);
}
static bool button(int action) {UIPanel_LayoutMeasurementPane();SDL_Rect r;return UIPanel_MeasurementControlRect(action,&r)&&UIPanel_MeasurementClick(r.x+r.w/2,r.y+r.h/2);}
static const LayoutConstraint* rule(GlobalState* s) {
    for(size_t i=0;i<s->layout.objectStore.constraintCount;++i)if(!strcmp(s->layout.objectStore.constraints[i].id,"worktop_hinge"))return &s->layout.objectStore.constraints[i];return NULL;
}
bool SinkSupportProof_Run(AppContext* app,const AppCallbacks* cb,const char* scene,const char* root) {
    GlobalState* s=Global_Get();if(!Layout_LoadFromFile(&s->layout,scene))return false;Global_OnLayoutLoaded(scene);
    char* before=Layout_SaveToString(&s->layout);size_t initial=Editor_UndoCount(&s->editor),count=s->layout.objectStore.count;
    Object3D* baseline=malloc(count*sizeof(Object3D));if(!before||!baseline){free(before);free(baseline);return false;}memcpy(baseline,s->layout.objectStore.items,count*sizeof(Object3D));
    Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);bool ok=CameraView_Enter(s,false)&&UIPanel_BeginEntityMotion("obj3d_239");
    const LayoutConstraint* c=rule(s);if(!c||!c->folding_brace.enabled){free(before);free(baseline);return false;}
    double endpoint_error=0;size_t dense_poses=0;
    Layout_BeginGeometryGesture(&s->layout);
    for(int sample=0;sample<366&&ok;++sample) {
        const double near[]={.00001,.0001,.001,.01,.1};double value=sample<=360?sample*.25:near[sample-361];
        ok=Layout_SetTravelPosition(&s->layout,"worktop_hinge",value,Layout_GeometryHistory,NULL);c=rule(s);
        const Object3D* link[2]={NULL,NULL};Vec3 pins[3]={0};
        for(size_t j=0;j<count;++j){const Object3D* o=&s->layout.objectStore.items[j];
            for(int n=0;n<2;++n)if(!strcmp(o->coreMeta.object_id,c->folding_brace.parts[n]))link[n]=o;
            for(int n=0;n<3;++n)if(!strcmp(o->coreMeta.object_id,c->folding_brace.parts[n+4]))pins[n]=Vec3_Scale(o->transform.position,Layout_WorldScale(&s->layout));
            if(!Layout_MotionScopeContains(&s->layout.objectStore,c,o))ok=ok&&!memcmp(o,&baseline[j],sizeof(*o));
        }
        for(int n=0;n<2&&ok;++n){if(!link[n]){ok=false;break;}double length=c->folding_brace.length_m[n];
            Vec3 ends[2];for(int k=0;k<2;++k)ends[k]=Vec3_Scale(Layout_Transform3D_ApplyLocalPoint(link[n]->transform,(Vec3){(float)((k?1:-1)*length/2),0,0}),Layout_WorldScale(&s->layout));
            for(int k=0;k<2;++k){double error=Vec3_Distance(ends[k],pins[n+k]);endpoint_error=fmax(endpoint_error,error);ok=ok&&error<3e-6;}
            double error=fabs(Vec3_Distance(pins[n],pins[n+1])-length);endpoint_error=fmax(endpoint_error,error);ok=ok&&error<3e-6;
        }
        Vec3 separation=Vec3_Sub(pins[2],pins[0]),mid=Vec3_Scale(Vec3_Add(pins[0],pins[2]),.5f);
        ok=ok&&Vec3_Dot(Vec3_Sub(pins[1],mid),Vec3_Cross(c->axis,separation))>=-1e-7;
        ++dense_poses;
    }
    ok=ok&&Layout_SetTravelPosition(&s->layout,"worktop_hinge",90,Layout_GeometryHistory,NULL);
    Layout_EndGeometryGesture(&s->layout);
    const char* names[]={"stowed","quarter","half","three_quarter","deployed"};double values[]={90,67.5,45,22.5,0};
    for(size_t i=0;i<count;++i)if(strcmp(s->layout.objectStore.items[i].coreMeta.object_id,c->a.entity_id)&&!Layout_MotionScopeContains(&s->layout.objectStore,c,&s->layout.objectStore.items[i]))ok=ok&&Layout_SetEntityHidden(&s->layout.objectStore,s->layout.objectStore.items[i].coreMeta.object_id,true);
    observer(s,(Vec3){-1.0f,1.55f,.75f},(Vec3){.55f,1.20f,.72f},35);
    Vec3 eye=s->cameraView.eye;float yaw=s->cameraView.yaw,pitch=s->cameraView.pitch;bool camera=true,controls=true;
    for(int i=0;i<5&&ok;++i) {
        if(!i) ok=button(MEASURE_TRAVEL_TO_MAX);
        else if(i==4)ok=button(MEASURE_TRAVEL_TO_MIN);
        else {
            UIPanel_LayoutMeasurementPane();SDL_Rect r;ok=UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&r);
            if(ok){SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+6+(int)lround(values[i]/90*(r.w-12));e.button.y=r.y+r.h/2;cb->handleInput(app,&e);e.type=SDL_MOUSEBUTTONUP;cb->handleInput(app,&e);cb->handleUpdate(app);}
        }
        c=rule(s);controls=controls&&fabs(c->target-values[i])<2;ok=ok&&controls&&Layout_FoldingBracesValidate(&s->layout,true);
        camera=camera&&s->cameraView.active&&Vec3_Distance(eye,s->cameraView.eye)<1e-7&&s->cameraView.yaw==yaw&&s->cameraView.pitch==pitch;
        char name[64];snprintf(name,sizeof(name),"leaf_%s",names[i]);ok=ok&&shot(app,cb,root,name);
    }
    ok=ok&&Layout_SetTravelPosition(&s->layout,"worktop_hinge",90,Layout_GeometryHistory,NULL)&&UIPanel_BeginEntityMotion("obj3d_239")&&Layout_SetEntityHidden(&s->layout.objectStore,"obj3d_239",true);
    ok=ok&&shot(app,cb,root,"support_only_stowed")&&Layout_SetEntityHidden(&s->layout.objectStore,"obj3d_239",false);
    double worst=0;size_t frames=0;
    Layout_BeginGeometryGesture(&s->layout);
    for(int i=0;i<=40&&ok;++i) {CoreTimeNs start=core_time_now_ns();ok=Layout_SetTravelPosition(&s->layout,"worktop_hinge",90.0*i/40,Layout_GeometryHistory,NULL);cb->handleUpdate(app);ok=ok&&App_RenderOnce(app,cb->handleRender);double ms=core_time_diff_ns(core_time_now_ns(),start)*1e-6;if(ms>worst)worst=ms;++frames;}
    Layout_EndGeometryGesture(&s->layout);
    ok=ok&&button(MEASURE_TRAVEL_RESET)&&fabs(rule(s)->target)<1e-6&&button(MEASURE_TRAVEL_TO_MAX);
    bool fixed=true;double error=0;c=rule(s);
    for(size_t i=0;i<count&&ok;++i){const Object3D* o=&s->layout.objectStore.items[i];if(!Layout_MotionScopeContains(&s->layout.objectStore,c,o)){fixed=fixed&&!memcmp(o,&baseline[i],sizeof(*o));continue;}
        Vec3 a[8],b[8];bool mesh=o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE;ok=ok&&(mesh?Layout_Object3D_ComputeMeshInstanceCorners(o,a):Layout_Object3D_ComputeRectPrismCorners(o,a));ok=ok&&(mesh?Layout_Object3D_ComputeMeshInstanceCorners(&baseline[i],b):Layout_Object3D_ComputeRectPrismCorners(&baseline[i],b));for(int k=0;k<8;++k){double v=Vec3_Distance(a[k],b[k])*Layout_WorldScale(&s->layout);if(v>error)error=v;}}
    Layout_RestoreEntityVisibility(&s->layout.objectStore);
    const char* shells[]={"assembly_23","assembly_24","assembly_25","assembly_39"};
    for(int i=0;i<4;++i)ok=ok&&Layout_SetEntityHidden(&s->layout.objectStore,shells[i],true);
    for(size_t i=0;i<count;++i){const Object3D* o=&s->layout.objectStore.items[i];if(Layout_EntityIsSpatialGuide(&o->info)||o->info.reference||strstr(o->info.parent_id,"roof")||strstr(o->info.parent_id,"exterior"))ok=ok&&Layout_SetEntityHidden(&s->layout.objectStore,o->coreMeta.object_id,true);}
    ok=ok&&Layout_SetTravelPosition(&s->layout,"worktop_hinge",0,Layout_GeometryHistory,NULL)&&UIPanel_BeginEntityMotion("obj3d_239");observer(s,(Vec3){-3,4,2.8f},(Vec3){0,0,.8f},55);ok=ok&&shot(app,cb,root,"full_van_context");Layout_RestoreEntityVisibility(&s->layout.objectStore);
    while(Editor_UndoCount(&s->editor)>initial&&ok)ok=Editor_Undo(&s->editor,&s->layout);
    char* after=Layout_SaveToString(&s->layout);bool undo=before&&after&&!strcmp(before,after);ok=ok&&camera&&fixed&&error<2e-6&&undo;
    char path[4096];if(!undo){snprintf(path,sizeof(path),"%s/undo_before.json",root);FILE* diagnostic=fopen(path,"wx");if(diagnostic){fputs(before,diagnostic);fclose(diagnostic);}snprintf(path,sizeof(path),"%s/undo_after.json",root);diagnostic=fopen(path,"wx");if(diagnostic){fputs(after,diagnostic);fclose(diagnostic);}}snprintf(path,sizeof(path),"%s/support.json",root);FILE* f=fopen(path,"wx");if(f){fprintf(f,"{\"passed\":%s,\"native_five_pose_slider_min_max_reset\":%s,\"free_camera_retained\":%s,\"fixed_geometry_unchanged\":%s,\"return_to_stowed_corner_error_m\":%.9g,\"undo_exact\":%s,\"dense_poses\":%zu,\"endpoint_length_error_m\":%.9g,\"frames\":%zu,\"worst_motion_render_ms\":%.3f}\n",ok?"true":"false",controls?"true":"false",camera?"true":"false",fixed?"true":"false",error,undo?"true":"false",dense_poses,endpoint_error,frames,worst);fclose(f);}else ok=false;
    free(before);free(after);free(baseline);return ok;
}
