#include "Core/sink_support_v2_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_json.h"
#include "Layout/layout_motion.h"
#include "Layout/layout_constraints.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_folding_brace.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_shell.h"
#include "UI/topbar/visibility_guide_groups.h"
#include "vk_renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const LayoutConstraint* rule(const Layout* l) {
    for(size_t i=0;i<l->objectStore.constraintCount;++i)if(!strcmp(l->objectStore.constraints[i].id,"worktop_hinge"))return &l->objectStore.constraints[i];return NULL;
}
static const Object3D* entity(const Layout* l,const char* id) {
    for(size_t i=0;i<l->objectStore.count;++i)if(!strcmp(l->objectStore.items[i].coreMeta.object_id,id))return &l->objectStore.items[i];return NULL;
}
static bool button(int action){UIPanel_LayoutMeasurementPane();SDL_Rect r;return UIPanel_MeasurementControlRect(action,&r)&&UIPanel_MeasurementClick(r.x+r.w/2,r.y+r.h/2);}
static void observer(GlobalState* s,Vec3 eye,Vec3 target,float fov){Vec3 d=Vec3_Sub(target,eye);s->cameraView.eye=eye;s->cameraView.orbit_target=target;s->cameraView.yaw=atan2f(d.y,d.x)*57.295779513f;s->cameraView.pitch=atan2f(d.z,hypotf(d.x,d.y))*57.295779513f;s->cameraView.fov=fov;}
static bool shot(AppContext* app,const AppCallbacks* cb,const char* root,const char* name){char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,name);Global_FlagHitboxesDirty();cb->handleUpdate(app);for(int i=0;i<4;++i)if(!App_RenderOnce(app,cb->handleRender))return false;return vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS&&App_RenderOnce(app,cb->handleRender);}
static bool corners(FILE* f,const Layout* l,const Object3D* o){Vec3 p[8];bool ok=o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE?Layout_Object3D_ComputeMeshInstanceCorners(o,p):Layout_Object3D_ComputeRectPrismCorners(o,p);if(!ok)return false;fprintf(f,"\"%s\":[",o->coreMeta.object_id);for(int k=0;k<8;++k){Vec3 v=Vec3_Scale(p[k],Layout_WorldScale(l));fprintf(f,"%s[%.9g,%.9g,%.9g]",k?",":"",v.x,v.y,v.z);}fputc(']',f);return true;}
static bool atomic_denial(GlobalState* s,bool engage){char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);bool accepted=engage?Layout_FoldingBraceSetLock(&s->layout,"worktop_hinge",true,Layout_GeometryHistory,NULL):Layout_SetTravelPosition(&s->layout,"worktop_hinge",45,Layout_GeometryHistory,NULL);char* after=Layout_SaveToString(&s->layout);bool ok=!accepted&&before&&after&&!strcmp(before,after)&&history==Editor_UndoCount(&s->editor)&&strstr(s->layout.geometryMessage,engage?"Deploy":"Release");free(before);free(after);return ok;}
bool SinkSupportV2Proof_Run(AppContext* app,const AppCallbacks* cb,const char* scene,const char* root){
    GlobalState* s=Global_Get();if(!Layout_LoadFromFile(&s->layout,scene))return false;Global_OnLayoutLoaded(scene);const LayoutConstraint* c=rule(&s->layout);if(!c||!c->folding_brace.manual_lock||!c->folding_brace.lock_engaged)return false;
    char* before=Layout_SaveToString(&s->layout);size_t initial=Editor_UndoCount(&s->editor),count=s->layout.objectStore.count;Object3D* baseline=malloc(count*sizeof(*baseline));if(!before||!baseline){free(before);free(baseline);return false;}memcpy(baseline,s->layout.objectStore.items,count*sizeof(*baseline));
    Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);bool ok=CameraView_Enter(s,false)&&UIPanel_BeginEntityMotion("obj3d_239");const char* stage="locked denial";bool denials=ok&&atomic_denial(s,false);ok=ok&&denials;
    char path[4096];snprintf(path,sizeof(path),"%s/context_corners.json",root);FILE* context=fopen(path,"wx");if(!context)ok=false;else{fputc('{',context);bool comma=false;for(size_t i=0;i<count;++i){const Object3D* o=&baseline[i];if(o->isDeleted||LDVisibilityGuide_Category(&s->layout.objectStore,o)||o->kind==OBJECT3D_KIND_PLANE)continue;if(comma)fputc(',',context);ok=ok&&corners(context,&s->layout,o);comma=true;}fputs("}\n",context);ok=ok&&!fclose(context);}
    /* Side section keeps the real cabinet end, countertop and mounting correction
     * visible; hides unrelated van shells rather than hiding cabinet penetration. */
    for(size_t i=0;i<count;++i){const Object3D* o=&baseline[i];bool keep=Layout_MotionScopeContains(&s->layout.objectStore,c,o)||!strcmp(o->coreMeta.object_id,c->a.entity_id)||!strncmp(o->coreMeta.object_id,"kitchen_base",12)||!strcmp(o->coreMeta.object_id,"obj3d_243");for(int n=790;n<=794;++n)if(o->objectId==(uint32_t)n)keep=true;ok=ok&&Layout_SetEntityHidden(&s->layout.objectStore,o->coreMeta.object_id,!keep);}
    observer(s,(Vec3){-.70f,1.17f,.68f},(Vec3){.55f,1.17f,.68f},48);Vec3 eye=s->cameraView.eye;float yaw=s->cameraView.yaw,pitch=s->cameraView.pitch;
    stage="lock UI";ok=ok&&shot(app,cb,root,"deployed_locked_side")&&button(MEASURE_TRAVEL_TO_MAX)&&fabs(rule(&s->layout)->target)<1e-6&&button(MEASURE_SUPPORT_RELEASE)&&!rule(&s->layout)->folding_brace.lock_engaged&&shot(app,cb,root,"just_released_side");
    stage="dense reachability";snprintf(path,sizeof(path),"%s/dense_corners.jsonl",root);FILE* dense=fopen(path,"wx");if(!dense)ok=false;double endpoint=0;size_t poses=0;bool fixed=true,branch=true;
    Layout_BeginGeometryGesture(&s->layout);
    for(int sample=0;sample<922&&ok;++sample){const double critical=89.54453069851492;const double near[]={.00001,.0001,.001,.01,.05,89.95,89.99,89.999,89.9999,89.99999,critical-.1,critical-.01,critical-.001,critical-.0001,critical-.00001,critical,critical+.00001,critical+.0001,critical+.001,critical+.01,critical+.1};double value=sample<=900?sample*.1:near[sample-901];ok=Layout_SetTravelPosition(&s->layout,"worktop_hinge",value,Layout_GeometryHistory,NULL);c=rule(&s->layout);if(!ok)break;
        Vec3 pins[3];for(int n=0;n<3;++n)pins[n]=Vec3_Sub(Vec3_Scale(entity(&s->layout,c->folding_brace.parts[n+4])->transform.position,Layout_WorldScale(&s->layout)),Vec3_Scale(c->axis,c->folding_brace.pin_lateral_m[n]));
        for(int n=0;n<2;++n){const Object3D* link=entity(&s->layout,c->folding_brace.parts[n]);double length=c->folding_brace.length_m[n];for(int k=0;k<2;++k){Vec3 actual=Vec3_Scale(Layout_Transform3D_ApplyLocalPoint(link->transform,(Vec3){(k?1:-1)*length/2,0,0}),Layout_WorldScale(&s->layout));Vec3 expected=Vec3_Add(pins[n+k],Vec3_Scale(c->axis,c->folding_brace.link_lateral_m[n]));endpoint=fmax(endpoint,Vec3_Distance(actual,expected));}endpoint=fmax(endpoint,fabs(Vec3_Distance(pins[n],pins[n+1])-length));}
        Vec3 delta=Vec3_Sub(pins[2],pins[0]);branch=branch&&c->folding_brace.branch*Vec3_Dot(Vec3_Sub(pins[1],pins[0]),Vec3_Cross(c->axis,delta))>=-1e-7;ok=ok&&endpoint<3e-6&&branch&&Layout_FoldingBracesValidate(&s->layout,true);
        fprintf(dense,"{\"angle\":%.9g,\"items\":{",value);bool comma=false;for(size_t j=0;j<count;++j){const Object3D* o=&s->layout.objectStore.items[j];if(!Layout_MotionScopeContains(&s->layout.objectStore,c,o)){fixed=fixed&&!memcmp(o,&baseline[j],sizeof(*o));continue;}if(comma)fputc(',',dense);ok=ok&&corners(dense,&s->layout,o);comma=true;}fputs("}}\n",dense);++poses;
    }
    Layout_EndGeometryGesture(&s->layout);if(dense)ok=ok&&!fclose(dense);ok=ok&&fixed;
    stage="folded lock denial";ok=ok&&Layout_SetTravelPosition(&s->layout,"worktop_hinge",90,Layout_GeometryHistory,NULL)&&atomic_denial(s,true);denials=denials&&ok;
    stage="native slider";const double angles[]={90,67.5,45,22.5,0};const char* names[]={"stowed_side","quarter_side","half_side","three_quarter_side","deployed_released_side"};bool camera=true;
    for(int i=0;i<5&&ok;++i){if(i==0)ok=button(MEASURE_TRAVEL_TO_MAX);else if(i==4)ok=button(MEASURE_TRAVEL_TO_MIN);else{UIPanel_LayoutMeasurementPane();SDL_Rect r;ok=UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&r);if(ok){SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+6+(int)lround(angles[i]/90*(r.w-12));e.button.y=r.y+r.h/2;cb->handleInput(app,&e);e.type=SDL_MOUSEBUTTONUP;cb->handleInput(app,&e);cb->handleUpdate(app);}}
        ok=ok&&fabs(rule(&s->layout)->target-angles[i])<2;camera=camera&&s->cameraView.active&&Vec3_Distance(eye,s->cameraView.eye)<1e-7&&s->cameraView.yaw==yaw&&s->cameraView.pitch==pitch;ok=ok&&shot(app,cb,root,names[i]);}
    stage="reset engage release";ok=ok&&button(MEASURE_TRAVEL_RESET)&&fabs(rule(&s->layout)->target)<1e-6&&!rule(&s->layout)->folding_brace.lock_engaged&&button(MEASURE_SUPPORT_ENGAGE)&&rule(&s->layout)->folding_brace.lock_engaged&&atomic_denial(s,false)&&button(MEASURE_SUPPORT_RELEASE)&&button(MEASURE_TRAVEL_TO_MAX);
    ok=ok&&Layout_SetEntityHidden(&s->layout.objectStore,"obj3d_239",true)&&shot(app,cb,root,"stowed_leaf_hidden_side");observer(s,(Vec3){.45f,2.45f,.73f},(Vec3){.45f,1.10f,.73f},48);ok=ok&&shot(app,cb,root,"stowed_leaf_hidden_front")&&Layout_SetEntityHidden(&s->layout.objectStore,"obj3d_239",false)&&shot(app,cb,root,"stowed_front");
    ok=ok&&button(MEASURE_TRAVEL_TO_MIN)&&button(MEASURE_SUPPORT_ENGAGE)&&shot(app,cb,root,"deployed_locked_front");
    stage="undo exact";Layout_RestoreEntityVisibility(&s->layout.objectStore);while(Editor_UndoCount(&s->editor)>initial&&ok)ok=Editor_Undo(&s->editor,&s->layout);char* after=Layout_SaveToString(&s->layout);bool undo=before&&after&&!strcmp(before,after);ok=ok&&undo&&camera;
    snprintf(path,sizeof(path),"%s/support_v2.json",root);FILE* f=fopen(path,"wx");if(f){fprintf(f,"{\"passed\":%s,\"stage\":\"%s\",\"dense_poses\":%zu,\"endpoint_length_error_m\":%.9g,\"fixed_geometry_unchanged\":%s,\"upward_branch_valid\":%s,\"manual_ui_and_atomic_denials\":%s,\"free_camera_retained\":%s,\"undo_exact\":%s}\n",ok?"true":"false",stage,poses,endpoint,fixed?"true":"false",branch?"true":"false",denials?"true":"false",camera?"true":"false",undo?"true":"false");fclose(f);}else ok=false;
    free(before);free(after);free(baseline);return ok;
}
