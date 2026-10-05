#include "Layout/layout_engineering.h"
#include "Layout/layout_relationships.h"
#include "Layout/layout_spatial.h"
#include "Layout/layout_motion.h"
#include "Layout/layout_routes.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool refuse(Layout* layout, const char* message) {
    snprintf(layout->geometryMessage,sizeof(layout->geometryMessage),"%s",message);
    return false;
}
static bool bounded(const char* text, size_t capacity) {
    return memchr(text,0,capacity)!=NULL;
}
static bool key_valid(const char* text, size_t capacity) {
    if (!bounded(text,capacity) || !text[0]) return false;
    for (const unsigned char* p=(const unsigned char*)text; *p; ++p)
        if (!isalnum(*p) && *p!='_' && *p!='.' && *p!='-') return false;
    return true;
}
const LayoutAssembly* Layout_FindAssembly(const LayoutObjectStore* store, const char* id) {
    if (!store || !id || !id[0]) return NULL;
    for (size_t i=0;i<store->assembly_count && i<LAYOUT_MAX_ASSEMBLIES;++i)
        if (!strcmp(store->assemblies[i].id,id)) return &store->assemblies[i];
    return NULL;
}
const LayoutEntityInfo* Layout_EntityInfo(const LayoutObjectStore* store, const char* id) {
    if (!store || !id) return NULL;
    const LayoutAssembly* a=Layout_FindAssembly(store,id);
    if (a) return &a->info;
    for (size_t i=0;i<store->count;++i)
        if (!store->items[i].isDeleted && !strcmp(store->items[i].coreMeta.object_id,id)) return &store->items[i].info;
    return NULL;
}
const char* Layout_EntityType(const LayoutEntityInfo* info) {
    return info && info->entity_type[0] ? info->entity_type : "PhysicalObject";
}
bool Layout_IsDescendant(const LayoutObjectStore* store, const char* parent, const char* assembly) {
    if (!store || !parent || !assembly || !assembly[0]) return false;
    for (int depth=0;parent[0] && depth<=LAYOUT_MAX_ASSEMBLIES;++depth) {
        if (!strcmp(parent,assembly)) return true;
        const LayoutAssembly* a=Layout_FindAssembly(store,parent);
        if (!a) return false;
        parent=a->info.parent_id;
    }
    return false;
}
bool Layout_EntityInfoValid(const LayoutEntityInfo* info) {
    if (!info) return false;
    if (!bounded(info->label,sizeof(info->label)) || !bounded(info->entity_type,sizeof(info->entity_type)) ||
        !bounded(info->parent_id,sizeof(info->parent_id)) || info->property_count>LAYOUT_MAX_PROPERTIES) return false;
    for (size_t i=0;i<info->property_count;++i) {
        const LayoutProperty* p=&info->properties[i];
        if (!key_valid(p->key,sizeof(p->key)) || !bounded(p->text,sizeof(p->text)) ||
            p->kind<LAYOUT_PROPERTY_TEXT || p->kind>LAYOUT_PROPERTY_LENGTH || !isfinite(p->number) ||
            (p->kind==LAYOUT_PROPERTY_BOOL && p->number!=0 && p->number!=1)) return false;
        for (size_t j=0;j<i;++j) if (!strcmp(p->key,info->properties[j].key)) return false;
    }
    return true;
}
static bool finite_vec(Vec3 v) { return isfinite(v.x) && isfinite(v.y) && isfinite(v.z); }
static bool frame_valid(PlaneFrame3 f) {
    Vec3 b[]={f.axisU,f.axisV,f.normal};
    if (!finite_vec(f.origin)) return false;
    for (int i=0;i<3;++i) for (int j=0;j<3;++j)
        if (!finite_vec(b[i]) || fabs(Vec3_Dot(b[i],b[j])-(i==j ? 1 : 0))>1e-5) return false;
    return Vec3_Dot(Vec3_Cross(b[0],b[1]),b[2])>0.99999f;
}
bool Layout_ValidateEngineering(const Layout* layout, char* message, size_t capacity) {
    if (!layout) return false;
    const LayoutObjectStore* store=&layout->objectStore;
    bool valid=store->assembly_count<=LAYOUT_MAX_ASSEMBLIES;
    for (size_t i=0;valid && i<store->assembly_count;++i) {
        const LayoutAssembly* a=&store->assemblies[i];
        valid=key_valid(a->id,sizeof(a->id)) && Layout_EntityInfoValid(&a->info) && frame_valid(a->frame) &&
            !strcmp(Layout_EntityType(&a->info),"Assembly") &&
            (!a->info.parent_id[0] || Layout_FindAssembly(store,a->info.parent_id)) &&
            !Layout_IsDescendant(store,a->info.parent_id,a->id);
        for (size_t j=0;valid && j<i;++j) valid=strcmp(a->id,store->assemblies[j].id)!=0;
        for (size_t j=0;valid && j<store->count;++j) valid=strcmp(a->id,store->items[j].coreMeta.object_id)!=0;
    }
    for (size_t i=0;valid && i<store->count;++i) {
        const Object3D* o=&store->items[i];
        if (o->isDeleted) continue;
        valid=Layout_EntityInfoValid(&o->info) && (!o->info.parent_id[0] || Layout_FindAssembly(store,o->info.parent_id));
    }
    if (!valid && message && capacity) snprintf(message,capacity,"Invalid metadata or assembly tree: check IDs, parent cycles, properties and rigid frames.");
    return valid && Layout_ValidateMotionScopes(layout,message,capacity) && Layout_ValidateRelationships(layout,message,capacity) && Layout_ValidateSpatialRecords(layout,message,capacity) && Layout_ValidateRoutes(layout,message,capacity);
}
bool Layout_HasEngineeringData(const Layout* layout) {
    if (!layout) return false;
    if (layout->objectStore.assembly_count || layout->objectStore.relationship_count || layout->objectStore.spatial_rule_count || layout->objectStore.route_count) return true;
    for (size_t i=0;i<layout->objectStore.count;++i) {
        const Object3D* o=&layout->objectStore.items[i];
        if (!o->isDeleted && (o->info.label[0] || o->info.entity_type[0] || o->info.parent_id[0] || o->info.reference || o->info.property_count || o->info.volume_role || o->info.volume_owner[0])) return true;
    }
    return false;
}
typedef struct { const char* id; const LayoutEntityInfo* info; } InfoEdit;
static bool set_info(Layout* layout, void* context) {
    InfoEdit* edit=context;
    LayoutEntityInfo* info=(LayoutEntityInfo*)Layout_EntityInfo(&layout->objectStore,edit->id);
    if (!info || !Layout_EntityInfoValid(edit->info)) return false;
    *info=*edit->info;
    return true;
}
bool Layout_SetEntityInfo(Layout* layout, const char* id, const LayoutEntityInfo* info,
    LayoutGeometryBeforePublish history, void* context) {
    if (!id || !info) return false;
    InfoEdit edit={id,info};
    return Layout_RunGeometryEdit(layout,0,set_info,&edit,history,context);
}
typedef struct { const LayoutAssembly* assembly; const char* remove; } AssemblyEdit;
static bool edit_assembly(Layout* layout, void* context) {
    AssemblyEdit* edit=context;
    LayoutObjectStore* store=&layout->objectStore;
    if (edit->remove) {
        if (Layout_SpatialEntityReferenced(store,edit->remove)) return refuse(layout,"Remove this assembly's checks or volume ownership first.");
        if (Layout_QueryRelationships(store,edit->remove,-1,0,NULL,0))
            return refuse(layout,"Remove this assembly's links before deleting it.");
        size_t index=store->assembly_count;
        for (size_t i=0;i<store->assembly_count;++i) {
            if (!strcmp(store->assemblies[i].info.parent_id,edit->remove)) return refuse(layout,"Remove or reparent children before deleting this assembly.");
            if (!strcmp(store->assemblies[i].id,edit->remove)) index=i;
        }
        for (size_t i=0;i<store->count;++i) if (!store->items[i].isDeleted && !strcmp(store->items[i].info.parent_id,edit->remove))
            return refuse(layout,"Remove or reparent children before deleting this assembly.");
        if (index==store->assembly_count) return false;
        memmove(&store->assemblies[index],&store->assemblies[index+1],(store->assembly_count-index-1)*sizeof(LayoutAssembly));
        memset(&store->assemblies[--store->assembly_count],0,sizeof(LayoutAssembly));
        return true;
    }
    LayoutAssembly next=*edit->assembly;
    if (!bounded(next.id,sizeof(next.id))) return false;
    LayoutAssembly* existing=(LayoutAssembly*)Layout_FindAssembly(store,next.id);
    if (existing) { existing->info=next.info; return true; }
    if (next.id[0] || store->assembly_count==LAYOUT_MAX_ASSEMBLIES) return false;
    uint32_t number=store->next_assembly_id ? store->next_assembly_id : 1;
    bool used;
    do {
        if (number==UINT32_MAX) return false;
        snprintf(next.id,sizeof(next.id),"assembly_%u",number++);
        used=Layout_FindAssembly(store,next.id)!=NULL;
        for(size_t i=0;i<store->count;++i)if(!strcmp(store->items[i].coreMeta.object_id,next.id))used=true;
    } while (used);
    store->next_assembly_id=number;
    store->assemblies[store->assembly_count++]=next;
    return true;
}
bool Layout_EditAssembly(Layout* layout, const LayoutAssembly* assembly, const char* remove_id,
    LayoutGeometryBeforePublish history, void* context) {
    if (!layout || (!assembly && !remove_id)) return false;
    AssemblyEdit edit={assembly,remove_id};
    return Layout_RunGeometryEdit(layout,0,edit_assembly,&edit,history,context);
}
static Vec3 rotate(Vec3 v, Vec3 angles) {
    const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};
    const float values[]={angles.x,angles.y,angles.z};
    for (int i=0;i<3;++i) {
        float radians=values[i]*0.017453292519943295f;
        Vec3 a=axes[i];
        v=Vec3_Add(Vec3_Add(Vec3_Scale(v,cosf(radians)),Vec3_Scale(Vec3_Cross(a,v),sinf(radians))),
            Vec3_Scale(a,Vec3_Dot(a,v)*(1-cosf(radians))));
    }
    return v;
}
typedef struct { const char* id; const double* translation; Vec3 rotation; } AssemblyMove;
static bool move_assembly(Layout* layout, void* context) {
    AssemblyMove* edit=context;LayoutObjectStore* store=&layout->objectStore;
    const LayoutAssembly* selected=Layout_FindAssembly(store,edit->id);
    if (!selected || !finite_vec(edit->rotation)) return false;
    if (fabsf(edit->rotation.x)>180 || fabsf(edit->rotation.y)>180 || fabsf(edit->rotation.z)>180)
        return refuse(layout,"Assembly rotations must be within -180 to 180 degrees per world axis.");
    Vec3 origin=selected->frame.origin,delta={0};float* parts[]={&delta.x,&delta.y,&delta.z};
    for (int k=0;k<3;++k) {
        double world=edit->translation[k]/Layout_WorldScale(layout);
        if (!isfinite(world) || fabs(world)>FLT_MAX || fabs((double)(float)world-world)*Layout_WorldScale(layout)>1e-6+fabs(edit->translation[k])*1e-7) return false;
        *parts[k]=(float)world;
    }
    const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};const float values[]={edit->rotation.x,edit->rotation.y,edit->rotation.z};
    for (size_t i=0;i<store->count;++i) {
        Object3D* o=&store->items[i];
        if (o->isDeleted || !Layout_IsDescendant(store,o->info.parent_id,edit->id)) continue;
        if (o->coreMeta.flags.locked) return refuse(layout,"Assembly has a locked member.");
        Vec3 position=Vec3_Add(Vec3_Add(origin,rotate(Vec3_Sub(o->transform.position,origin),edit->rotation)),delta);
        for (int k=0;k<3;++k) if (values[k]!=0) {
            Object3D baseline=*o;bool adjusted=false;
            if (!Layout_RotateObject3D(layout,o->objectId,axes[k],values[k],&baseline,&adjusted) || adjusted)
                return refuse(layout,"Assembly rotation conflicts with member bounds or plane locks.");
        }
        bool adjusted=false;
        if (!finite_vec(position) || !Layout_SetObject3DPosition(layout,o->objectId,position,&adjusted) || adjusted ||
            Vec3_Distance(o->transform.position,position)*Layout_WorldScale(layout)>1e-6)
            return refuse(layout,"Assembly movement conflicts with member bounds, locks or precision.");
    }
    for (size_t i=0;i<store->assembly_count;++i) {
        LayoutAssembly* a=&store->assemblies[i];
        if (strcmp(a->id,edit->id) && !Layout_IsDescendant(store,a->info.parent_id,edit->id)) continue;
        a->frame.origin=Vec3_Add(Vec3_Add(origin,rotate(Vec3_Sub(a->frame.origin,origin),edit->rotation)),delta);
        a->frame.axisU=rotate(a->frame.axisU,edit->rotation);a->frame.axisV=rotate(a->frame.axisV,edit->rotation);a->frame.normal=rotate(a->frame.normal,edit->rotation);
    }
    /* External dependents may follow, but the solver must not distort any member's
     * requested rigid pose. Conflicting cross-boundary rules reject the entire move. */
    size_t bytes=store->count*sizeof(Object3D);Object3D* expected=bytes ? malloc(bytes) : NULL;
    if (bytes && !expected) return false;
    if (bytes) memcpy(expected,store->items,bytes);
    bool ok=Layout_SolveGeometryCandidate(layout);
    for (size_t i=0;ok && i<store->count;++i)
        if (!store->items[i].isDeleted && Layout_IsDescendant(store,store->items[i].info.parent_id,edit->id) &&
            memcmp(&expected[i],&store->items[i],sizeof(Object3D))) ok=false;
    free(expected);
    if (!ok) return refuse(layout,"Assembly move conflicts with a driving rule; its members must keep their rigid poses.");
    return true;
}
bool Layout_MoveAssembly(Layout* layout, const char* id, const double translation_m[3],
    Vec3 rotation_deg, LayoutGeometryBeforePublish history, void* context) {
    if (!id || !translation_m) return false;
    AssemblyMove move={id,translation_m,rotation_deg};
    return Layout_RunGeometryEdit(layout,0,move_assembly,&move,history,context);
}
static Vec3 inverse_vector(PlaneFrame3 parent, Vec3 v) {
    return (Vec3){Vec3_Dot(v,parent.axisU),Vec3_Dot(v,parent.axisV),Vec3_Dot(v,parent.normal)};
}
bool Layout_EntityLocalFrame(const Layout* layout, const char* id, PlaneFrame3* frame) {
    if (!layout || !frame) return false;
    const LayoutEntityInfo* info=Layout_EntityInfo(&layout->objectStore,id);
    if (!info) return false;
    const LayoutAssembly* a=Layout_FindAssembly(&layout->objectStore,id);
    if (a) *frame=a->frame;
    else {
        const Object3D* o=NULL;
        for (size_t i=0;i<layout->objectStore.count;++i) if (!layout->objectStore.items[i].isDeleted && !strcmp(layout->objectStore.items[i].coreMeta.object_id,id)) o=&layout->objectStore.items[i];
        if (!o) return false;
        if (o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE) *frame=(PlaneFrame3){.origin=o->transform.position,
            .axisU=rotate((Vec3){1,0,0},o->transform.rotationDeg),.axisV=rotate((Vec3){0,1,0},o->transform.rotationDeg),.normal=rotate((Vec3){0,0,1},o->transform.rotationDeg)};
        else *frame=o->kind==OBJECT3D_KIND_PLANE ? o->plane.frame : o->rectPrism.frame;
    }
    const LayoutAssembly* parent=Layout_FindAssembly(&layout->objectStore,info->parent_id);
    if (parent) {
        frame->origin=inverse_vector(parent->frame,Vec3_Sub(frame->origin,parent->frame.origin));
        frame->axisU=inverse_vector(parent->frame,frame->axisU);frame->axisV=inverse_vector(parent->frame,frame->axisV);frame->normal=inverse_vector(parent->frame,frame->normal);
    }
    return true;
}
bool Layout_QueryMatches(const LayoutObjectStore* store, const char* id, const LayoutEntityQuery* query) {
    const LayoutEntityInfo* info=Layout_EntityInfo(store,id);
    if (!info || !Layout_EntityInfoValid(info) || !query || !bounded(query->entity_type,sizeof(query->entity_type)) ||
        !bounded(query->assembly_id,sizeof(query->assembly_id)) || !bounded(query->property_key,sizeof(query->property_key)) ||
        !bounded(query->property_value,sizeof(query->property_value)) || query->designation<0 || query->designation>2) return false;
    if (query->entity_type[0] && strcmp(query->entity_type,Layout_EntityType(info))) return false;
    if (query->designation && info->reference!=(query->designation==2)) return false;
    if (query->assembly_id[0] && strcmp(id,query->assembly_id) && !Layout_IsDescendant(store,info->parent_id,query->assembly_id)) return false;
    if (!query->property_key[0]) return true;
    for (size_t i=0;i<info->property_count;++i) {
        const LayoutProperty* p=&info->properties[i];
        if (strcmp(p->key,query->property_key)) continue;
        if (!query->property_value[0]) return true;
        if (p->kind==LAYOUT_PROPERTY_TEXT) return !strcmp(p->text,query->property_value);
        if (p->kind==LAYOUT_PROPERTY_BOOL) return !strcmp(query->property_value,p->number ? "true" : "false");
        char* end=NULL;double value=strtod(query->property_value,&end);
        return end!=query->property_value && !*end && isfinite(value) && value==p->number;
    }
    return false;
}
bool Layout_ObjectShown(const LayoutObjectStore* store, const Object3D* object) {
    return object && !object->isDeleted && object->coreMeta.flags.visible && Layout_QueryMatches(store,object->coreMeta.object_id,&store->view_query);
}
size_t Layout_QueryEntities(const LayoutObjectStore* store, const LayoutEntityQuery* query, char (*ids)[64], size_t capacity) {
    if (!store) return 0;
    size_t count=0;
    for (size_t i=0;i<store->assembly_count;++i) if (Layout_QueryMatches(store,store->assemblies[i].id,query)) {
        if (ids && count<capacity) snprintf(ids[count],64,"%s",store->assemblies[i].id);
        ++count;
    }
    for (size_t i=0;i<store->count;++i) if (!store->items[i].isDeleted && Layout_QueryMatches(store,store->items[i].coreMeta.object_id,query)) {
        if (ids && count<capacity) snprintf(ids[count],64,"%s",store->items[i].coreMeta.object_id);
        ++count;
    }
    return count;
}
