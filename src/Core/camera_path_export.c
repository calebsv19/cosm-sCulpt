#include "Core/camera_path_export.h"
#include "Core/camera_path.h"
#include "Core/global_state.h"
#include "Layout/layout_json.h"
#include "UI/ui_panel.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static cJSON *read_json(const char *path) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    char *text=malloc(4*1024*1024+1);if(!text){fclose(f);return NULL;}
    size_t n=fread(text,1,4*1024*1024,f);bool ok=!ferror(f) && feof(f);
    fclose(f);text[n]=0;cJSON *json=ok ? cJSON_Parse(text) : NULL;free(text);return json;
}
static int number(const cJSON *json,const char *key) {
    const cJSON *n=cJSON_GetObjectItemCaseSensitive(json,key);
    return cJSON_IsNumber(n) && n->valuedouble==n->valueint ? n->valueint : 0;
}
static bool close_number(const cJSON *json,const char *key,float actual) {
    const cJSON *n=cJSON_GetObjectItemCaseSensitive(json,key);
    return cJSON_IsNumber(n) && isfinite(n->valuedouble) && fabs(n->valuedouble-actual)<1e-4;
}
static bool close_vector(const cJSON *json,const char *key,Vec3 actual) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(json,key);
    return close_number(v,"x",actual.x) && close_number(v,"y",actual.y) && close_number(v,"z",actual.z);
}
static bool matches(const cJSON *n,const CameraViewSession *v) {
    Vec3 forward,up;CameraView_Basis(v,&forward,NULL,&up);
    return close_vector(n,"position",v->eye) && close_vector(n,"forward",forward) &&
        close_vector(n,"up",up) && close_number(n,"vertical_fov_degrees",v->fov) &&
        close_number(n,"near_clip",v->near_clip) && close_number(n,"far_clip",v->far_clip);
}
static bool capture(AppContext *app,const AppCallbacks *cb,const char *root,const char *kind,
                    int index,int width,int height) {
    char temporary[4096],out[4096];
    if(snprintf(temporary,sizeof(temporary),"%s/capture.bmp",root)>=(int)sizeof(temporary) ||
       snprintf(out,sizeof(out),"%s/%s_%04d.bmp",root,kind,index)>=(int)sizeof(out))return false;
    if(vk_renderer_request_capture(app->renderer,temporary)!=VK_SUCCESS || !App_RenderOnce(app,cb->handleRender))return false;
    SDL_Surface *full=SDL_LoadBMP(temporary),*cropped=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
    bool ok=false;GlobalState *s=Global_Get();
    if(full && cropped) {
        float sx=(float)full->w/s->screenWidth,sy=(float)full->h/s->screenHeight;
        SDL_Rect rect={(int)(s->cameraView.viewport[0]*sx),(int)(s->cameraView.viewport[1]*sy),
            (int)(s->cameraView.viewport[2]*sx),(int)(s->cameraView.viewport[3]*sy)};
        ok=rect.x>=0 && rect.y>=0 && rect.x+rect.w<=full->w && rect.y+rect.h<=full->h &&
            SDL_BlitScaled(full,&rect,cropped,NULL)==0 && SDL_SaveBMP(cropped,out)==0;
    }
    SDL_FreeSurface(full);SDL_FreeSurface(cropped);
    /* Only this invocation's single scratch capture is disposable. */
    if(ok)(void)remove(temporary);
    return ok;
}
bool CameraPathExport_Run(AppContext *app,const AppCallbacks *cb,const char *manifest_path) {
    const char *layout_path=getenv("LINE_DRAWING_CAMERA_EXPORT_LAYOUT");
    const char *root=getenv("LINE_DRAWING_CAMERA_EXPORT_OUTPUT");
    cJSON *json=read_json(manifest_path);bool ok=false;
    const cJSON *schema=cJSON_GetObjectItemCaseSensitive(json,"schema");
    const cJSON *id=cJSON_GetObjectItemCaseSensitive(json,"path_id");
    int width=number(json,"width"),height=number(json,"height");
    if(!layout_path || !root || !cJSON_IsString(schema) || strcmp(schema->valuestring,"line_drawing_camera_samples_v1") ||
       !cJSON_IsString(id) || width<64 || width>1600 || height<64 || height>1000)goto done;
    GlobalState *s=Global_Get();
    if(!Layout_LoadFromFile(&s->layout,layout_path))goto done;
    const LineDrawingScenePath *path=Layout_SceneAuthoringState_FindPathByIdConst(&s->layout.sceneAuthoring,id->valuestring);
    if(!path || !path->key_count)goto done;
    s->layout.sceneAuthoring.selected_kind=LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH;
    s->layout.sceneAuthoring.selected_index=(size_t)(path-s->layout.sceneAuthoring.paths);
    s->previewMode=LINE_DRAWING_PREVIEW_MODE_MATERIAL;s->inspectionView=true;
    if(!CameraPath_ViewPoint(s,0) || mkdir(root,0700))goto done;
    for(int i=0;i<3;++i) {
        cb->handleUpdate(app);
        /* The first submission after a resize only recreates the swapchain. */
        if(!App_RenderOnce(app,cb->handleRender) && !App_RenderOnce(app,cb->handleRender))goto done;
        CameraView_RefreshViewport(s);
        int dw=width-(int)s->cameraView.viewport[2],dh=height-(int)s->cameraView.viewport[3];
        if(!dw && !dh)break;
        SDL_SetWindowSize(app->window,s->screenWidth+dw,s->screenHeight+dh);
    }
    CameraView_RefreshViewport(s);
    if((int)s->cameraView.viewport[2]!=width || (int)s->cameraView.viewport[3]!=height)goto done;
    const char *kinds[]={"anchors","frames"};
    for(int kind=0;kind<2;++kind) {
        const cJSON *array=cJSON_GetObjectItemCaseSensitive(json,kinds[kind]);
        int count=cJSON_GetArraySize(array);
        if(!cJSON_IsArray(array) || count<1 || count>4096)goto done;
        for(int i=0;i<count;++i) {
            const cJSON *entry=cJSON_GetArrayItem(array,i);
            const cJSON *n=cJSON_GetObjectItemCaseSensitive(entry,"seconds");
            if(!cJSON_IsNumber(n) || !isfinite(n->valuedouble) ||
               !CameraPath_Scrub(s,(float)n->valuedouble) || !matches(entry,&s->cameraView))goto done;
            cb->handleUpdate(app);
            if(!capture(app,cb,root,kinds[kind],i,width,height))goto done;
        }
    }
    char receipt[4096];
    if(snprintf(receipt,sizeof(receipt),"%s/complete.json",root)>=(int)sizeof(receipt))goto done;
    FILE *f=fopen(receipt,"wx");
    if(!f)goto done;
    ok=fputs("{\"status\":\"succeeded\",\"sampling\":\"explicit_native_scrub\",\"all_camera_frames_match\":true}\n",f)>=0;
    if(fclose(f))ok=false;
done:
    cJSON_Delete(json);
    fprintf(stderr,"Camera native sequence %s\n",ok ? "succeeded" : "failed (partial output retained)");
    return ok;
}
