#include "Tools/layout_edit_inspection.h"
#include "Layout/layout_scene_collections.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool text(const cJSON* o,const char* key,char* out,size_t capacity,bool optional) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(o,key);if(!v&&optional){out[0]=0;return true;}
    if(!cJSON_IsString(v)||strlen(v->valuestring)>=capacity)return false;snprintf(out,capacity,"%s",v->valuestring);return true;
}
static bool integer(const cJSON* o,const char* key,int* out,int max) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(o,key);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>max||floor(v->valuedouble)!=v->valuedouble)return false;*out=(int)v->valuedouble;return true;
}
static bool datum(const cJSON* o,const char* key,LayoutInspectionDatum* d,bool optional) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(o,key);if(!v&&optional)return true;
    return cJSON_IsObject(v)&&text(v,"entity",d->entity_id,64,false)&&integer(v,"feature",&d->feature,3);
}
bool LayoutEditInspection_Handles(const char* op){return op&&(!strcmp(op,"save_measurement")||!strcmp(op,"remove_measurement")||!strcmp(op,"inspection_frame"));}
bool LayoutEditInspection_Apply(Layout* l,const cJSON* row,cJSON* result) {
    char op[64];if(!text(row,"operation",op,64,false))return false;(void)result;
    if(!strcmp(op,"inspection_frame")) {
        char id[64],frame[64];if(!text(row,"collection_id",id,64,false)||!text(row,"frame_entity",frame,64,false))return false;
        const LayoutSceneCollection* old=Layout_FindSceneCollection(&l->objectStore,id);PlaneFrame3 resolved;
        if(!old||Layout_InspectionFrame(l,frame,&resolved)!=LAYOUT_MEASUREMENT_OK)return false;
        LayoutSceneCollection c=*old;snprintf(c.inspection_frame,64,"%s",frame);return Layout_EditSceneCollection(l,&c,NULL,NULL,NULL);
    }
    if(!strcmp(op,"remove_measurement")){char id[64];return text(row,"id",id,64,false)&&Layout_SaveMeasurement(l,NULL,id,NULL,NULL);}
    LayoutSavedMeasurement m={0};if(!text(row,"id",m.id,64,false)||!text(row,"name",m.name,96,false)||!text(row,"collection_id",m.collection,64,false)||
        !text(row,"frame_entity",m.frame_entity,64,true)||!integer(row,"kind",&m.kind,6)||!datum(row,"a",&m.a,m.kind<3)||!datum(row,"b",&m.b,m.kind<3))return false;
    return Layout_SaveMeasurement(l,&m,NULL,NULL,NULL);
}
static void triple(cJSON* o,const char* key,const double v[3]){cJSON_AddItemToObject(o,key,cJSON_CreateDoubleArray(v,3));}
cJSON* LayoutEditInspection_Report(const Layout* l,const char* id) {
    const LayoutSceneCollection* c=Layout_FindSceneCollection(&l->objectStore,id);if(!c)return NULL;
    LayoutCollectionInspection r=Layout_InspectCollection(l,id,c->inspection_frame);cJSON* root=cJSON_CreateObject();if(!root)return NULL;
    cJSON_AddStringToObject(root,"schema","line_drawing_collection_inspection_v1");cJSON_AddStringToObject(root,"collection",id);cJSON_AddStringToObject(root,"name",c->name);cJSON_AddStringToObject(root,"frameEntity",c->inspection_frame);
    cJSON_AddStringToObject(root,"status",Layout_MeasurementStatusLabel(r.status));cJSON_AddStringToObject(root,"meaning",r.message);cJSON_AddStringToObject(root,"lengthUnit","m");
    cJSON_AddNumberToObject(root,"physicalParts",(double)r.parts);cJSON_AddNumberToObject(root,"meshEnvelopes",(double)r.mesh_parts);cJSON_AddNumberToObject(root,"excluded",(double)r.excluded);cJSON_AddNumberToObject(root,"unsupported",(double)r.unsupported);
    if(r.status==LAYOUT_MEASUREMENT_OK){triple(root,"frameMin_m",r.min_meters);triple(root,"frameMax_m",r.max_meters);triple(root,"worldMin_m",r.world_min_meters);triple(root,"worldMax_m",r.world_max_meters);}
    cJSON* parts=cJSON_AddArrayToObject(root,"parts");
    for(size_t i=0;i<l->objectStore.count;++i){const Object3D* o=&l->objectStore.items[i];if(!Layout_InspectionMember(l,id,o))continue;cJSON* row=cJSON_CreateObject();cJSON_AddItemToArray(parts,row);cJSON_AddStringToObject(row,"entity",o->coreMeta.object_id);cJSON_AddStringToObject(row,"name",o->info.label);cJSON_AddBoolToObject(row,"meshEnvelope",o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE);double size[3];if(Layout_InspectionLocalSize(l,o,size))triple(row,"localSize_m",size);}
    cJSON* measurements=cJSON_AddArrayToObject(root,"measurements");for(size_t i=0;i<l->objectStore.measurement_count;++i){const LayoutSavedMeasurement* m=&l->objectStore.measurements[i];if(strcmp(m->collection,id))continue;LayoutMeasurementResult v=Layout_EvaluateSavedMeasurement(l,m);cJSON* row=cJSON_CreateObject();cJSON_AddItemToArray(measurements,row);cJSON_AddStringToObject(row,"id",m->id);cJSON_AddStringToObject(row,"name",m->name);cJSON_AddStringToObject(row,"frameEntity",m->frame_entity);cJSON_AddStringToObject(row,"status",Layout_MeasurementStatusLabel(v.status));if(v.status==LAYOUT_MEASUREMENT_OK)cJSON_AddNumberToObject(row,"value_m",v.value);}
    return root;
}
