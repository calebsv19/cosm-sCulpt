#include "Layout/scene/layout_telescoping_stay.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool vector(const cJSON* o,const char* key,double* v,int count){
    const cJSON* a=cJSON_GetObjectItemCaseSensitive(o,key);if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)!=count)return false;
    for(int i=0;i<count;++i){const cJSON* n=cJSON_GetArrayItem(a,i);if(!cJSON_IsNumber(n)||!isfinite(n->valuedouble))return false;v[i]=n->valuedouble;}return true;
}
bool Layout_TelescopingStayRead(const cJSON* o,LayoutTelescopingStay* out){
    LayoutTelescopingStay s={.enabled=true};double insertion[2];const cJSON* parts=cJSON_GetObjectItemCaseSensitive(o,"parts");
    if(!cJSON_IsObject(o)||!vector(o,"fixed_offset_m",s.fixed_offset_m,3)||!vector(o,"leaf_offset_m",s.leaf_offset_m,3)||!vector(o,"length_m",s.length_m,2)||
        !vector(o,"insertion_m",insertion,2)||!vector(o,"fixed_plate_offset_m",s.fixed_plate_offset_m,3)||!vector(o,"leaf_plate_offset_m",s.leaf_plate_offset_m,3)||!cJSON_IsArray(parts)||cJSON_GetArraySize(parts)!=6)return false;
    s.insertion_min_m=insertion[0];s.insertion_max_m=insertion[1];
    for(int i=0;i<6;++i){const cJSON* v=cJSON_GetArrayItem(parts,i);if(!cJSON_IsString(v)||!v->valuestring[0]||strlen(v->valuestring)>=64)return false;snprintf(s.parts[i],64,"%s",v->valuestring);}*out=s;return true;
}
bool Layout_TelescopingStayWrite(const LayoutTelescopingStay* s,cJSON* root){
    if(!s->enabled)return true;cJSON* o=cJSON_AddObjectToObject(root,"telescopingStay");double insertion[]={s->insertion_min_m,s->insertion_max_m};
    if(!o||!cJSON_AddItemToObject(o,"fixed_offset_m",cJSON_CreateDoubleArray(s->fixed_offset_m,3))||!cJSON_AddItemToObject(o,"leaf_offset_m",cJSON_CreateDoubleArray(s->leaf_offset_m,3))||
        !cJSON_AddItemToObject(o,"length_m",cJSON_CreateDoubleArray(s->length_m,2))||!cJSON_AddItemToObject(o,"insertion_m",cJSON_CreateDoubleArray(insertion,2))||
        !cJSON_AddItemToObject(o,"fixed_plate_offset_m",cJSON_CreateDoubleArray(s->fixed_plate_offset_m,3))||!cJSON_AddItemToObject(o,"leaf_plate_offset_m",cJSON_CreateDoubleArray(s->leaf_plate_offset_m,3)))return false;
    const char* parts[6];for(int i=0;i<6;++i)parts[i]=s->parts[i];return cJSON_AddItemToObject(o,"parts",cJSON_CreateStringArray(parts,6));
}
