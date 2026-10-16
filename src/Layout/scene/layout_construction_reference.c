#include "Layout/layout_construction_reference.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void triple(cJSON* row,const char* key,const double v[3]){cJSON_AddItemToObject(row,key,cJSON_CreateDoubleArray(v,3));}
static void vector(cJSON* row,const char* key,Vec3 v,double scale){double n[]={v.x*scale,v.y*scale,v.z*scale};triple(row,key,n);}
static void properties(cJSON* row,const LayoutEntityInfo* info) {
    cJSON* list=cJSON_AddArrayToObject(row,"authoredProperties");
    for(size_t i=0;i<info->property_count;++i){const LayoutProperty* prop=&info->properties[i];cJSON* item=cJSON_CreateObject();cJSON_AddItemToArray(list,item);
        cJSON_AddStringToObject(item,"key",prop->key);cJSON_AddNumberToObject(item,"kind",prop->kind);
        if(prop->kind==LAYOUT_PROPERTY_TEXT)cJSON_AddStringToObject(item,"text",prop->text);else cJSON_AddNumberToObject(item,"value",prop->number);
        if(prop->kind==LAYOUT_PROPERTY_LENGTH)cJSON_AddStringToObject(item,"unit","m");
    }
}
cJSON* Layout_ConstructionReference(const Layout* l,const char* id) {
    if(!l||!id)return NULL;const LayoutSceneCollection* c=Layout_FindSceneCollection(&l->objectStore,id);if(!c)return NULL;
    LayoutCollectionInspection bounds=Layout_InspectCollection(l,id,c->inspection_frame);if(bounds.status!=LAYOUT_MEASUREMENT_OK)return NULL;
    cJSON* root=cJSON_CreateObject();if(!root)return NULL;
    cJSON_AddStringToObject(root,"schema","sculpt_construction_reference_v1");cJSON_AddStringToObject(root,"collection",id);cJSON_AddStringToObject(root,"name",c->name);
    cJSON_AddStringToObject(root,"lengthUnit","m");cJSON_AddStringToObject(root,"frameEntity",c->inspection_frame);
    cJSON_AddStringToObject(root,"qualification","Model reference - physical verification required before fabrication");
    cJSON_AddStringToObject(root,"pose","Current authored pose; temporary visibility and camera excluded");
    cJSON_AddStringToObject(root,"dimensionMeaning","Native primitive dimensions; STL instance envelopes are conservative. No stock/cut allowances inferred.");
    cJSON_AddStringToObject(root,"snapshot","source_snapshot.layout.json");
    cJSON_AddNumberToObject(root,"excluded",(double)bounds.excluded);
    vector(root,"frameOriginWorld_m",bounds.frame.origin,Layout_WorldScale(l));vector(root,"axisU",bounds.frame.axisU,1);vector(root,"axisV",bounds.frame.axisV,1);vector(root,"axisN",bounds.frame.normal,1);
    triple(root,"frameMin_m",bounds.min_meters);triple(root,"frameMax_m",bounds.max_meters);
    cJSON* parts=cJSON_AddArrayToObject(root,"parts");size_t number=0;Vec3 axes[]={bounds.frame.axisU,bounds.frame.axisV,bounds.frame.normal};
    for(size_t i=0;i<l->objectStore.count;++i) {
        const Object3D* o=&l->objectStore.items[i];if(!Layout_InspectionMember(l,id,o))continue;
        /* Duplicate entity IDs cannot support durable fabrication references. */
        if(Layout_InspectionObject(l,o->coreMeta.object_id)!=o)goto fail;
        double size[3];Vec3 corners[8];size_t count;if(!Layout_InspectionLocalSize(l,o,size)||!Layout_InspectionCorners(o,corners,&count))goto fail;
        cJSON* row=cJSON_CreateObject();cJSON_AddItemToArray(parts,row);cJSON_AddNumberToObject(row,"partNumber",(double)++number);
        cJSON_AddStringToObject(row,"entity",o->coreMeta.object_id);cJSON_AddStringToObject(row,"name",o->info.label[0]?o->info.label:o->coreMeta.object_id);
        cJSON_AddStringToObject(row,"assembly",o->info.parent_id);const LayoutAssembly* parent=NULL;
        for(size_t j=0;j<l->objectStore.assembly_count;++j)if(!strcmp(l->objectStore.assemblies[j].id,o->info.parent_id))parent=&l->objectStore.assemblies[j];
        cJSON_AddStringToObject(row,"assemblyName",parent?parent->info.label:"");cJSON_AddStringToObject(row,"entityType",o->info.entity_type);
        bool mesh=o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE;cJSON_AddBoolToObject(row,"meshEnvelope",mesh);cJSON_AddNumberToObject(row,"quantity",1);
        cJSON_AddStringToObject(row,"geometryBasis",mesh?"STL envelope":o->kind==OBJECT3D_KIND_PLANE?"Authored zero-thickness plane":"Authored solid prism");
        cJSON_AddStringToObject(row,"fabricationStatus","Unreviewed model part; stock, joint, grain, finish, tolerance and hardware fit unspecified");
        cJSON_AddStringToObject(row,"visualFinish",o->info.surface_material);properties(row,&o->info);triple(row,"localSize_m",size);
        cJSON* points=cJSON_AddArrayToObject(row,"cornersFrame_m");double lo[]={INFINITY,INFINITY,INFINITY},hi[]={-INFINITY,-INFINITY,-INFINITY};
        for(size_t j=0;j<count;++j){double point[3];for(int k=0;k<3;++k){point[k]=Vec3_Dot(Vec3_Sub(corners[j],bounds.frame.origin),axes[k])*Layout_WorldScale(l);if(!isfinite(point[k]))goto fail;lo[k]=fmin(lo[k],point[k]);hi[k]=fmax(hi[k],point[k]);}cJSON_AddItemToArray(points,cJSON_CreateDoubleArray(point,3));}
        triple(row,"frameMin_m",lo);triple(row,"frameMax_m",hi);
    }
    cJSON* measurements=cJSON_AddArrayToObject(root,"measurements");
    for(size_t i=0;i<l->objectStore.measurement_count;++i){const LayoutSavedMeasurement* m=&l->objectStore.measurements[i];if(strcmp(m->collection,id))continue;
        LayoutMeasurementResult value=Layout_EvaluateSavedMeasurement(l,m);cJSON* row=cJSON_CreateObject();cJSON_AddItemToArray(measurements,row);
        cJSON_AddStringToObject(row,"id",m->id);cJSON_AddStringToObject(row,"name",m->name);cJSON_AddStringToObject(row,"frameEntity",m->frame_entity);cJSON_AddNumberToObject(row,"kind",m->kind);
        cJSON_AddStringToObject(row,"datumA",m->a.entity_id);cJSON_AddNumberToObject(row,"featureA",m->a.feature);cJSON_AddStringToObject(row,"datumB",m->b.entity_id);cJSON_AddNumberToObject(row,"featureB",m->b.feature);
        cJSON_AddStringToObject(row,"status",Layout_MeasurementStatusLabel(value.status));if(value.status==LAYOUT_MEASUREMENT_OK)cJSON_AddNumberToObject(row,"value_m",value.value);
    }
    return root;
fail:cJSON_Delete(root);return NULL;
}
static bool save(const char* dir,const char* leaf,const char* contents) {
    char path[4096];if(snprintf(path,sizeof(path),"%s/%s",dir,leaf)>=(int)sizeof(path))return false;
    FILE* file=fopen(path,"wx");if(!file)return false;size_t length=strlen(contents);bool ok=fwrite(contents,1,length,file)==length;return fclose(file)==0&&ok;
}
bool Layout_ExportConstructionReference(const Layout* l,const char* id,const char* source,const char* dir,char* error,size_t capacity) {
    if(error&&capacity)snprintf(error,capacity,"Export refused: invalid/incomplete area or output already exists.");
    if(!dir||!dir[0]||strlen(dir)>3500)return false;
    cJSON* spec=Layout_ConstructionReference(l,id);char* snapshot=spec?Layout_SaveToString(l):NULL;if(!spec||!snapshot){cJSON_Delete(spec);free(snapshot);return false;}
    cJSON_AddStringToObject(spec,"sourceLabel",source?source:"");char* json=cJSON_Print(spec);
    if(!json||mkdir(dir,0700)){free(json);free(snapshot);cJSON_Delete(spec);return false;}
    bool ok=save(dir,"source_snapshot.layout.json",snapshot)&&save(dir,"specification.json",json)&&Layout_ReferenceSheetsWrite(spec,dir)&&save(dir,"COMPLETE.txt","sculpt_construction_reference_v1\nModel reference only. Snapshot and specifications share the current authored pose.\n");
    if(!ok){const char* files[]={"source_snapshot.layout.json","specification.json","reference_sheets.html","view_uv.svg","view_un.svg","view_vn.svg","COMPLETE.txt"};char path[4096];for(size_t i=0;i<sizeof(files)/sizeof(files[0]);++i){snprintf(path,sizeof(path),"%s/%s",dir,files[i]);(void)unlink(path);}(void)rmdir(dir);}
    if(error&&capacity)snprintf(error,capacity,"%s",ok?"Reference sheets exported. Open reference_sheets.html to view or print.":"Export failed; incomplete invocation output removed.");
    free(json);free(snapshot);cJSON_Delete(spec);return ok;
}
