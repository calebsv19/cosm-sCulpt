#include "Core/articulated_expansion_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_json.h"
#include "Layout/layout_motion.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_guide_groups.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_telescoping_stay.h"
#include "Layout/scene/layout_folding_brace.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_shell.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "vk_renderer.h"
#include "Layout/scene/layout_mesh_preview_cache.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool corners(FILE* f,const Layout* l,const Object3D* o){
    Vec3 p[8];bool ok=o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE?Layout_Object3D_ComputeMeshInstanceCorners(o,p):Layout_Object3D_ComputeRectPrismCorners(o,p);
    if(!ok)return false;fprintf(f,"\"%s\":[",o->coreMeta.object_id);
    for(int k=0;k<8;++k){Vec3 v=Vec3_Scale(p[k],Layout_WorldScale(l));fprintf(f,"%s[%.9g,%.9g,%.9g]",k?",":"",v.x,v.y,v.z);}fputc(']',f);return true;
}
static bool physical(const LayoutObjectStore* s,const Object3D* o){return !o->isDeleted&&o->coreMeta.flags.visible&&!Layout_GuideCategory(s,o)&&(o->kind==OBJECT3D_KIND_RECT_PRISM||o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE);}
static const Object3D* entity(const Layout* l,const char* id){for(size_t i=0;i<l->objectStore.count;++i)if(!strcmp(l->objectStore.items[i].coreMeta.object_id,id))return &l->objectStore.items[i];return NULL;}
static const LayoutConstraint* rule(const Layout* l,const char* id){for(size_t i=0;i<l->objectStore.constraintCount;++i)if(!strcmp(l->objectStore.constraints[i].id,id))return &l->objectStore.constraints[i];return NULL;}
static bool button(int action){UIPanel_LayoutMeasurementPane();SDL_Rect r;return UIPanel_MeasurementControlRect(action,&r)&&UIPanel_MeasurementClick(r.x+r.w/2,r.y+r.h/2);}
static bool shot(AppContext* app,const AppCallbacks* cb,const char* root,const char* name){char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,name);Global_FlagHitboxesDirty();cb->handleUpdate(app);for(int i=0;i<4;++i)if(!App_RenderOnce(app,cb->handleRender))return false;return vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS&&App_RenderOnce(app,cb->handleRender);}
static void observer(GlobalState* s,Vec3 eye,Vec3 target){Vec3 d=Vec3_Sub(target,eye);s->cameraView.eye=eye;s->cameraView.orbit_target=target;s->cameraView.yaw=atan2f(d.y,d.x)*57.295779513f;s->cameraView.pitch=atan2f(d.z,hypotf(d.x,d.y))*57.295779513f;s->cameraView.fov=48;}
static bool restore(GlobalState* s,const char* baseline){while(Editor_UndoCount(&s->editor))if(!Editor_Undo(&s->editor,&s->layout))return false;char* after=Layout_SaveToString(&s->layout);bool ok=after&&!strcmp(after,baseline);free(after);Editor_ClearHistory(&s->editor);return ok;}
static bool header_click(AppContext* app,const AppCallbacks* cb,int command) {
    LineDrawingEditorTopbar_UpdateControls();SDL_Rect rect;
    if(!LDViewControls_Rect(command,&rect))return false;
    SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=rect.x+rect.w/2;e.button.y=rect.y+rect.h/2;cb->handleInput(app,&e);return true;
}
static bool warm_cache(AppContext* app,const AppCallbacks* cb) {
    for(int i=0;i<4096;++i){cb->handleUpdate(app);if(!App_RenderOnce(app,cb->handleRender))return false;
        if(!Layout_MeshPreviewCacheRead().pending_jobs)return true;SDL_Delay(1);}
    return false;
}
bool ArticulatedExpansionProof_Run(AppContext* app,const AppCallbacks* cb,const char* scene,const char* root){
    GlobalState* s=Global_Get();if(!Layout_LoadFromFile(&s->layout,scene))return false;Global_OnLayoutLoaded(scene);Editor_ClearHistory(&s->editor);
    size_t count=s->layout.objectStore.count;Object3D* original=malloc(count*sizeof(*original));char* baseline=Layout_SaveToString(&s->layout);if(!original||!baseline){free(original);free(baseline);return false;}memcpy(original,s->layout.objectStore.items,count*sizeof(*original));
    bool ok=CameraView_Enter(s,false)&&Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);char path[4096];snprintf(path,sizeof(path),"%s/context_corners.json",root);FILE* f=fopen(path,"wx");if(!f)ok=false;else{fputc('{',f);bool comma=false;for(size_t i=0;i<count&&ok;++i)if(physical(&s->layout.objectStore,&original[i])){if(comma)fputc(',',f);ok=corners(f,&s->layout,&original[i]);comma=true;}fputs("}\n",f);ok=ok&&!fclose(f);}
    snprintf(path,sizeof(path),"%s/dense_corners.jsonl",root);FILE* dense=fopen(path,"wx");if(!dense)ok=false;
    ok=ok&&warm_cache(app,cb);LayoutMeshPreviewCacheStats initial_cache=Layout_MeshPreviewCacheRead();
    size_t poses=0,rules=0;double endpoint=0,min_insert=100,max_insert=0;bool fixed=true,assets=true,camera=true,ui=true;
    for(size_t n=0;n<s->layout.objectStore.constraintCount&&ok;++n){LayoutConstraint c=s->layout.objectStore.constraints[n];if(strncmp(c.id,"ax1_",4)&&strcmp(c.id,"worktop_hinge"))continue;
        if(getenv("LINE_DRAWING_ARTICULATED_HARDWARE_ONLY")&&!c.paired_folding_brace.enabled)continue;
        ++rules;
        if(c.folding_brace.manual_lock&&c.folding_brace.lock_engaged)ok=Layout_FoldingBraceSetLock(&s->layout,c.id,false,Layout_GeometryHistory,NULL);
        observer(s,(Vec3){-.20f,.96f,1.74f},(Vec3){.55f,1.08f,1.74f});
        if(c.telescoping_stay.enabled){Vec3 a,b;if(Layout_TelescopingStayAnchors(&s->layout,&c,&a,&b)){Vec3 target=Vec3_Scale(Vec3_Add(a,b),.5f);observer(s,(Vec3){a.x>0?-.20f:.25f,a.y-.15f,a.z+.12f},target);}}
        Vec3 eye=s->cameraView.eye;float yaw=s->cameraView.yaw,pitch=s->cameraView.pitch;
        ui=ui&&UIPanel_BeginEntityMotion(c.b.entity_id)&&button(MEASURE_TRAVEL_TO_MAX)&&fabs(rule(&s->layout,c.id)->target-c.travel_max)<1e-5&&button(MEASURE_TRAVEL_TO_MIN)&&fabs(rule(&s->layout,c.id)->target-c.travel_min)<1e-5&&button(MEASURE_TRAVEL_RESET)&&fabs(rule(&s->layout,c.id)->target-c.travel_home)<1e-5;
        ok=ok&&ui&&restore(s,baseline);
        if(c.folding_brace.manual_lock&&c.folding_brace.lock_engaged)ok=ok&&Layout_FoldingBraceSetLock(&s->layout,c.id,false,Layout_GeometryHistory,NULL);
        Layout_BeginGeometryGesture(&s->layout);
        for(int sample=0;sample<=180&&ok;++sample){double value=c.travel_min+(c.travel_max-c.travel_min)*sample/180;
            ok=Layout_SetTravelPosition(&s->layout,c.id,value,Layout_GeometryHistory,NULL)&&Layout_TelescopingStaysValidate(&s->layout,true);if(!ok){fprintf(stderr,"%s %.9g: %s\n",c.id,value,s->layout.geometryMessage);break;}
            const LayoutConstraint* current=rule(&s->layout,c.id);
            if(c.telescoping_stay.enabled){Vec3 a,b;ok=Layout_TelescopingStayAnchors(&s->layout,current,&a,&b);double insertion=c.telescoping_stay.length_m[0]+c.telescoping_stay.length_m[1]-Vec3_Distance(a,b);min_insert=fmin(min_insert,insertion);max_insert=fmax(max_insert,insertion);
                const Object3D* body=entity(&s->layout,c.telescoping_stay.parts[0]),*rod=entity(&s->layout,c.telescoping_stay.parts[1]);Vec3 body_start=Vec3_Scale(Layout_Transform3D_ApplyLocalPoint(body->transform,(Vec3){-c.telescoping_stay.length_m[0]/2,0,0}),Layout_WorldScale(&s->layout));Vec3 rod_end=Vec3_Scale(Layout_Transform3D_ApplyLocalPoint(rod->transform,(Vec3){c.telescoping_stay.length_m[1]/2,0,0}),Layout_WorldScale(&s->layout));endpoint=fmax(endpoint,fmax(Vec3_Distance(a,body_start),Vec3_Distance(b,rod_end)));ok=ok&&endpoint<1e-5;
            }
            for(int pair=0;pair<2;++pair){const LayoutFoldingBrace* brace=pair?&current->paired_folding_brace:&current->folding_brace;if(!brace->enabled)continue;
                ok=ok&&Layout_FoldingBracesValidate(&s->layout,true);
                Vec3 pins[3];for(int k=0;k<3;++k)pins[k]=Vec3_Sub(Vec3_Scale(entity(&s->layout,brace->parts[k+4])->transform.position,Layout_WorldScale(&s->layout)),Vec3_Scale(c.axis,brace->pin_lateral_m[k]));
                for(int k=0;k<2;++k){const Object3D* link=entity(&s->layout,brace->parts[k]);for(int end=0;end<2;++end){Vec3 actual=Vec3_Scale(Layout_Transform3D_ApplyLocalPoint(link->transform,(Vec3){(end?1:-1)*brace->length_m[k]/2,0,0}),Layout_WorldScale(&s->layout));Vec3 expected=Vec3_Add(pins[k+end],Vec3_Scale(c.axis,brace->link_lateral_m[k]));endpoint=fmax(endpoint,Vec3_Distance(actual,expected));}endpoint=fmax(endpoint,fabs(Vec3_Distance(pins[k],pins[k+1])-brace->length_m[k]));}ok=ok&&endpoint<1e-5;
            }
            fprintf(dense,"{\"rule\":\"%s\",\"position\":%.9g,\"items\":{",c.id,value);bool comma=false;
            for(size_t i=0;i<count;++i){const Object3D* o=&s->layout.objectStore.items[i];if(o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE)assets=assets&&!memcmp(&o->meshInstance,&original[i].meshInstance,sizeof(o->meshInstance));
                if(!Layout_MotionScopeContains(&s->layout.objectStore,&c,o)){fixed=fixed&&!memcmp(&o->transform,&original[i].transform,sizeof(o->transform));continue;}
                if(!physical(&s->layout.objectStore,o))continue;if(comma)fputc(',',dense);ok=ok&&corners(dense,&s->layout,o);comma=true;
            }fputs("}}\n",dense);++poses;
            camera=camera&&s->cameraView.active&&Vec3_Distance(eye,s->cameraView.eye)<1e-7&&s->cameraView.yaw==yaw&&s->cameraView.pitch==pitch;
            if(c.telescoping_stay.enabled&&(sample==0||sample==90||sample==180)){
                /* Section only for review: the independent context file still
                 * includes every fixed physical object and the actual shell. */
                for(size_t i=0;i<count;++i){const Object3D* o=&s->layout.objectStore.items[i];
                    bool keep=Layout_MotionScopeContains(&s->layout.objectStore,&c,o);
                    const char* unit=strstr(c.id,"passenger")?"upper_cabinet_passenger_unit":"upper_cabinet_driver_unit";
                    keep=keep||Layout_IsDescendant(&s->layout.objectStore,o->info.parent_id,unit);
                    if(strstr(c.id,"passenger")&&!strncmp(o->info.label,"Upper sink",10))keep=true;
                    if(strstr(c.id,"battery")){Vec3 centre;if(Layout_Object3D_ComputeVisualCenter(o,&centre)){centre=Vec3_Scale(centre,Layout_WorldScale(&s->layout));keep=keep||(centre.x<.02&&centre.y>.80&&centre.y<2.10&&centre.z<.80);}}

                    keep=keep&&original[i].coreMeta.flags.visible&&!Layout_GuideCategory(&s->layout.objectStore,o);
                    Layout_SetEntityHidden(&s->layout.objectStore,o->coreMeta.object_id,!keep);
                }
                char name[128];snprintf(name,sizeof(name),"%s_%03d",c.id,sample);ok=ok&&shot(app,cb,root,name);
                if(sample>0){for(size_t i=0;i<count;++i)if(Layout_CoveringCollectionContains(&s->layout.objectStore,"@coverings",s->layout.objectStore.items[i].coreMeta.object_id)&&Layout_MotionScopeContains(&s->layout.objectStore,&c,&s->layout.objectStore.items[i]))Layout_SetEntityHidden(&s->layout.objectStore,s->layout.objectStore.items[i].coreMeta.object_id,true);
                    snprintf(name,sizeof(name),"%s_%03d_front_hidden",c.id,sample);ok=ok&&shot(app,cb,root,name);}
                Layout_RestoreEntityVisibility(&s->layout.objectStore);
            }
        }Layout_EndGeometryGesture(&s->layout);ok=ok&&restore(s,baseline);
    }
    if(dense)ok=ok&&!fclose(dense);ok=ok&&fixed&&assets&&camera&&ui;
    /* Compatible combined opening keeps the guarded battery service cover closed.
     * The sink and battery access modes are qualified separately.
     * Combined opening uses the existing sink motions as well as every new
     * front, with the qualified manual support released through its native API. */
    bool combined=true;
    const LayoutConstraint* sink=rule(&s->layout,"worktop_hinge");
    if(sink&&sink->folding_brace.manual_lock&&sink->folding_brace.lock_engaged)combined=Layout_FoldingBraceSetLock(&s->layout,sink->id,false,Layout_GeometryHistory,NULL);
    LayoutConstraint movements[LAYOUT_MAX_CONSTRAINTS];size_t movement_count=0;
    for(size_t i=0;i<s->layout.objectStore.constraintCount;++i){LayoutConstraint c=s->layout.objectStore.constraints[i];
        if(strcmp(c.id,"ax1_battery_service") && (!strncmp(c.id,"ax1_",4)||!strncmp(c.id,"af1_",4)||!strcmp(c.id,"worktop_hinge"))&&(c.kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL||c.kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL))movements[movement_count++]=c;
    }
    snprintf(path,sizeof(path),"%s/combined_corners.jsonl",root);FILE* all=fopen(path,"wx");if(!all)combined=false;
    Layout_BeginGeometryGesture(&s->layout);
    for(int sample=0;sample<=180&&combined;++sample){
        for(size_t i=0;i<movement_count&&combined;++i){const LayoutConstraint* c=&movements[i];combined=Layout_SetTravelPosition(&s->layout,c->id,c->travel_min+(c->travel_max-c->travel_min)*sample/180,Layout_GeometryHistory,NULL);}
        if(!combined)break;fprintf(all,"{\"fraction\":%.9g,\"items\":{",sample/180.0);bool comma=false;
        for(size_t j=0;j<count;++j){const Object3D* o=&s->layout.objectStore.items[j];bool moving=false;for(size_t i=0;i<movement_count;++i)if(Layout_MotionScopeContains(&s->layout.objectStore,&movements[i],o))moving=true;
            if(!moving||!physical(&s->layout.objectStore,o))continue;if(comma)fputc(',',all);combined=combined&&corners(all,&s->layout,o);comma=true;
        }fputs("}}\n",all);
    }
    Layout_EndGeometryGesture(&s->layout);if(all)combined=combined&&!fclose(all);ok=ok&&combined&&restore(s,baseline);
    /* A real header action exercises the shared collection state and scoped reset. */
    LDVisibilityTree_Reset();LDVisibilityTree_Build(&s->layout.objectStore,"");ok=ok&&LDVisibilityTree_Hide(&s->layout.objectStore,"@guides",true)&&Layout_SetEntityHidden(&s->layout.objectStore,"@coverings",true);
    ok=ok&&header_click(app,cb,LD_VIEW_VISIBILITY)&&header_click(app,cb,LD_VIEW_COVER_RESET);LDViewControls_Close();bool guides_off=LDVisibilityTree_State(&s->layout.objectStore,"@guides")==0;ok=ok&&guides_off&&Layout_EntityVisibilityState(&s->layout.objectStore,"@coverings")==1;
    observer(s,(Vec3){0,-2,1.70f},(Vec3){0,.65f,1.05f});
    ok=ok&&shot(app,cb,root,"all_closed_guides_off");
    const LineDrawingPreviewMode modes[]={LINE_DRAWING_PREVIEW_MODE_MATERIAL,LINE_DRAWING_PREVIEW_MODE_FLAT,LINE_DRAWING_PREVIEW_MODE_WIREFRAME};
    for(int mode=0;mode<3&&ok;++mode){ok=Global_SetPreviewMode(modes[mode]);char name[80];snprintf(name,sizeof(name),"guides_off_mode_%d",mode);ok=ok&&shot(app,cb,root,name);}
    size_t guide_count=0,shown_guides=0;
    for(size_t i=0;i<count;++i)if(Layout_GuideCategory(&s->layout.objectStore,&s->layout.objectStore.items[i])){++guide_count;if(Layout_ObjectShown(&s->layout.objectStore,&s->layout.objectStore.items[i]))++shown_guides;}
    snprintf(path,sizeof(path),"%s/guide_members.json",root);f=fopen(path,"wx");
    if(f){fputc('[',f);bool comma=false;for(size_t i=0;i<count;++i){const Object3D* o=&s->layout.objectStore.items[i];int category=Layout_GuideCategory(&s->layout.objectStore,o);if(!category)continue;if(comma)fputc(',',f);fprintf(f,"{\"id\":\"%s\",\"category\":%d}",o->coreMeta.object_id,category);comma=true;}fputs("]\n",f);fclose(f);}else ok=false;
    LayoutMeshPreviewCacheStats final_cache=Layout_MeshPreviewCacheRead();
    snprintf(path,sizeof(path),"%s/cache_motion.json",root);f=fopen(path,"wx");
    if(f){fprintf(f,"{\"initial_loads\":%llu,\"final_loads\":%llu,\"initial_lod_builds\":%llu,\"final_lod_builds\":%llu,\"initial_prepared_rebuilds\":%llu,\"final_prepared_rebuilds\":%llu,\"guides\":%zu,\"shown_guides\":%zu}\n",(unsigned long long)initial_cache.loads,(unsigned long long)final_cache.loads,(unsigned long long)initial_cache.lod_builds,(unsigned long long)final_cache.lod_builds,(unsigned long long)initial_cache.prepared_rebuilds,(unsigned long long)final_cache.prepared_rebuilds,guide_count,shown_guides);fclose(f);}else ok=false;
    ok=ok&&shown_guides==0;
    snprintf(path,sizeof(path),"%s/expansion_native.json",root);f=fopen(path,"wx");if(f){fprintf(f,"{\"passed\":%s,\"rules\":%zu,\"dense_poses\":%zu,\"endpoint_error_m\":%.9g,\"min_insertion_m\":%.9g,\"max_insertion_m\":%.9g,\"fixed_geometry\":%s,\"assets_and_scales\":%s,\"camera\":%s,\"native_controls\":%s,\"cover_reset_preserves_guides_off\":%s}\n",ok?"true":"false",rules,poses,endpoint,min_insert,max_insert,fixed?"true":"false",assets?"true":"false",camera?"true":"false",ui?"true":"false",guides_off?"true":"false");fclose(f);}else ok=false;
    free(original);free(baseline);return ok;
}
