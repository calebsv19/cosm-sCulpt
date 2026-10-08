#include "Tools/canonical_scene_export_surfaces.h"
#include "Layout/layout_surface_material.h"
#include <stdio.h>
#include <string.h>

static cJSON *member(cJSON *parent,const char *key,bool array) {
    cJSON *n=cJSON_GetObjectItemCaseSensitive(parent,key);
    if(n) return (array ? cJSON_IsArray(n) : cJSON_IsObject(n)) ? n : NULL;
    return array ? cJSON_AddArrayToObject(parent,key) : cJSON_AddObjectToObject(parent,key);
}
static void replace_string(cJSON *n,const char *key,const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(n,key);cJSON_AddStringToObject(n,key,value);
}
static cJSON *find(cJSON *array,const char *key,const char *id) {
    cJSON *n=NULL;
    cJSON_ArrayForEach(n,array) {
        const cJSON *v=cJSON_GetObjectItemCaseSensitive(n,key);
        if(cJSON_IsString(v) && !strcmp(v->valuestring,id)) return n;
    }
    return NULL;
}
bool LineDrawingCanonicalScene_ApplySurfaceMaterials(cJSON *root,const Layout *layout) {
    cJSON *objects=cJSON_GetObjectItemCaseSensitive(root,"objects");
    cJSON *materials=cJSON_GetObjectItemCaseSensitive(root,"materials");
    cJSON *bindings=cJSON_GetObjectItemCaseSensitive(root,"material_bindings");
    if(!cJSON_IsArray(objects) || !cJSON_IsArray(materials) || !cJSON_IsArray(bindings)) return false;
    for(size_t i=0;i<layout->objectStore.count;++i) {
        const Object3D *o=&layout->objectStore.items[i];
        if(o->isDeleted || Layout_EntityIsSpatialGuide(&o->info)) continue;
        const char *id=Layout_SurfaceMaterialId(&o->info);
        if(!id[0]) continue;
        const LayoutSurfaceMaterial *p=Layout_SurfaceMaterialFind(id);
        cJSON *object=find(objects,"object_id",o->coreMeta.object_id);
        if(!p || !object) return false;
        char material_id[96];snprintf(material_id,sizeof(material_id),"ld_surface_%s",id);
        cJSON *material=find(materials,"material_id",material_id);
        if(!material) {
            material=cJSON_CreateObject();if(!material) return false;
            cJSON_AddItemToArray(materials,material);
            cJSON_AddStringToObject(material,"material_id",material_id);
            cJSON_AddStringToObject(material,"material_type","flat_color");
            cJSON_AddStringToObject(material,"label",p->label);
            double rgba[]={((p->rgb>>16)&255)/255.,((p->rgb>>8)&255)/255.,(p->rgb&255)/255.,1};
            cJSON_AddItemToObject(material,"rgba",cJSON_CreateDoubleArray(rgba,4));
            cJSON_AddNumberToObject(material,"roughness",p->roughness);
        }
        cJSON *reference=cJSON_GetObjectItemCaseSensitive(object,"material_ref");
        if(!cJSON_IsObject(reference)) return false;
        replace_string(reference,"id",material_id);
        cJSON *binding=NULL;
        cJSON_ArrayForEach(binding,bindings) {
            const cJSON *object_id=cJSON_GetObjectItemCaseSensitive(binding,"object_id");
            const cJSON *kind=cJSON_GetObjectItemCaseSensitive(binding,"target_kind");
            if(cJSON_IsString(object_id) && !strcmp(object_id->valuestring,o->coreMeta.object_id) &&
                cJSON_IsString(kind) && !strcmp(kind->valuestring,"object")) replace_string(binding,"material_id",material_id);
        }
        cJSON *extensions=member(root,"extensions",false);
        cJSON *ray=member(extensions,"ray_tracing",false);
        cJSON *authoring=member(ray,"authoring",false);
        cJSON *rows=member(authoring,"object_materials",true);
        if(!rows) return false;
        cJSON *row=find(rows,"object_id",o->coreMeta.object_id);
        /* Explicit shader graphs require a separate edit; never silently discard them. */
        if(row && (cJSON_GetObjectItemCaseSensitive(row,"surface_graph") ||
            cJSON_GetObjectItemCaseSensitive(row,"material_graph") ||
            cJSON_GetObjectItemCaseSensitive(row,"surface_material_binding"))) return false;
        if(!row) {
            row=cJSON_CreateObject();if(!row) return false;
            cJSON_AddItemToArray(rows,row);
        }
        const char *controlled[]={"object_id","material_id","object_color","roughness","reflectivity",
            "material_texture_stack","materialTextureStack"};
        for(size_t k=0;k<sizeof(controlled)/sizeof(*controlled);++k)
            cJSON_DeleteItemFromObjectCaseSensitive(row,controlled[k]);
        cJSON_AddStringToObject(row,"object_id",o->coreMeta.object_id);
        cJSON_AddNumberToObject(row,"material_id",0);
        cJSON_AddNumberToObject(row,"object_color",p->rgb);
        cJSON_AddNumberToObject(row,"roughness",p->roughness);
        cJSON_AddNumberToObject(row,"reflectivity",p->reflectivity);
        cJSON *stack=cJSON_AddObjectToObject(row,"material_texture_stack");
        cJSON *layers=cJSON_AddArrayToObject(stack,"layers");
        /* Solid finishes use only the base color; procedural bases use existing optiC kinds. */
        if(strcmp(p->texture,"solid")) {
            cJSON *layer=cJSON_CreateObject();if(!layer) return false;
            cJSON_AddItemToArray(layers,layer);
            cJSON_AddStringToObject(layer,"id","ld_surface_base");
            cJSON_AddStringToObject(layer,"name",p->label);
            cJSON_AddStringToObject(layer,"kind",p->texture);
            cJSON_AddStringToObject(layer,"blend","replace");
            cJSON_AddBoolToObject(layer,"enabled",true);
            cJSON_AddNumberToObject(layer,"opacity",1);
            cJSON *placement=cJSON_AddObjectToObject(layer,"placement");
            /* Texture coordinates multiply by scale: finer wood at van dimensions. */
            cJSON_AddNumberToObject(placement,"scale",!strcmp(p->texture,"wood") ? 8 : 1);
            cJSON_AddNumberToObject(placement,"strength",1);
            cJSON *params=cJSON_AddObjectToObject(layer,"parameters");
            cJSON_AddNumberToObject(params,"seed",131);
            cJSON_AddNumberToObject(params,"grain",.58);
            cJSON_AddNumberToObject(params,"contrast",.40);
            cJSON_AddNumberToObject(params,"color_depth",.50);
        }
    }
    return true;
}
