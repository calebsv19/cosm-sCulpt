#include "Layout/layout_engineering.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool string(const cJSON* o, const char* key, char* value, size_t size) {
    const cJSON* item=cJSON_GetObjectItemCaseSensitive(o,key);
    if (!cJSON_IsString(item) || strlen(item->valuestring)>=size) return false;
    snprintf(value,size,"%s",item->valuestring);
    return true;
}
cJSON* Layout_EntityInfoToJson(const LayoutEntityInfo* info) {
    if (!info || info->property_count>LAYOUT_MAX_PROPERTIES) return NULL;
    cJSON* o=cJSON_CreateObject();
    if (!o) return NULL;
    if (!cJSON_AddStringToObject(o,"name",info->label) || !cJSON_AddStringToObject(o,"type",info->entity_type) ||
        !cJSON_AddStringToObject(o,"parent",info->parent_id) || !cJSON_AddBoolToObject(o,"reference",info->reference)) {
        cJSON_Delete(o); return NULL;
    }
    cJSON* properties=cJSON_AddArrayToObject(o,"properties");
    if (!properties) {cJSON_Delete(o);return NULL;}
    for (size_t i=0;i<info->property_count;++i) {
        const LayoutProperty* p=&info->properties[i];cJSON* item=cJSON_CreateObject();
        if (!item) {cJSON_Delete(o);return NULL;}
        cJSON_AddItemToArray(properties,item);
        if (!cJSON_AddStringToObject(item,"key",p->key) || !cJSON_AddNumberToObject(item,"kind",p->kind) ||
            !(p->kind==LAYOUT_PROPERTY_TEXT ? cJSON_AddStringToObject(item,"value",p->text) :
              p->kind==LAYOUT_PROPERTY_BOOL ? cJSON_AddBoolToObject(item,"value",p->number!=0) :
              cJSON_AddNumberToObject(item,"value",p->number))) {cJSON_Delete(o);return NULL;}
    }
    return o;
}
static bool read_info(const cJSON* o, LayoutEntityInfo* info) {
    if (!cJSON_IsObject(o) || !string(o,"name",info->label,sizeof(info->label)) ||
        !string(o,"type",info->entity_type,sizeof(info->entity_type)) || !string(o,"parent",info->parent_id,sizeof(info->parent_id))) return false;
    const cJSON* reference=cJSON_GetObjectItemCaseSensitive(o,"reference");
    const cJSON* properties=cJSON_GetObjectItemCaseSensitive(o,"properties");
    if (!cJSON_IsBool(reference) || !cJSON_IsArray(properties) || cJSON_GetArraySize(properties)>LAYOUT_MAX_PROPERTIES) return false;
    info->reference=cJSON_IsTrue(reference);info->property_count=(size_t)cJSON_GetArraySize(properties);
    for (size_t i=0;i<info->property_count;++i) {
        LayoutProperty* p=&info->properties[i];const cJSON* item=cJSON_GetArrayItem(properties,(int)i);
        const cJSON* kind=cJSON_GetObjectItemCaseSensitive(item,"kind"),*value=cJSON_GetObjectItemCaseSensitive(item,"value");
        if (!string(item,"key",p->key,sizeof(p->key)) || !cJSON_IsNumber(kind) ||
            kind->valuedouble<0 || kind->valuedouble>LAYOUT_PROPERTY_LENGTH || floor(kind->valuedouble)!=kind->valuedouble) return false;
        p->kind=(LayoutPropertyKind)kind->valueint;
        if (p->kind==LAYOUT_PROPERTY_TEXT) {if (!string(item,"value",p->text,sizeof(p->text))) return false;}
        else if (p->kind==LAYOUT_PROPERTY_BOOL) {if (!cJSON_IsBool(value)) return false;p->number=cJSON_IsTrue(value);}
        else {if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble)) return false;p->number=value->valuedouble;}
    }
    return true;
}
bool Layout_EngineeringWriteJson(const Layout* layout, cJSON* root) {
    if (!Layout_ValidateEngineering(layout,NULL,0)) return false;
    cJSON* engineering=cJSON_AddObjectToObject(root,"engineering");
    if (!engineering || !cJSON_AddNumberToObject(engineering,"nextAssemblyId",layout->objectStore.next_assembly_id ? layout->objectStore.next_assembly_id : 1)) return false;
    cJSON* entities=cJSON_AddArrayToObject(engineering,"entities"),*assemblies=cJSON_AddArrayToObject(engineering,"assemblies");
    if (!entities || !assemblies) return false;
    for (size_t i=0;i<layout->objectStore.count;++i) {
        const Object3D* object=&layout->objectStore.items[i];
        if (object->isDeleted) continue;
        cJSON* o=Layout_EntityInfoToJson(&object->info);
        if (!o) return false;
        cJSON_AddItemToArray(entities,o);
        if (!cJSON_AddStringToObject(o,"id",object->coreMeta.object_id)) return false;
    }
    for (size_t i=0;i<layout->objectStore.assembly_count;++i) {
        const LayoutAssembly* a=&layout->objectStore.assemblies[i];cJSON* o=Layout_EntityInfoToJson(&a->info);
        if (!o) return false;
        cJSON_AddItemToArray(assemblies,o);
        double f[]={a->frame.origin.x,a->frame.origin.y,a->frame.origin.z,a->frame.axisU.x,a->frame.axisU.y,a->frame.axisU.z,
            a->frame.axisV.x,a->frame.axisV.y,a->frame.axisV.z,a->frame.normal.x,a->frame.normal.y,a->frame.normal.z};
        cJSON* frame=cJSON_CreateDoubleArray(f,12);
        if (!frame) return false;
        if (!cJSON_AddItemToObject(o,"worldFrame",frame)) {cJSON_Delete(frame);return false;}
        if (!cJSON_AddStringToObject(o,"id",a->id)) return false;
    }
    return true;
}
bool Layout_EngineeringReadJson(Layout* layout, const cJSON* root, bool required) {
    const cJSON* engineering=cJSON_GetObjectItemCaseSensitive(root,"engineering");
    if (!engineering) return !required;
    if (!cJSON_IsObject(engineering)) return false;
    const cJSON* entities=cJSON_GetObjectItemCaseSensitive(engineering,"entities"),*assemblies=cJSON_GetObjectItemCaseSensitive(engineering,"assemblies");
    const cJSON* next=cJSON_GetObjectItemCaseSensitive(engineering,"nextAssemblyId");
    if (!cJSON_IsArray(entities) || !cJSON_IsArray(assemblies) || cJSON_GetArraySize(assemblies)>LAYOUT_MAX_ASSEMBLIES ||
        !cJSON_IsNumber(next) || next->valuedouble<1 || next->valuedouble>UINT32_MAX || floor(next->valuedouble)!=next->valuedouble ||
        (size_t)cJSON_GetArraySize(entities)!=Layout_ObjectStore_LiveCount(&layout->objectStore)) return false;
    layout->objectStore.next_assembly_id=(uint32_t)next->valuedouble;
    for (int i=0;i<cJSON_GetArraySize(entities);++i) {
        const cJSON* item=cJSON_GetArrayItem(entities,i);char id[64];
        if (!string(item,"id",id,sizeof(id))) return false;
        for (int j=0;j<i;++j) {
            const cJSON* old=cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(entities,j),"id");
            if (!strcmp(id,old->valuestring)) return false;
        }
        LayoutEntityInfo* info=(LayoutEntityInfo*)Layout_EntityInfo(&layout->objectStore,id);
        if (!info || !read_info(item,info)) return false;
    }
    layout->objectStore.assembly_count=(size_t)cJSON_GetArraySize(assemblies);
    for (size_t i=0;i<layout->objectStore.assembly_count;++i) {
        const cJSON* item=cJSON_GetArrayItem(assemblies,(int)i);LayoutAssembly* a=&layout->objectStore.assemblies[i];
        const cJSON* frame=cJSON_GetObjectItemCaseSensitive(item,"worldFrame");
        if (!string(item,"id",a->id,sizeof(a->id)) || !read_info(item,&a->info) || !cJSON_IsArray(frame) || cJSON_GetArraySize(frame)!=12) return false;
        float values[12];
        for (int k=0;k<12;++k) {
            const cJSON* v=cJSON_GetArrayItem(frame,k);
            if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || fabs(v->valuedouble)>FLT_MAX) return false;
            values[k]=(float)v->valuedouble;
        }
        a->frame=(PlaneFrame3){.origin={values[0],values[1],values[2]},.axisU={values[3],values[4],values[5]},
            .axisV={values[6],values[7],values[8]},.normal={values[9],values[10],values[11]}};
    }
    return Layout_ValidateEngineering(layout,NULL,0) && (required || !Layout_HasEngineeringData(layout));
}
