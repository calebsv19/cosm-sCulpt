#include "Layout/layout_collection_inspection.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool text(const cJSON* o,const char* key,char* out,size_t n) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsString(v)||strlen(v->valuestring)>=n)return false;
    snprintf(out,n,"%s",v->valuestring);return true;
}
static bool integer(const cJSON* o,const char* key,int* out,int max) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>max||floor(v->valuedouble)!=v->valuedouble)return false;
    *out=(int)v->valuedouble;return true;
}
static bool write_datum(cJSON* o,const char* key,const LayoutInspectionDatum* d) {
    cJSON* v=cJSON_AddObjectToObject(o,key);return v&&cJSON_AddStringToObject(v,"entity",d->entity_id)&&cJSON_AddNumberToObject(v,"feature",d->feature);
}
static bool read_datum(const cJSON* o,const char* key,LayoutInspectionDatum* d) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(o,key);
    return cJSON_IsObject(v)&&text(v,"entity",d->entity_id,64)&&integer(v,"feature",&d->feature,3);
}
bool Layout_MeasurementsWriteJson(const Layout* l,cJSON* eng) {
    if(!Layout_ValidateMeasurements(l))return false;
    if(!l->objectStore.measurement_count)return true;
    cJSON* rows=cJSON_AddArrayToObject(eng,"measurements");if(!rows)return false;
    for(size_t i=0;i<l->objectStore.measurement_count;++i) {
        const LayoutSavedMeasurement* m=&l->objectStore.measurements[i];cJSON* o=cJSON_CreateObject();
        if(!o||!cJSON_AddItemToArray(rows,o)||!cJSON_AddStringToObject(o,"id",m->id)||!cJSON_AddStringToObject(o,"name",m->name)||
           !cJSON_AddStringToObject(o,"collection",m->collection)||!cJSON_AddStringToObject(o,"frameEntity",m->frame_entity)||
           !cJSON_AddNumberToObject(o,"kind",m->kind)||!write_datum(o,"a",&m->a)||!write_datum(o,"b",&m->b))return false;
    }
    return true;
}
bool Layout_MeasurementsReadJson(Layout* l,const cJSON* eng) {
    const cJSON* rows=cJSON_GetObjectItemCaseSensitive(eng,"measurements");if(!rows)return true;
    if(!cJSON_IsArray(rows)||cJSON_GetArraySize(rows)>LAYOUT_MAX_SAVED_MEASUREMENTS)return false;
    l->objectStore.measurement_count=(size_t)cJSON_GetArraySize(rows);
    for(size_t i=0;i<l->objectStore.measurement_count;++i) {
        LayoutSavedMeasurement* m=&l->objectStore.measurements[i];const cJSON* o=cJSON_GetArrayItem(rows,(int)i);
        if(!text(o,"id",m->id,64)||!text(o,"name",m->name,96)||!text(o,"collection",m->collection,64)||!text(o,"frameEntity",m->frame_entity,64)||
           !integer(o,"kind",&m->kind,6)||!read_datum(o,"a",&m->a)||!read_datum(o,"b",&m->b))return false;
    }
    return Layout_ValidateMeasurements(l);
}
