#define _POSIX_C_SOURCE 200809L
#include "Core/agent_control.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_json.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_collection_inspection.h"
#include "Layout/layout_coverings.h"
#include "UI/ui_panel_internal.h"
#include "vk_renderer.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* text(const cJSON* j,const char* key) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:"";
}
static struct {char* request;char* response;char* id;} receipts[8];
static size_t next_receipt;
static void (*enter_editor)(void);
void SculptAgent_SetEnterEditor(void (*enter)(void)){enter_editor=enter;}
static bool vector_valid(const cJSON* a) {
    if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)!=3)return false;
    for(int k=0;k<3;++k){const cJSON* v=cJSON_GetArrayItem(a,k);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||fabs(v->valuedouble)>10000)return false;}
    return true;
}
static void revision(Layout* layout,char out[32]) {
    char* s=Layout_SaveToString(layout);uint64_t h=UINT64_C(14695981039346656037);
    if(s){for(unsigned char* p=(unsigned char*)s;*p;++p){h^=*p;h*=UINT64_C(1099511628211);}Layout_FreeString(s);}
    snprintf(out,32,"%016llx",(unsigned long long)h);
}
static cJSON* state(AppContext* app,GlobalState* s) {
    cJSON* r=cJSON_CreateObject();char rev[32];revision(&s->layout,rev);
    cJSON_AddStringToObject(r,"revision",rev);cJSON_AddStringToObject(r,"path",Global_GetCurrentConfigPath());
    cJSON_AddBoolToObject(r,"dirty",Global_IsLayoutDirty());cJSON_AddNumberToObject(r,"pid",getpid());
    cJSON_AddNumberToObject(r,"parts",s->layout.objectStore.count);cJSON_AddNumberToObject(r,"presented_frames",(double)app->presented_frames);
    cJSON_AddBoolToObject(r,"camera_active",s->cameraView.active);cJSON_AddBoolToObject(r,"opaque",!s->inspectionView);
    cJSON* views=cJSON_AddArrayToObject(r,"views");
    for(size_t i=0;i<s->layout.objectStore.working_view_count;++i){const LayoutWorkingView* v=&s->layout.objectStore.working_views[i];cJSON* row=cJSON_CreateObject();cJSON_AddStringToObject(row,"id",v->id);cJSON_AddStringToObject(row,"name",v->name);cJSON_AddStringToObject(row,"only",v->only);cJSON_AddItemToArray(views,row);}
    cJSON* groups=cJSON_AddArrayToObject(r,"collections");
    for(size_t i=0;i<s->layout.objectStore.scene_collection_count;++i){const LayoutSceneCollection* c=&s->layout.objectStore.scene_collections[i];cJSON* row=cJSON_CreateObject();cJSON_AddStringToObject(row,"id",c->id);cJSON_AddStringToObject(row,"name",c->name);cJSON_AddItemToArray(groups,row);}
    return r;
}
static bool snapshot(GlobalState* s,const char* path) {
    if(path[0]!='/')return false;
    char* data=Layout_SaveToString(&s->layout);if(!data)return false;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);bool ok=fd>=0;
    size_t offset=0,length=strlen(data);
    while(ok&&offset<length){ssize_t n=write(fd,data+offset,length-offset);if(n<=0)ok=false;else offset+=(size_t)n;}
    if(fd>=0){if(fsync(fd))ok=false;if(close(fd))ok=false;if(!ok)unlink(path);}
    Layout_FreeString(data);return ok;
}
char* SculptAgent_Command(AppContext* app,const char* input) {
    cJSON* q=cJSON_Parse(input);cJSON* r=cJSON_CreateObject();GlobalState* s=Global_Get();const char* error=NULL;
    const char* op=text(q,"operation"),*id=text(q,"request_id");
    for(size_t i=0;i<8&&id[0];++i)if(receipts[i].id&&!strcmp(id,receipts[i].id)) {
        if(!strcmp(input,receipts[i].request)){cJSON_Delete(q);cJSON_Delete(r);return strdup(receipts[i].response);}
        error="request_id reused with different bytes";
    }
    bool reading=!strcmp(op,"state")||!strcmp(op,"inspect");
    if(!q||!s)error="No active layout or malformed JSON";
    if(!reading&&!id[0])error="request_id required";
    char rev[32];if(s)revision(&s->layout,rev);
    if(!error&&!reading&&strcmp(text(q,"expected_revision"),rev))error="revision conflict; read current state";
    const cJSON* requested_eye=cJSON_GetObjectItemCaseSensitive(q,"eye_m");
    const cJSON* requested_target=cJSON_GetObjectItemCaseSensitive(q,"target_m");
    if(!error&&(requested_eye||requested_target)) {
        if(!vector_valid(requested_eye)||!vector_valid(requested_target))error="eye_m and target_m require three finite metres each";
        else {double distance=0;for(int k=0;k<3;++k){double d=cJSON_GetArrayItem(requested_eye,k)->valuedouble-cJSON_GetArrayItem(requested_target,k)->valuedouble;distance+=d*d;}if(distance<1e-10)error="camera eye and target coincide";}
    }
    if(!error&&!strcmp(op,"state"))cJSON_AddItemToObject(r,"state",state(app,s));
    else if(!error&&(!strcmp(op,"snapshot")||!strcmp(op,"save_as"))) {
        if(!snapshot(s,text(q,"path")))error="snapshot refused; use a new absolute path";
        else if(!strcmp(op,"save_as"))Global_OnLayoutSaved(text(q,"path"));
    } else if(!error&&!strcmp(op,"open")) {
        const char* path=text(q,"path");
        if(!enter_editor)error="editor lifecycle is not ready";
        else if(Global_IsLayoutDirty()||UIPanel_IsCapturingKeyboard()||s->cameraView.path_modified)error="unsaved edits or active draft; save before switching documents";
        else if(path[0]!='/'||!UIPanel_LoadLayoutFromPath(path))error="native load refused";
        else enter_editor();
    } else if(!error&&!strcmp(op,"view")) {
        const char* view=text(q,"view_id");bool guides=false;
        const LayoutWorkingView* found=NULL;
        for(size_t i=0;i<s->layout.objectStore.working_view_count;++i)if(!strcmp(s->layout.objectStore.working_views[i].id,view))found=&s->layout.objectStore.working_views[i];
        if(s->cameraView.path_modified||UIPanel_IsCapturingKeyboard())error="active draft; view refused";
        else if(!found)error="unknown working view";
        else {
            char only[64];snprintf(only,64,"%s",found->only);
            LayoutCollectionInspection b=Layout_InspectCollection(&s->layout,only,"");
            if(only[0]&&b.status!=LAYOUT_MEASUREMENT_OK)error="view has no valid inspection bounds";
            else if(!Layout_ApplyWorkingView(&s->layout.objectStore,view,&guides))error="view refused";
            else {
                s->authoringGuides=false;s->inspectionView=false;s->editor.selectedObject3DId=0;s->editor.hoveredObject3DId=0;
                Global_SetSpaceMode(SPACE_MODE_3D,false);Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);
                if(!s->cameraView.active)(void)CameraView_Enter(s,false);
                double scale=Layout_WorldScale(&s->layout);
                Vec3 lo={b.world_min_meters[0]/scale,b.world_min_meters[1]/scale,b.world_min_meters[2]/scale};
                Vec3 hi={b.world_max_meters[0]/scale,b.world_max_meters[1]/scale,b.world_max_meters[2]/scale};
                if(only[0]?!CameraView_FrameBounds(s,lo,hi):!CameraView_Frame(s,false))error="camera framing refused";
                const cJSON* eye=cJSON_GetObjectItemCaseSensitive(q,"eye_m");
                const cJSON* target=cJSON_GetObjectItemCaseSensitive(q,"target_m");
                if(cJSON_IsArray(eye)&&cJSON_GetArraySize(eye)==3&&cJSON_IsArray(target)&&cJSON_GetArraySize(target)==3) {
                    float e[3],t[3];bool valid=true;
                    for(int k=0;k<3;++k){const cJSON* a=cJSON_GetArrayItem(eye,k),*bq=cJSON_GetArrayItem(target,k);valid=valid&&cJSON_IsNumber(a)&&cJSON_IsNumber(bq)&&isfinite(a->valuedouble)&&isfinite(bq->valuedouble);e[k]=(float)(a->valuedouble/scale);t[k]=(float)(bq->valuedouble/scale);}
                    if(valid){Vec3 delta={t[0]-e[0],t[1]-e[1],t[2]-e[2]};float h=hypotf(delta.x,delta.y);s->cameraView.eye=(Vec3){e[0],e[1],e[2]};s->cameraView.orbit_target=(Vec3){t[0],t[1],t[2]};s->cameraView.yaw=atan2f(delta.y,delta.x)*180.0f/3.14159265358979323846f;s->cameraView.pitch=atan2f(delta.z,h)*180.0f/3.14159265358979323846f;s->cameraView.roll=0;}
                }
                Global_FlagHitboxesDirty();app->redraw_requested=true;
                cJSON_AddStringToObject(r,"applied_view",view);cJSON_AddStringToObject(r,"only",only);
            }
        }
    } else if(!error&&!strcmp(op,"inspect")) {
        LayoutCollectionInspection b=Layout_InspectCollection(&s->layout,text(q,"collection_id"),"");
        if(b.status!=LAYOUT_MEASUREMENT_OK)error="invalid inspection collection";
        else {cJSON_AddNumberToObject(r,"parts",b.parts);cJSON_AddItemToObject(r,"min_m",cJSON_CreateDoubleArray(b.world_min_meters,3));cJSON_AddItemToObject(r,"max_m",cJSON_CreateDoubleArray(b.world_max_meters,3));}
    } else if(!error&&!strcmp(op,"capture")) {
        const char* path=text(q,"path");
        if(path[0]!='/'||access(path,F_OK)==0||vk_renderer_request_capture(app->renderer,path)!=VK_SUCCESS)error="capture refused; use a new absolute BMP path";
        else {app->redraw_requested=true;cJSON_AddStringToObject(r,"capture_scheduled",path);}
    } else if(!error&&!strcmp(op,"quit")) {
        if(Global_IsLayoutDirty()||UIPanel_IsCapturingKeyboard()||s->cameraView.path_modified)error="unsaved edits or active draft; quit refused";else app->quit=true;
    } else if(!error&&!reading)error="unknown operation";
    if(error)cJSON_AddStringToObject(r,"error",error);
    cJSON_AddBoolToObject(r,"ok",error==NULL);if(s){revision(&s->layout,rev);cJSON_AddStringToObject(r,"revision",rev);}
    cJSON_AddStringToObject(r,"request_id",id);char* output=cJSON_PrintUnformatted(r);
    if(output&&id[0]&&!reading&&!error){size_t i=next_receipt++%8;free(receipts[i].request);free(receipts[i].response);free(receipts[i].id);receipts[i].request=strdup(input);receipts[i].response=strdup(output);receipts[i].id=strdup(id);}
    cJSON_Delete(q);cJSON_Delete(r);return output;
}
