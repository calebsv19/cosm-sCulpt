#include "Layout/scene/layout_folding_brace.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool vector(const cJSON* o,const char* key,double* values,int count) {
    const cJSON* a=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsArray(a) || cJSON_GetArraySize(a)!=count) return false;
    for(int i=0;i<count;++i) {const cJSON* v=cJSON_GetArrayItem(a,i);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)) return false;values[i]=v->valuedouble;}
    return true;
}
bool Layout_FoldingBraceRead(const cJSON* o,LayoutFoldingBrace* out) {
    LayoutFoldingBrace f={.enabled=true};const cJSON* branch=cJSON_GetObjectItemCaseSensitive(o,"branch"),*parts=cJSON_GetObjectItemCaseSensitive(o,"parts");
    if(!cJSON_IsObject(o)||!vector(o,"fixed_offset_m",f.fixed_offset_m,3)||!vector(o,"leaf_offset_m",f.leaf_offset_m,3)||!vector(o,"length_m",f.length_m,2)||!cJSON_IsNumber(branch)||(branch->valuedouble!=1 && branch->valuedouble!=-1)||!cJSON_IsArray(parts)||cJSON_GetArraySize(parts)!=9) return false;
    f.branch=branch->valueint;
    for(int i=0;i<9;++i) {const cJSON* v=cJSON_GetArrayItem(parts,i);if(!cJSON_IsString(v)||!v->valuestring[0]||strlen(v->valuestring)>=64) return false;snprintf(f.parts[i],64,"%s",v->valuestring);}
    const cJSON* compact=cJSON_GetObjectItemCaseSensitive(o,"compactMount");
    if(compact){
        if(!cJSON_IsObject(compact)||!vector(compact,"link_lateral_m",f.link_lateral_m,2)||!vector(compact,"fixed_plate_offset_m",f.fixed_plate_offset_m,3)||!vector(compact,"leaf_plate_offset_m",f.leaf_plate_offset_m,3))return false;
        if(cJSON_GetObjectItemCaseSensitive(compact,"pin_lateral_m")&&!vector(compact,"pin_lateral_m",f.pin_lateral_m,3))return false;
        f.compact_mount=true;
    }
    const cJSON* lock=cJSON_GetObjectItemCaseSensitive(o,"manualLock");
    if(lock){const cJSON* engaged=cJSON_GetObjectItemCaseSensitive(lock,"engaged"),*angle=cJSON_GetObjectItemCaseSensitive(lock,"position_deg");
        if(!f.compact_mount||!cJSON_IsObject(lock)||!cJSON_IsBool(engaged)||!cJSON_IsNumber(angle)||!isfinite(angle->valuedouble))return false;
        f.manual_lock=true;f.lock_engaged=cJSON_IsTrue(engaged);f.lock_position_deg=angle->valuedouble;
    }
    *out=f;return true;
}
bool Layout_FoldingBraceWrite(const LayoutFoldingBrace* f,cJSON* root) {
    if(!f->enabled) return true;
    cJSON* o=cJSON_AddObjectToObject(root,"foldingBrace");if(!o) return false;
    if(!cJSON_AddItemToObject(o,"fixed_offset_m",cJSON_CreateDoubleArray(f->fixed_offset_m,3))||!cJSON_AddItemToObject(o,"leaf_offset_m",cJSON_CreateDoubleArray(f->leaf_offset_m,3))||!cJSON_AddItemToObject(o,"length_m",cJSON_CreateDoubleArray(f->length_m,2))||!cJSON_AddNumberToObject(o,"branch",f->branch)) return false;
    if(f->compact_mount){cJSON* m=cJSON_AddObjectToObject(o,"compactMount");if(!m||!cJSON_AddItemToObject(m,"link_lateral_m",cJSON_CreateDoubleArray(f->link_lateral_m,2))||!cJSON_AddItemToObject(m,"fixed_plate_offset_m",cJSON_CreateDoubleArray(f->fixed_plate_offset_m,3))||!cJSON_AddItemToObject(m,"leaf_plate_offset_m",cJSON_CreateDoubleArray(f->leaf_plate_offset_m,3)))return false;}
    if(f->compact_mount){cJSON* m=cJSON_GetObjectItemCaseSensitive(o,"compactMount");if(!cJSON_AddItemToObject(m,"pin_lateral_m",cJSON_CreateDoubleArray(f->pin_lateral_m,3)))return false;}
    if(f->manual_lock){cJSON* m=cJSON_AddObjectToObject(o,"manualLock");if(!m||!cJSON_AddBoolToObject(m,"engaged",f->lock_engaged)||!cJSON_AddNumberToObject(m,"position_deg",f->lock_position_deg))return false;}
    const char* parts[9];for(int i=0;i<9;++i) parts[i]=f->parts[i];return cJSON_AddItemToObject(o,"parts",cJSON_CreateStringArray(parts,9));
}
