#include "Layout/layout_spatial.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fail(Layout* l, const char* text) {
    snprintf(l->geometryMessage,sizeof(l->geometryMessage),"%s",text);return false;
}
static bool string_valid(const char* s, size_t n) {return memchr(s,0,n)!=NULL;}
const LayoutSpatialRule* Layout_FindSpatialRule(const LayoutObjectStore* s, const char* id) {
    if (!s || !id) return NULL;
    for (size_t i=0;i<s->spatial_rule_count && i<LAYOUT_MAX_SPATIAL_RULES;++i)
        if (!strcmp(s->spatial_rules[i].id,id)) return &s->spatial_rules[i];
    return NULL;
}
bool Layout_SpatialEntityReferenced(const LayoutObjectStore* s, const char* id) {
    if (!s || !id) return false;
    for (size_t i=0;i<s->spatial_rule_count && i<LAYOUT_MAX_SPATIAL_RULES;++i)
        if (!strcmp(s->spatial_rules[i].source,id) || !strcmp(s->spatial_rules[i].target,id)) return true;
    for (size_t i=0;i<s->count;++i)
        if (!s->items[i].isDeleted && !strcmp(s->items[i].info.volume_owner,id)) return true;
    return false;
}
bool Layout_ValidateSpatialRecords(const Layout* l, char* message, size_t capacity) {
    if (!l) return false;
    const LayoutObjectStore* s=&l->objectStore;const char* reason=NULL;
    for (size_t i=0;!reason && i<s->assembly_count;++i)
        if (s->assemblies[i].info.volume_role || s->assemblies[i].info.volume_owner[0]) reason="Reserved roles belong to prism objects, not assemblies.";
    for (size_t i=0;!reason && i<s->count;++i) {
        const Object3D* o=&s->items[i];const LayoutEntityInfo* v=&o->info;
        if (o->isDeleted) continue;
        if (v->volume_role<LAYOUT_VOLUME_NONE || v->volume_role>LAYOUT_VOLUME_SERVICE || !string_valid(v->volume_owner,64)) reason="Invalid volume role or owner.";
        else if (v->volume_role && (o->kind!=OBJECT3D_KIND_RECT_PRISM || v->reference || !isfinite(o->rectPrism.width) || !isfinite(o->rectPrism.height) || !isfinite(o->rectPrism.depth) || o->rectPrism.depth<=0)) reason="Reserved volumes require a design prism.";
        else if (v->volume_owner[0] && (!v->volume_role || !Layout_EntityInfo(s,v->volume_owner) || !strcmp(v->volume_owner,o->coreMeta.object_id))) reason="Volume owner must be a different existing object or assembly.";
    }
    if (s->spatial_rule_count>LAYOUT_MAX_SPATIAL_RULES) reason="A scene supports up to 32 spatial checks.";
    for (size_t i=0;!reason && i<s->spatial_rule_count;++i) {
        const LayoutSpatialRule* c=&s->spatial_rules[i];
        if (!string_valid(c->id,64) || !string_valid(c->source,64) || !string_valid(c->target,64) || !c->id[0] ||
            c->kind<LAYOUT_SPATIAL_NO_INTERSECTION || c->kind>LAYOUT_SPATIAL_CLEARANCE || !isfinite(c->clearance_meters) || c->clearance_meters<0 ||
            (c->kind==LAYOUT_SPATIAL_NO_INTERSECTION && c->clearance_meters!=0)) {reason="Invalid spatial check record.";break;}
        for (const unsigned char* p=(const unsigned char*)c->id;*p;++p)
            if (!isalnum(*p) && *p!='_' && *p!='.' && *p!='-') reason="Invalid spatial check ID.";
        if (!Layout_EntityInfo(s,c->source) || !Layout_EntityInfo(s,c->target) || !strcmp(c->source,c->target)) reason="Checks require two different existing entities.";
        for (size_t j=0;j<i;++j) {
            const LayoutSpatialRule* old=&s->spatial_rules[j];
            if (!strcmp(old->id,c->id)) reason="Spatial check IDs must be unique.";
            if (old->kind==c->kind && ((!strcmp(old->source,c->source) && !strcmp(old->target,c->target)) ||
                (!strcmp(old->target,c->source) && !strcmp(old->source,c->target)))) reason="This pair already has that check.";
        }
    }
    if (reason && message && capacity) snprintf(message,capacity,"%s",reason);
    return !reason;
}
typedef struct {const LayoutVolumeEdit* v;bool remove;} VolumeCommand;
static bool volume_edit(Layout* l, void* context) {
    VolumeCommand* cmd=context;const LayoutVolumeEdit* v=cmd->v;
    Object3D* o=Layout_ObjectStore_Find(&l->objectStore,v->object_id);
    if (cmd->remove) {
        if (!o || !o->info.volume_role || !Layout_CanDeleteObject(&l->objectStore,v->object_id)) return fail(l,"Remove volume links and checks before deleting it.");
        return Layout_ObjectStore_Delete(&l->objectStore,v->object_id);
    }
    if (!string_valid(v->name,96) || !v->name[0] || !string_valid(v->owner,64) || v->role<LAYOUT_VOLUME_KEEPOUT || v->role>LAYOUT_VOLUME_SERVICE) return fail(l,"Enter a name and reserved-space role.");
    float size[3],position[3];double scale=Layout_WorldScale(l);
    if (!isfinite(scale) || scale<=0) return false;
    for (int k=0;k<3;++k) {
        double d=v->size_meters[k]/scale,p=v->center_meters[k]/scale;
        if (!isfinite(d) || !isfinite(p) || d<Layout_PlanePrimitiveMinSize() || d>FLT_MAX || fabs(p)>FLT_MAX ||
            fabs((double)(float)p-p)*scale>1e-6+fabs(v->center_meters[k])*1e-7 || fabs((double)(float)d-d)*scale>1e-6+fabs(v->size_meters[k])*1e-7)
            return fail(l,"Enter positive dimensions and finite positions within geometry precision.");
        size[k]=(float)d;position[k]=(float)p;
    }
    if (!o) {
        if (v->object_id) return fail(l,"Volume no longer exists.");
        RectPrismPrimitiveCreateParams p={.width=size[0],.height=size[1],.depth=size[2],.useExplicitFrame=true,
            .explicitFrame={.origin={position[0],position[1],position[2]},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
        uint32_t id=0;bool adjusted=false;
        if (!Layout_CreateRectPrismPrimitive(l,&p,&id,&adjusted) || adjusted) return fail(l,"Volume conflicts with scene bounds.");
        o=Layout_ObjectStore_Find(&l->objectStore,id);
    } else {
        if (!o->info.volume_role) return fail(l,"Choose a reserved volume to edit.");
        bool adjusted=false;
        if (!Layout_SetRectPrismDimensions(l,o->objectId,size[0],size[1],size[2],&adjusted) || adjusted ||
            !Layout_SetObject3DPosition(l,o->objectId,(Vec3){position[0],position[1],position[2]},&adjusted) || adjusted) return fail(l,"Volume edit conflicts with geometry locks or bounds.");
    }
    snprintf(o->info.label,96,"%s",v->name);snprintf(o->info.volume_owner,64,"%s",v->owner);
    o->info.volume_role=v->role;
    snprintf(o->info.entity_type,64,"%s",v->role==LAYOUT_VOLUME_SERVICE?"ServiceVolume":"KeepoutVolume");
    return true;
}
bool Layout_EditVolume(Layout* l, const LayoutVolumeEdit* v, bool remove, LayoutGeometryBeforePublish history, void* context) {
    if (!l || !v) return false;
    VolumeCommand cmd={v,remove};return Layout_RunGeometryEdit(l,v->object_id,volume_edit,&cmd,history,context);
}
typedef struct {const LayoutSpatialRule* rule;const char* remove;} RuleCommand;
static bool rule_edit(Layout* l, void* context) {
    RuleCommand* cmd=context;LayoutObjectStore* s=&l->objectStore;
    if (cmd->remove) {
        const LayoutSpatialRule* saved=Layout_FindSpatialRule(s,cmd->remove);
        if (!saved) return fail(l,"Check no longer exists.");
        size_t i=(size_t)(saved-s->spatial_rules);
        memmove(&s->spatial_rules[i],&s->spatial_rules[i+1],(s->spatial_rule_count-i-1)*sizeof(LayoutSpatialRule));
        memset(&s->spatial_rules[--s->spatial_rule_count],0,sizeof(LayoutSpatialRule));return true;
    }
    LayoutSpatialRule next=*cmd->rule;
    if (!string_valid(next.id,64)) return false;
    LayoutSpatialRule* saved=(LayoutSpatialRule*)Layout_FindSpatialRule(s,next.id);
    if (saved) {*saved=next;return true;}
    if (next.id[0]) return fail(l,"Choose an existing check to update.");
    if (s->spatial_rule_count==LAYOUT_MAX_SPATIAL_RULES) return fail(l,"A scene supports up to 32 spatial checks.");
    uint32_t n=s->next_spatial_rule_id?s->next_spatial_rule_id:1;
    do {if(n==UINT32_MAX)return false;snprintf(next.id,64,"spatial_%u",n++);} while(Layout_FindSpatialRule(s,next.id));
    s->next_spatial_rule_id=n;s->spatial_rules[s->spatial_rule_count++]=next;return true;
}
bool Layout_EditSpatialRule(Layout* l, const LayoutSpatialRule* rule, const char* remove, LayoutGeometryBeforePublish history, void* context) {
    if (!l || (!rule && !remove)) return false;
    RuleCommand cmd={rule,remove};return Layout_RunGeometryEdit(l,0,rule_edit,&cmd,history,context);
}
bool Layout_SpatialWriteJson(const Layout* l, cJSON* engineering) {
    cJSON* array=cJSON_AddArrayToObject(engineering,"spatialChecks");
    if (!array || !cJSON_AddNumberToObject(engineering,"nextSpatialCheckId",l->objectStore.next_spatial_rule_id?l->objectStore.next_spatial_rule_id:1)) return false;
    for (size_t i=0;i<l->objectStore.spatial_rule_count;++i) {
        const LayoutSpatialRule* r=&l->objectStore.spatial_rules[i];cJSON* item=cJSON_CreateObject();
        if (!item) return false;
        cJSON_AddItemToArray(array,item);
        if (!cJSON_AddStringToObject(item,"id",r->id) || !cJSON_AddStringToObject(item,"source",r->source) || !cJSON_AddStringToObject(item,"target",r->target) ||
            !cJSON_AddStringToObject(item,"type",r->kind==LAYOUT_SPATIAL_CLEARANCE?"minimum_clearance":"must_not_intersect") || !cJSON_AddNumberToObject(item,"clearanceMeters",r->clearance_meters)) return false;
    }
    return true;
}
static bool read_string(const cJSON* o, const char* key, char* value, size_t capacity) {
    const cJSON* n=cJSON_GetObjectItemCaseSensitive(o,key);
    if (!cJSON_IsString(n) || !n->valuestring || strlen(n->valuestring)>=capacity) return false;
    snprintf(value,capacity,"%s",n->valuestring);return true;
}
bool Layout_SpatialReadJson(Layout* l, const cJSON* engineering, bool required) {
    const cJSON* array=cJSON_GetObjectItemCaseSensitive(engineering,"spatialChecks"),*next=cJSON_GetObjectItemCaseSensitive(engineering,"nextSpatialCheckId");
    if (required) {
        const cJSON* entities=cJSON_GetObjectItemCaseSensitive(engineering,"entities");
        for (int i=0;i<cJSON_GetArraySize(entities);++i) {
            const cJSON* o=cJSON_GetArrayItem(entities,i);
            if (!cJSON_HasObjectItem(o,"volumeRole") || !cJSON_HasObjectItem(o,"volumeOwner")) return false;
        }
    } else for (size_t i=0;i<l->objectStore.count;++i)
        if (l->objectStore.items[i].info.volume_role || l->objectStore.items[i].info.volume_owner[0]) return false;
    if (!array && !next) return !required && Layout_ValidateSpatialRecords(l,NULL,0);
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array)>LAYOUT_MAX_SPATIAL_RULES || !cJSON_IsNumber(next) || !isfinite(next->valuedouble) ||
        next->valuedouble<1 || next->valuedouble>UINT32_MAX || floor(next->valuedouble)!=next->valuedouble || (!required && cJSON_GetArraySize(array))) return false;
    l->objectStore.next_spatial_rule_id=(uint32_t)next->valuedouble;l->objectStore.spatial_rule_count=(size_t)cJSON_GetArraySize(array);
    for (size_t i=0;i<l->objectStore.spatial_rule_count;++i) {
        LayoutSpatialRule* r=&l->objectStore.spatial_rules[i];const cJSON* o=cJSON_GetArrayItem(array,(int)i);char type[64];
        if (!read_string(o,"id",r->id,64) || !read_string(o,"source",r->source,64) || !read_string(o,"target",r->target,64) || !read_string(o,"type",type,64)) return false;
        if (!strcmp(type,"minimum_clearance"))r->kind=LAYOUT_SPATIAL_CLEARANCE;
        else if (!strcmp(type,"must_not_intersect"))r->kind=LAYOUT_SPATIAL_NO_INTERSECTION;
        else return false;
        const cJSON* n=cJSON_GetObjectItemCaseSensitive(o,"clearanceMeters");if(!cJSON_IsNumber(n))return false;r->clearance_meters=n->valuedouble;
    }
    return Layout_ValidateSpatialRecords(l,NULL,0);
}
