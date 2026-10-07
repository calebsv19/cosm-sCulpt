#include "Layout/layout_inventory.h"
#include "Layout/layout_routes.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* text_value(const LayoutEntityInfo* info, const char* key) {
    for (size_t i=0;i<info->property_count;++i)
        if (!strcmp(info->properties[i].key,key) && info->properties[i].kind==LAYOUT_PROPERTY_TEXT)
            return info->properties[i].text;
    return "";
}
static double number_value(const LayoutEntityInfo* info, const char* key) {
    for (size_t i=0;i<info->property_count;++i)
        if (!strcmp(info->properties[i].key,key) && info->properties[i].kind==LAYOUT_PROPERTY_NUMBER)
            return info->properties[i].number;
    return 0;
}
static cJSON* record(const char* id, const LayoutEntityInfo* info, const char* kind) {
    cJSON* row=cJSON_CreateObject();
    if (!row) return NULL;
    cJSON_AddStringToObject(row,"entity_id",id);
    cJSON_AddStringToObject(row,"record_kind",kind);
    cJSON_AddStringToObject(row,"name",info->label);
    cJSON_AddStringToObject(row,"entity_type",Layout_EntityType(info));
    cJSON_AddStringToObject(row,"assembly_id",info->parent_id);
    const char* keys[]={"subsystem","part_number","status"};
    for(size_t i=0;i<3;++i)cJSON_AddStringToObject(row,keys[i],text_value(info,keys[i]));
    return row;
}
static void number(cJSON* row,const char* key,double value) {
    if(value>0)cJSON_AddNumberToObject(row,key,value);
    else cJSON_AddNullToObject(row,key);
}
cJSON* Layout_InventoryJson(const Layout* l) {
    if(!l)return NULL;
    cJSON* root=cJSON_CreateObject();
    cJSON_AddStringToObject(root,"schema","line_drawing_inventory_v1");
    cJSON_AddStringToObject(root,"unit","m");
    cJSON_AddStringToObject(root,"load_basis","explicit_assumptions_no_network_aggregation");
    cJSON* rows=cJSON_AddArrayToObject(root,"items");
    for(size_t i=0;i<l->objectStore.count;++i) {
        const Object3D* o=&l->objectStore.items[i];if(o->isDeleted)continue;
        cJSON* row=record(o->coreMeta.object_id,&o->info,"object");
        if(o->kind==OBJECT3D_KIND_RECT_PRISM) {
            double scale=Layout_WorldScale(l);
            double dims[]={o->rectPrism.width*o->transform.scale.x*scale,
                o->rectPrism.height*o->transform.scale.y*scale,o->rectPrism.depth*o->transform.scale.z*scale};
            cJSON_AddItemToObject(row,"dimensions_m",cJSON_CreateDoubleArray(dims,3));
            double volume = dims[0]*dims[1]*dims[2];
            if (o->rectPrism.opening.enabled) {
                const LayoutPanelOpening* f=&o->rectPrism.opening;
                double removed=f->width*f->height*(o->rectPrism.depth-f->floor)*scale*scale*scale*
                    o->transform.scale.x*o->transform.scale.y*o->transform.scale.z;
                volume-=removed;
                cJSON_AddNumberToObject(row,"opening_removed_volume_m3",removed);
            }
            cJSON_AddNumberToObject(row,"material_volume_m3",volume);
        }
        cJSON_AddStringToObject(row,"power_domain",text_value(&o->info,"power_domain"));
        number(row,"nominal_volts",number_value(&o->info,"nominal_volts"));
        number(row,"design_amps",number_value(&o->info,"design_amps"));
        cJSON_AddItemToArray(rows,row);
    }
    for(size_t i=0;i<l->objectStore.assembly_count;++i) {
        const LayoutAssembly* a=&l->objectStore.assemblies[i];
        cJSON_AddItemToArray(rows,record(a->id,&a->info,"assembly"));
    }
    for(size_t i=0;i<l->objectStore.route_count;++i) {
        const LayoutPhysicalRoute* r=&l->objectStore.routes[i];
        cJSON* row=record(r->id,&r->info,"route");
        cJSON_AddStringToObject(row,"power_domain",r->electrical.power_domain);
        number(row,"nominal_volts",r->electrical.nominal_volts);
        number(row,"design_amps",r->electrical.design_amps);
        cJSON_AddNumberToObject(row,"length_m",Layout_RouteLength(r));
        cJSON_AddItemToArray(rows,row);
    }
    return root;
}
static bool property(LayoutEntityInfo* info,const char* key,const cJSON* value,bool numeric) {
    size_t index=info->property_count;
    for(size_t i=0;i<info->property_count;++i)if(!strcmp(info->properties[i].key,key))index=i;
    bool remove=numeric?cJSON_IsNull(value):(cJSON_IsString(value) && !value->valuestring[0]);
    if(remove) {
        if(index<info->property_count) {
            memmove(&info->properties[index],&info->properties[index+1],(info->property_count-index-1)*sizeof(LayoutProperty));
            memset(&info->properties[--info->property_count],0,sizeof(LayoutProperty));
        }
        return true;
    }
    if(index==info->property_count) {
        if(index==LAYOUT_MAX_PROPERTIES)return false;
        ++info->property_count;
    }
    LayoutProperty* p=&info->properties[index];memset(p,0,sizeof(*p));
    snprintf(p->key,sizeof(p->key),"%s",key);
    p->kind=numeric?LAYOUT_PROPERTY_NUMBER:LAYOUT_PROPERTY_TEXT;
    if(numeric)p->number=value->valuedouble;
    else snprintf(p->text,sizeof(p->text),"%s",value->valuestring);
    return true;
}
static bool patch(Layout* l,void* context) {
    const cJSON* items=context;
    if(!cJSON_IsArray(items) || cJSON_GetArraySize(items)>1024)return false;
    const cJSON* row;
    cJSON_ArrayForEach(row,items) {
        const cJSON* id=cJSON_GetObjectItemCaseSensitive(row,"entity_id");
        if(!cJSON_IsObject(row) || !cJSON_IsString(id) || strlen(id->valuestring)>=64)return false;
        for(const cJSON* earlier=items->child;earlier!=row;earlier=earlier->next) {
            const cJSON* other=cJSON_GetObjectItemCaseSensitive(earlier,"entity_id");
            if(cJSON_IsString(other) && !strcmp(id->valuestring,other->valuestring))return false;
        }
        LayoutPhysicalRoute* route=(LayoutPhysicalRoute*)Layout_FindRoute(&l->objectStore,id->valuestring);
        LayoutEntityInfo* info=route?&route->info:(LayoutEntityInfo*)Layout_EntityInfo(&l->objectStore,id->valuestring);
        if(!info)return false;
        for(const cJSON* v=row->child;v;v=v->next) {
            if(!strcmp(v->string,"entity_id"))continue;
            bool name=!strcmp(v->string,"name"), numeric=!strcmp(v->string,"nominal_volts") || !strcmp(v->string,"design_amps");
            bool domain=!strcmp(v->string,"power_domain");
            bool text=!strcmp(v->string,"subsystem") || !strcmp(v->string,"part_number") || !strcmp(v->string,"status");
            if(numeric) {
                if(!cJSON_IsNull(v) && (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<=0 || v->valuedouble>1e9))return false;
                if(route) {
                    double value=cJSON_IsNull(v)?0:v->valuedouble;
                    if(!strcmp(v->string,"nominal_volts"))route->electrical.nominal_volts=value;
                    else route->electrical.design_amps=value;
                } else if(!property(info,v->string,v,true))return false;
            } else if(name || domain || text) {
                size_t capacity=name?96:domain?64:128;
                if(!cJSON_IsString(v) || strlen(v->valuestring)>=capacity)return false;
                if(name)snprintf(info->label,sizeof(info->label),"%s",v->valuestring);
                else if(domain && route)snprintf(route->electrical.power_domain,sizeof(route->electrical.power_domain),"%s",v->valuestring);
                else if(!property(info,v->string,v,false))return false;
            } else return false; /* Read-only observed columns are not patches. */
        }
    }
    return true;
}
bool Layout_EditInventory(Layout* l,const cJSON* items,LayoutGeometryBeforePublish history,void* context) {
    if(!l)return false;
    bool ok=Layout_RunGeometryEdit(l,0,patch,(void*)items,history,context);
    if(!ok)snprintf(l->geometryMessage,sizeof(l->geometryMessage),"Inventory rejected: use unique existing IDs, editable fields, finite positive values or null, and available metadata slots.");
    return ok;
}
