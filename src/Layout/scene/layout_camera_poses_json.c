#include "Layout/scene/layout_camera_poses.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

cJSON *CameraPoses_ToJson(const LineDrawingScenePath *p) {
    if(!CameraPoses_Valid(p)) return NULL;
    cJSON *root=cJSON_CreateObject(),*keys=cJSON_CreateArray();
    if(!root || !keys) { cJSON_Delete(root); cJSON_Delete(keys); return NULL; }
    cJSON_AddNumberToObject(root,"version",1);
    cJSON_AddNumberToObject(root,"next_id",p->next_key_id);
    cJSON_AddStringToObject(root,"orientation","quaternion_wxyz_x_forward_z_up");
    cJSON_AddItemToObject(root,"keys",keys);
    for(size_t i=0;i<p->key_count;++i) {
        cJSON *k=cJSON_CreateObject();
        cJSON_AddItemToArray(keys,k);
        cJSON_AddStringToObject(k,"id",p->keys[i].id);
        cJSON_AddItemToObject(k,"rotation",cJSON_CreateFloatArray(p->keys[i].rotation,4));
        cJSON_AddNumberToObject(k,"fov",p->keys[i].fov);
        cJSON_AddNumberToObject(k,"seconds",p->keys[i].seconds);
    }
    return root;
}
bool CameraPoses_FromJson(LineDrawingScenePath *p,const cJSON *node) {
    if(!node) return true;
    if(!p || !cJSON_IsObject(node)) return false;
    const cJSON *version=cJSON_GetObjectItemCaseSensitive(node,"version");
    const cJSON *keys=cJSON_GetObjectItemCaseSensitive(node,"keys");
    const cJSON *orientation=cJSON_GetObjectItemCaseSensitive(node,"orientation");
    if(!cJSON_IsString(orientation) || strcmp(orientation->valuestring,"quaternion_wxyz_x_forward_z_up")) return false;
    const cJSON *next=cJSON_GetObjectItemCaseSensitive(node,"next_id");
    if(!cJSON_IsNumber(version) || version->valuedouble!=1 || !cJSON_IsArray(keys) ||
       !cJSON_IsNumber(next) || !isfinite(next->valuedouble) || next->valuedouble<0 || next->valuedouble>1000000000 || floor(next->valuedouble)!=next->valuedouble) return false;
    int n=cJSON_GetArraySize(keys);
    if(n<1 || n>LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) return false;
    LineDrawingScenePath copy=*p;
    memset(copy.keys,0,sizeof(copy.keys)); copy.key_count=(size_t)n; copy.next_key_id=(unsigned)next->valuedouble;
    for(int i=0;i<n;++i) {
        const cJSON *k=cJSON_GetArrayItem(keys,i),*id=cJSON_GetObjectItemCaseSensitive(k,"id");
        const cJSON *q=cJSON_GetObjectItemCaseSensitive(k,"rotation");
        const cJSON *fov=cJSON_GetObjectItemCaseSensitive(k,"fov"),*seconds=cJSON_GetObjectItemCaseSensitive(k,"seconds");
        if(!cJSON_IsString(id) || !id->valuestring || strlen(id->valuestring)>=sizeof(copy.keys[i].id) ||
           !cJSON_IsArray(q) || cJSON_GetArraySize(q)!=4 || !cJSON_IsNumber(fov) || !cJSON_IsNumber(seconds)) return false;
        snprintf(copy.keys[i].id,sizeof(copy.keys[i].id),"%s",id->valuestring);
        copy.keys[i].fov=(float)fov->valuedouble; copy.keys[i].seconds=(float)seconds->valuedouble;
        for(int j=0;j<4;++j) {
            const cJSON *v=cJSON_GetArrayItem(q,j);
            if(!cJSON_IsNumber(v)) return false;
            copy.keys[i].rotation[j]=(float)v->valuedouble;
        }
    }
    if(!CameraPoses_Valid(&copy)) return false;
    *p=copy; p->playing=false;
    return true;
}
