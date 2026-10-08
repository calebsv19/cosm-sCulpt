/* Deterministic offline camera sampling. Never writes the input layout. */
#include "Layout/layout_json.h"
#include "Layout/scene/layout_camera_poses.h"
#include "Tools/canonical_scene_export.h"
#include "core_scene_compile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static cJSON *vector(Vec3 v) {
    cJSON *n=cJSON_CreateObject();
    cJSON_AddNumberToObject(n,"x",v.x);
    cJSON_AddNumberToObject(n,"y",v.y);
    cJSON_AddNumberToObject(n,"z",v.z);
    return n;
}
static bool write_json(const char *root,const char *name,const cJSON *json) {
    char path[4096];
    int n=snprintf(path,sizeof(path),"%s/%s",root,name);
    if(n<0 || (size_t)n>=sizeof(path))return false;
    char *text=cJSON_PrintUnformatted(json);
    FILE *f=text ? fopen(path,"wx") : NULL;
    bool ok=f && fputs(text,f)>=0;
    if(f && fclose(f))ok=false;
    free(text);return ok;
}
static bool integer(const char *s,int minimum,int maximum,int *out) {
    char *end;long n=strtol(s,&end,10);
    if(!s[0] || *end || n<minimum || n>maximum)return false;
    *out=(int)n;return true;
}
static cJSON *sample(const LineDrawingScenePath *path,const LineDrawingSceneCamera *camera,
                     double seconds,int index,int anchor) {
    LineDrawingSceneCameraPose pose;float fov;
    if(!CameraPoses_Time(path,camera,(float)seconds,&pose,&fov))return NULL;
    cJSON *n=cJSON_CreateObject();
    cJSON_AddNumberToObject(n,"index",index);
    cJSON_AddNumberToObject(n,"seconds",seconds);
    if(anchor>=0)cJSON_AddStringToObject(n,"anchor_id",path->keys[anchor].id);
    cJSON_AddItemToObject(n,"position",vector(pose.position));
    cJSON_AddItemToObject(n,"forward",vector(pose.forward));
    cJSON_AddItemToObject(n,"up",vector(pose.up));
    cJSON_AddNumberToObject(n,"vertical_fov_degrees",fov);
    cJSON_AddNumberToObject(n,"near_clip",camera->near_clip);
    cJSON_AddNumberToObject(n,"far_clip",camera->far_clip);
    return n;
}
int main(int argc,char **argv) {
    int fps,width,height;
    if(argc!=8 || !integer(argv[5],1,60,&fps) || !integer(argv[6],64,4096,&width) ||
       !integer(argv[7],64,4096,&height)) {
        fprintf(stderr,"usage: camera_path_tool LAYOUT PATH_ID NEW_OUTPUT_DIR SCENE_ID FPS WIDTH HEIGHT\n");
        return 1;
    }
    Layout layout={0};Layout_Init(&layout,.05f);
    cJSON *manifest=NULL,*scene=NULL;char *text=NULL;int result=1;
    if(!Layout_LoadFromFile(&layout,argv[1]))goto done;
    const LineDrawingScenePath *path=Layout_SceneAuthoringState_FindPathByIdConst(&layout.sceneAuthoring,argv[2]);
    const LineDrawingSceneCamera *camera=path ? Layout_SceneAuthoringState_FindCameraForPathConst(&layout.sceneAuthoring,path) : NULL;
    if(!path || !camera || !CameraPoses_Valid(path) || !path->key_count || path->closed)goto done;
    double duration=CameraPoses_Duration(path);
    if(!isfinite(duration) || duration<=0 || duration*fps>4095)goto done;
    int frames=(int)ceil(duration*fps)+1;
    manifest=cJSON_CreateObject();
    cJSON_AddStringToObject(manifest,"schema","line_drawing_camera_samples_v1");
    cJSON_AddStringToObject(manifest,"path_id",path->path_id);
    cJSON_AddStringToObject(manifest,"camera_id",camera->camera_id);
    cJSON_AddNumberToObject(manifest,"fps",fps);
    cJSON_AddNumberToObject(manifest,"width",width);
    cJSON_AddNumberToObject(manifest,"height",height);
    cJSON_AddNumberToObject(manifest,"duration_seconds",duration);
    cJSON *anchors=cJSON_AddArrayToObject(manifest,"anchors");
    cJSON *samples=cJSON_AddArrayToObject(manifest,"frames");
    double t=0;
    for(size_t i=0;i<path->key_count;++i) {
        cJSON *n=sample(path,camera,t,(int)i,(int)i);
        if(!n)goto done;
        cJSON_AddItemToArray(anchors,n);t+=path->keys[i].seconds;
    }
    for(int i=0;i<frames;++i) {
        cJSON *n=sample(path,camera,fmin((double)i/fps,duration),i,-1);
        if(!n)goto done;
        cJSON_AddItemToArray(samples,n);
    }
    text=LineDrawingCanonicalScene_ExportLayoutToString(&layout,argv[4]);
    scene=text ? cJSON_Parse(text) : NULL;
    if(!scene || mkdir(argv[3],0700))goto done;
    if(!write_json(argv[3],"scene_authoring.json",scene) ||
       !write_json(argv[3],"camera_samples.json",manifest))goto done;
    char authoring[4096],runtime[4096],diagnostics[512];
    if(snprintf(authoring,sizeof(authoring),"%s/scene_authoring.json",argv[3])>=(int)sizeof(authoring) ||
       snprintf(runtime,sizeof(runtime),"%s/scene_runtime.json",argv[3])>=(int)sizeof(runtime))goto done;
    if(core_scene_compile_authoring_file_to_runtime_file(authoring,runtime,diagnostics,sizeof(diagnostics)).code!=CORE_OK) {
        fprintf(stderr,"%s\n",diagnostics);goto done;
    }
    printf("Exported %zu anchors and %d frames (%.6f seconds)\n",path->key_count,frames,duration);
    result=0;
done:
    if(result)fprintf(stderr,"Camera export refused or failed; source unchanged. Existing output directories are never reused.\n");
    free(text);cJSON_Delete(scene);cJSON_Delete(manifest);Layout_Free(&layout);return result;
}
