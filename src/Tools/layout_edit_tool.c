/* Typed create-only edits to native documents. All failed batches stay in memory. */
#include "Layout/layout_json.h"
#include "Layout/layout_saved_views.h"
#include "Tools/layout_edit_motion.h"
#include "Tools/layout_edit_collections.h"
#include "Tools/layout_edit_scene_groups.h"
#include "Tools/layout_edit_inspection.h"
#include "Tools/layout_edit_mesh_fit.h"
#include "Layout/layout_construction_reference.h"
#include "Layout/layout_engineering.h"
#include "Layout/layout_furniture.h"
#include "Layout/scene/layout_camera_poses.h"
#include "Layout/layout_surface_material.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const cJSON *field(const cJSON *o, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(o,key);
}
static const char *string(const cJSON *o, const char *key) {
    const cJSON *v=field(o,key);return cJSON_IsString(v) ? v->valuestring : NULL;
}
static bool vector(const cJSON *o, const char *key, Vec3 *v) {
    const cJSON *a=field(o,key);
    if (!cJSON_IsArray(a) || cJSON_GetArraySize(a)!=3) return false;
    float values[3];
    for (int i=0;i<3;++i) {
        const cJSON *n=cJSON_GetArrayItem(a,i);
        if (!cJSON_IsNumber(n) || !isfinite(n->valuedouble) || fabs(n->valuedouble)>1e6) return false;
        values[i]=(float)n->valuedouble;
    }
    *v=(Vec3){values[0],values[1],values[2]};return true;
}
static Object3D *object(Layout *l, const char *id) {
    if (!id) return NULL;
    for (size_t i=0;i<l->objectStore.count;++i) {
        Object3D *o=&l->objectStore.items[i];
        if (!o->isDeleted && !strcmp(o->coreMeta.object_id,id)) return o;
    }
    return NULL;
}
static bool name(char *out, size_t capacity, const char *value) {
    if (!value || !value[0] || strlen(value)>=capacity) return false;
    snprintf(out,capacity,"%s",value);return true;
}
static bool finish(const cJSON *row, LayoutEntityInfo *info) {
    Vec3 rgb;
    const cJSON *rough=field(row,"roughness"),*reflect=field(row,"reflectivity");
    if (!vector(row,"rgb",&rgb) || rgb.x<0 || rgb.x>1 || rgb.y<0 || rgb.y>1 || rgb.z<0 || rgb.z>1 ||
        !cJSON_IsNumber(rough) || !cJSON_IsNumber(reflect)) return false;
    info->surface_custom=true;info->surface_material[0]=0;
    info->surface_rgb=((unsigned)lroundf(rgb.x*255)<<16) | ((unsigned)lroundf(rgb.y*255)<<8) | (unsigned)lroundf(rgb.z*255);
    info->surface_roughness=rough->valuedouble;info->surface_reflectivity=reflect->valuedouble;
    return Layout_SurfaceMaterialValid(info);
}
static bool apply(Layout *l, const cJSON *row, cJSON *result) {
    const char *op=string(row,"operation"),*id=string(row,"object_id");
    if (!op) return false;
    if(!strcmp(op,"replace_mesh_asset"))return LayoutEditMeshFit_Apply(l,row);
    if(LayoutEditInspection_Handles(op))return LayoutEditInspection_Apply(l,row,result);
    if(LayoutEditSceneGroups_Handles(op))return LayoutEditSceneGroups_Apply(l,row,result);
    if(LayoutEditCollections_Handles(op))return LayoutEditCollections_Apply(l,row,result);
    if (LayoutEditMotion_Handles(op)) return LayoutEditMotion_Apply(l,row,result);
    if(!strcmp(op,"set_visibility")) {
        const cJSON* visible=field(row,"visible");
        return cJSON_IsBool(visible) && Layout_SetAuthoredVisibility(l,string(row,"entity_id"),cJSON_IsTrue(visible),NULL,NULL);
    }
    Object3D *o=object(l,id);
    if (!strcmp(op,"delete_object")) return o && !o->coreMeta.flags.locked && Layout_ObjectStore_Delete(&l->objectStore,o->objectId);
    if (!strcmp(op,"object")) {
        if (!o || o->coreMeta.flags.locked) return false;
        uint32_t object_number=o->objectId;
        Vec3 dimensions,position;bool adjusted=false;
        if (field(row,"dimensions_m")) {
            if (!vector(row,"dimensions_m",&dimensions)) return false;
            double scale=Layout_WorldScale(l);
            if (!Layout_SetRectPrismDimensions(l,object_number,dimensions.x/scale,dimensions.y/scale,dimensions.z/scale,&adjusted) || adjusted) return false;
        }
        if (field(row,"position_m")) {
            if (!vector(row,"position_m",&position)) return false;
            position=Vec3_Scale(position,1/Layout_WorldScale(l));
            if (!Layout_SetObject3DPosition(l,object_number,position,&adjusted) || adjusted) return false;
        }
        o=Layout_ObjectStore_Find(&l->objectStore,object_number);
        const cJSON *visible=field(row,"visible");
        if (visible) {if (!cJSON_IsBool(visible) || !Layout_SetAuthoredVisibility(l,id,cJSON_IsTrue(visible),NULL,NULL)) return false;
            o=object(l,id);}
        if (field(row,"finish")) {
            LayoutEntityInfo info=o->info;
            if (!finish(field(row,"finish"),&info) || !Layout_SetEntityInfo(l,id,&info,NULL,NULL)) return false;
        }
        return true;
    }
    if (!strcmp(op,"add_inspection_camera") || !strcmp(op,"update_inspection_camera")) {
        Vec3 eye,target;size_t index;
        const cJSON *fov=field(row,"fov_deg");
        if (!vector(row,"eye_m",&eye) || !vector(row,"target_m",&target) ||
            Vec3_Distance(eye,target)<1e-6 || !cJSON_IsNumber(fov) ||
            !isfinite(fov->valuedouble) || fov->valuedouble<10 || fov->valuedouble>150) return false;
        LineDrawingScenePath *path=NULL;
        LineDrawingSceneCamera *camera=NULL;
        bool update=!strcmp(op,"update_inspection_camera");
        if (update) {
            const char *path_id=string(row,"path_id"),*camera_id=string(row,"camera_id");
            if (!path_id || !camera_id) return false;
            for (size_t i=0;i<l->sceneAuthoring.path_count;++i)
                if (!strcmp(l->sceneAuthoring.paths[i].path_id,path_id)) path=&l->sceneAuthoring.paths[i];
            for (size_t i=0;i<l->sceneAuthoring.camera_count;++i)
                if (!strcmp(l->sceneAuthoring.cameras[i].camera_id,camera_id)) camera=&l->sceneAuthoring.cameras[i];
            /* Refuse to replace a moving route or a mismatched binding. */
            if (!path || !camera || strcmp(path->bound_camera_id,camera_id) ||
                strcmp(camera->path_id,path_id) || path->role!=LINE_DRAWING_SCENE_PATH_ROLE_CAMERA ||
                path->bound_light_id[0] || path->closed || path->playing ||
                strcmp(path->curve_type,"linear") || path->control_point_count!=2 ||
                Vec3_Distance(path->control_points[0],path->control_points[1])*Layout_WorldScale(l)>1e-5)
                return false;
        } else {
            if (!Layout_SceneAuthoringState_AddDefaultCameraPath(&l->sceneAuthoring,&index)) return false;
            path=&l->sceneAuthoring.paths[index];
            camera=&l->sceneAuthoring.cameras[l->sceneAuthoring.camera_count-1];
        }
        if (!update || field(row,"name")) {
            if (!name(path->label,sizeof(path->label),string(row,"name")) ||
                !name(camera->label,sizeof(camera->label),string(row,"name"))) return false;
        }
        eye=Vec3_Scale(eye,1/Layout_WorldScale(l));target=Vec3_Scale(target,1/Layout_WorldScale(l));
        snprintf(path->curve_type,sizeof(path->curve_type),"linear");
        path->control_point_count=2;path->control_points[0]=eye;
        path->control_points[1]=Vec3_Add(eye,(Vec3){0,0,.000001f});
        path->duration_seconds=1;path->playing=false;
        path->closed=false;path->normalized_distance=0;
        path->playback_mode=LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE;
        /* Existing keys otherwise override the new look-at target and FOV. */
        path->key_count=0;memset(path->keys,0,sizeof(path->keys));
        camera->position=eye;camera->look_at_target=target;camera->roll_degrees=0;
        camera->orientation_mode=LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET;
        camera->vertical_fov_degrees=(float)fov->valuedouble;
        camera->near_clip=.01f/Layout_WorldScale(l);camera->far_clip=100/Layout_WorldScale(l);
        if (!CameraPoses_Enable(path,camera)) return false;
        cJSON_AddStringToObject(result,"camera_id",camera->camera_id);
        cJSON_AddStringToObject(result,"path_id",path->path_id);return true;
    }
    if (!strcmp(op,"configure_furniture_rail")) {
        const LayoutFurnitureUnit* old=Layout_FindFurnitureUnit(&l->objectStore,string(row,"assembly_id"));
        Vec3 offset,span;if(!old||!id||!vector(row,"offset_m",&offset)||!vector(row,"span_m",&span))return false;
        LayoutFurnitureUnit next=*old;size_t i=0;while(i<next.part_count&&strcmp(next.parts[i].entity_id,id))++i;
        if(i==next.part_count||next.parts[i].role!=LAYOUT_FURNITURE_RUN_RAIL)return false;
        next.parts[i].offset_m[0]=offset.x;next.parts[i].offset_m[1]=offset.y;next.parts[i].offset_m[2]=offset.z;
        next.parts[i].span_m[0]=span.x;next.parts[i].span_m[1]=span.y;next.parts[i].span_m[2]=span.z;
        return Layout_EditFurnitureUnit(l,&next,0,NULL,NULL);
    }
    if (!strcmp(op,"resize_furniture")) {
        const LayoutFurnitureUnit *old=Layout_FindFurnitureUnit(&l->objectStore,string(row,"assembly_id"));
        Vec3 size;
        if (!old || !vector(row,"size_m",&size)) return false;
        const cJSON *retain=field(row,"retain_run_end");
        int keep=0;
        if (retain) {
            if (!cJSON_IsNumber(retain) || !isfinite(retain->valuedouble) ||
                retain->valuedouble < -1 || retain->valuedouble > 1 ||
                floor(retain->valuedouble)!=retain->valuedouble) return false;
            keep=retain->valueint;
        }
        LayoutFurnitureUnit next=*old;
        next.size_m[0]=size.x;next.size_m[1]=size.y;next.size_m[2]=size.z;
        return Layout_EditFurnitureUnit(l,&next,keep,NULL,NULL);
    }
    if (!strcmp(op,"move_assembly")) {
        Vec3 delta,rotation;
        if (!vector(row,"translation_m",&delta) || !vector(row,"rotation_deg",&rotation)) return false;
        double translation[]={delta.x,delta.y,delta.z};
        return Layout_MoveAssembly(l,string(row,"assembly_id"),translation,rotation,NULL,NULL);
    }
    if (!strcmp(op,"add_prism")) {
        Vec3 dimensions,position;uint32_t number;bool adjusted=false;
        RectPrismPrimitiveCreateParams params;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
        if (!vector(row,"dimensions_m",&dimensions) || !vector(row,"position_m",&position)) return false;
        double scale=Layout_WorldScale(l);
        params.width=dimensions.x/scale;params.height=dimensions.y/scale;params.depth=dimensions.z/scale;
        params.useExplicitFrame=true;params.lockToBounds=false;params.lockToConstructionPlane=false;
        params.explicitFrame=(PlaneFrame3){.origin=Vec3_Scale(position,1/scale),
            .axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}};
        if (!Layout_CreateRectPrismPrimitive(l,&params,&number,&adjusted) || adjusted) return false;
        o=Layout_ObjectStore_Find(&l->objectStore,number);
        LayoutEntityInfo info=o->info;
        if (!name(info.label,sizeof(info.label),string(row,"name"))) return false;
        if (field(row,"finish") && !finish(field(row,"finish"),&info)) return false;
        char stable[64];snprintf(stable,sizeof(stable),"%s",o->coreMeta.object_id);
        if (!Layout_SetEntityInfo(l,stable,&info,NULL,NULL)) return false;
        cJSON_AddStringToObject(result,"object_id",stable);return true;
    }
    if (!strcmp(op,"set_parent")) {
        const char* id=string(row,"entity_id");o=NULL;
        if(!id) return false;
        for(size_t i=0;i<l->objectStore.count;++i) if(!strcmp(l->objectStore.items[i].coreMeta.object_id,id)) o=&l->objectStore.items[i];
        const char* parent=string(row,"parent_id");
        if(!o || !parent || strlen(parent)>=64 || !Layout_FindAssembly(&l->objectStore,parent)) return false;
        LayoutEntityInfo info=o->info;snprintf(info.parent_id,64,"%s",parent);
        return Layout_SetEntityInfo(l,o->coreMeta.object_id,&info,NULL,NULL);
    }
    if (!strcmp(op,"add_mesh_group")) {
        Vec3 position,rotation;
        const cJSON *parts=field(row,"parts");
        if (!vector(row,"position_m",&position) || !vector(row,"rotation_deg",&rotation) ||
            !cJSON_IsArray(parts) || cJSON_GetArraySize(parts)<1 || cJSON_GetArraySize(parts)>128) return false;
        LayoutAssembly assembly={.frame={.origin=Vec3_Scale(position,1/Layout_WorldScale(l)),
            .axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
        snprintf(assembly.info.entity_type,sizeof(assembly.info.entity_type),"Assembly");
        const char* reuse=string(row,"existing_assembly");
        if(reuse && (!Layout_FindAssembly(&l->objectStore,reuse) || rotation.x || rotation.y || rotation.z)) return false;
        if (!reuse && (!name(assembly.info.label,sizeof(assembly.info.label),string(row,"name")) ||
            !Layout_EditAssembly(l,&assembly,NULL,NULL,NULL))) {fprintf(stderr,"group assembly creation failed\n");return false;}
        char parent[64];snprintf(parent,sizeof(parent),"%s",reuse?reuse:l->objectStore.assemblies[l->objectStore.assembly_count-1].id);
        cJSON_AddStringToObject(result,"assembly_id",parent);
        cJSON *ids=cJSON_AddArrayToObject(result,"object_ids");
        const cJSON *part=NULL;
        cJSON_ArrayForEach(part,parts) {
            const char *runtime=string(part,"runtime_mesh");uint32_t number;char diagnostic[512];
            Transform3D transform=Layout_Transform3D_Default();
            transform.position=Vec3_Scale(position,1/Layout_WorldScale(l));
            transform.scale=(Vec3){1/Layout_WorldScale(l),1/Layout_WorldScale(l),1/Layout_WorldScale(l)};
            if (!runtime || runtime[0]!='/' || !Layout_CreateMeshAssetInstanceFromRuntimeAsset(l,runtime,&transform,&number,diagnostic,sizeof(diagnostic))) return false;
            o=Layout_ObjectStore_Find(&l->objectStore,number);
            LayoutEntityInfo info=o->info;
            if (!name(info.label,sizeof(info.label),string(part,"name")) || !finish(field(part,"finish"),&info)) return false;
            snprintf(info.parent_id,sizeof(info.parent_id),"%s",parent);
            char stable[64];snprintf(stable,sizeof(stable),"%s",o->coreMeta.object_id);
            if (!Layout_SetEntityInfo(l,stable,&info,NULL,NULL)) {fprintf(stderr,"part metadata failed: %s parent %s\n",stable,parent);return false;}
            cJSON_AddItemToArray(ids,cJSON_CreateString(stable));
        }
        double zero[]={0,0,0};
        return reuse || Layout_MoveAssembly(l,parent,zero,rotation,NULL,NULL);
    }
    return false;
}
int main(int argc, char **argv) {
    if(argc==5&&!strcmp(argv[1],"reference")){Layout l={0};Layout_Init(&l,.05f);char message[256];bool ok=Layout_LoadFromFile(&l,argv[2])&&Layout_ExportConstructionReference(&l,argv[3],argv[2],argv[4],message,sizeof(message));if(ok)puts(message);else fprintf(stderr,"Construction reference export refused. Use a valid area and NEW directory.\n");Layout_Free(&l);return ok?0:1;}
    bool report_mode=argc==4&&!strcmp(argv[1],"inspect");
    if(report_mode){Layout l={0};Layout_Init(&l,.05f);bool ok=Layout_LoadFromFile(&l,argv[2]);cJSON* result=ok?LayoutEditInspection_Report(&l,argv[3]):NULL;char* output=result?cJSON_Print(result):NULL;bool printed=output!=NULL;if(output){puts(output);free(output);}else fprintf(stderr,"Cannot inspect that collection.\n");cJSON_Delete(result);Layout_Free(&l);return printed?0:1;}
    bool inspection=argc==4&&!strcmp(argv[1],"visibility");
    if (!inspection && (argc!=5 || strcmp(argv[1],"edit"))) {
        fprintf(stderr,"usage: layout_edit_tool edit INPUT.layout.json COMMAND.json NEW.layout.json\n");return 1;
    }
    FILE *f=fopen(argv[3],"rb");char *text=NULL;
    if (f) {
        if (!fseek(f,0,SEEK_END)) {
            long size=ftell(f);
            if (size>0 && size<=4*1024*1024 && !fseek(f,0,SEEK_SET)) {
                text=malloc((size_t)size+1);
                if (text && fread(text,1,(size_t)size,f)==(size_t)size) text[size]=0;
                else {free(text);text=NULL;}
            }
        }
        fclose(f);
    }
    cJSON *command=text ? cJSON_Parse(text) : NULL;free(text);
    const char *schema=string(command,"schema");const cJSON *ops=field(command,"operations");
    Layout layout={0},reloaded={0};Layout_Init(&layout,.05f);Layout_Init(&reloaded,.05f);
    if(inspection){
        cJSON* report=cJSON_CreateArray();bool ok=Layout_LoadFromFile(&layout,argv[2])&&LayoutEditCollections_Inspect(&layout,command,report);
        if(ok){char* output=cJSON_Print(report);if(output){puts(output);free(output);}else ok=false;}
        else fprintf(stderr,"Invalid transient collection inspection; no scene written.\n");
        cJSON_Delete(report);cJSON_Delete(command);Layout_Free(&layout);Layout_Free(&reloaded);return ok?0:1;
    }
    bool ok=schema && !strcmp(schema,"line_drawing_layout_edit_v1") && cJSON_IsArray(ops) &&
        cJSON_GetArraySize(ops)>0 && cJSON_GetArraySize(ops)<=256 && Layout_LoadFromFile(&layout,argv[2]);
    cJSON *report=cJSON_CreateArray();const cJSON *row=NULL;
    cJSON_ArrayForEach(row,ops) {
        if (!ok) break;
        cJSON *result=cJSON_CreateObject();cJSON_AddItemToArray(report,result);
        ok=result && apply(&layout,row,result);
        if (!ok) fprintf(stderr,"Operation %d (%s): %s\n",cJSON_GetArraySize(report),
            string(row,"operation"),layout.geometryMessage);
    }
    char *saved=ok ? Layout_SaveToString(&layout) : NULL;
    ok=saved && Layout_LoadFromString(&reloaded,saved);
    FILE *output=ok ? fopen(argv[4],"wx") : NULL;
    ok=output && fputs(saved,output)>=0;
    if (output && fclose(output)) ok=false;
    if (output && !ok) (void)remove(argv[4]);
    Layout_FreeString(saved);Layout_Free(&layout);Layout_Free(&reloaded);cJSON_Delete(command);
    if (ok) {text=cJSON_Print(report);if(text) {puts(text);free(text);}}
    else fprintf(stderr,"Invalid edit, constrained/locked geometry, invalid mesh/finish, or existing output; no candidate published.\n");
    cJSON_Delete(report);return ok ? 0 : 1;
}
