#include "Layout/layout_relationships.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

const char* Layout_RelationshipType(LayoutRelationshipKind kind) {
    static const char* names[]={"attached_to", "supported_by", "contained_by"};
    return kind>=LAYOUT_RELATIONSHIP_ATTACHED_TO && kind<=LAYOUT_RELATIONSHIP_CONTAINED_BY ? names[kind] : NULL;
}
static bool fail(char* message, size_t capacity, const char* text) {
    if (message && capacity) snprintf(message, capacity, "%s", text);
    return false;
}
const LayoutRelationship* Layout_FindRelationship(const LayoutObjectStore* store, const char* id) {
    if (!store || !id) return NULL;
    for (size_t i=0; i<store->relationship_count && i<LAYOUT_MAX_RELATIONSHIPS; ++i)
        if (!strcmp(store->relationships[i].id,id)) return &store->relationships[i];
    return NULL;
}
size_t Layout_QueryRelationships(const LayoutObjectStore* store, const char* entity,
    int kind, int direction, char (*ids)[64], size_t capacity) {
    if (!store || kind < -1 || kind>LAYOUT_RELATIONSHIP_CONTAINED_BY || direction<0 || direction>2) return 0;
    size_t count=0;
    for (size_t i=0; i<store->relationship_count && i<LAYOUT_MAX_RELATIONSHIPS; ++i) {
        const LayoutRelationship* r=&store->relationships[i];
        if (kind>=0 && (int)r->kind!=kind) continue;
        if (entity && entity[0] && !((direction!=2 && !strcmp(entity,r->source)) || (direction!=1 && !strcmp(entity,r->target)))) continue;
        if (ids && count<capacity) snprintf(ids[count],64,"%s",r->id);
        ++count;
    }
    return count;
}
/* Containment has meaning without parent transforms. Follow only containment
 * edges; attachment/support may form cycles and never enter the geometry solver. */
static bool containment_reaches(const LayoutObjectStore* store, const char* from, const char* to) {
    const char* queue[LAYOUT_MAX_RELATIONSHIPS+1];size_t count=1,head=0;queue[0]=from;
    while (head<count) {
        const char* current=queue[head++];
        if (!strcmp(current,to)) return true;
        for (size_t i=0; i<store->relationship_count; ++i) {
            const LayoutRelationship* r=&store->relationships[i];
            if (r->kind!=LAYOUT_RELATIONSHIP_CONTAINED_BY || strcmp(r->source,current)) continue;
            bool seen=false;
            for (size_t j=0; j<count; ++j) if (!strcmp(queue[j],r->target)) seen=true;
            if (!seen && count<LAYOUT_MAX_RELATIONSHIPS+1) queue[count++]=r->target;
        }
    }
    return false;
}
bool Layout_ValidateRelationships(const Layout* layout, char* message, size_t capacity) {
    if (!layout) return false;
    const LayoutObjectStore* store=&layout->objectStore;
    if (store->relationship_count>LAYOUT_MAX_RELATIONSHIPS) return fail(message,capacity,"A scene supports up to 128 relationships.");
    for (size_t i=0; i<store->relationship_count; ++i) {
        const LayoutRelationship* r=&store->relationships[i];
        if (!memchr(r->id,0,64) || !memchr(r->source,0,64) || !memchr(r->target,0,64) || !r->id[0] || !Layout_RelationshipType(r->kind))
            return fail(message,capacity,"Invalid relationship ID or type.");
        for (const unsigned char* c=(const unsigned char*)r->id; *c; ++c)
            if (!isalnum(*c) && *c!='_' && *c!='.' && *c!='-') return fail(message,capacity,"Relationship IDs use letters, digits, underscore, dot or hyphen.");
        if (!Layout_EntityInfo(store,r->source) || !Layout_EntityInfo(store,r->target)) return fail(message,capacity,"Relationship endpoints must be existing objects or assemblies.");
        if (!strcmp(r->source,r->target)) return fail(message,capacity,"A relationship cannot refer to the same entity at both ends.");
        for (size_t j=0; j<i; ++j) {
            const LayoutRelationship* old=&store->relationships[j];
            if (!strcmp(r->id,old->id)) return fail(message,capacity,"Relationship IDs must be unique.");
            if (r->kind==old->kind && !strcmp(r->source,old->source) && !strcmp(r->target,old->target))
                return fail(message,capacity,"This directed relationship already exists.");
        }
        if (r->kind==LAYOUT_RELATIONSHIP_CONTAINED_BY && containment_reaches(store,r->target,r->source))
            return fail(message,capacity,"Containment cannot form a cycle.");
    }
    return true;
}
typedef struct {
    const LayoutRelationship* relationship;
    const char* remove_id;
} RelationshipEdit;
static bool edit_relationship(Layout* layout, void* context) {
    RelationshipEdit* edit=context;LayoutObjectStore* store=&layout->objectStore;
    if (edit->remove_id) {
        const LayoutRelationship* saved=Layout_FindRelationship(store,edit->remove_id);
        if (!saved) return fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Relationship no longer exists.");
        size_t index=(size_t)(saved-store->relationships);
        memmove(&store->relationships[index],&store->relationships[index+1],(store->relationship_count-index-1)*sizeof(LayoutRelationship));
        memset(&store->relationships[--store->relationship_count],0,sizeof(LayoutRelationship));
        return true;
    }
    LayoutRelationship next=*edit->relationship;
    if (!memchr(next.id,0,sizeof(next.id))) return false;
    LayoutRelationship* existing=(LayoutRelationship*)Layout_FindRelationship(store,next.id);
    if (existing) { *existing=next; return true; }
    if (next.id[0]) return fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Choose an existing relationship to update, or leave its ID empty to create.");
    if (store->relationship_count==LAYOUT_MAX_RELATIONSHIPS) return fail(layout->geometryMessage,sizeof(layout->geometryMessage),"A scene supports up to 128 relationships.");
    uint32_t number=store->next_relationship_id ? store->next_relationship_id : 1;
    do {
        if (number==UINT32_MAX) return false;
        snprintf(next.id,sizeof(next.id),"relationship_%u",number++);
    } while (Layout_FindRelationship(store,next.id));
    store->next_relationship_id=number;
    store->relationships[store->relationship_count++]=next;
    return true;
}
bool Layout_EditRelationship(Layout* layout, const LayoutRelationship* relationship,
    const char* remove_id, LayoutGeometryBeforePublish history, void* context) {
    if (!layout || (!relationship && !remove_id)) return false;
    RelationshipEdit edit={relationship,remove_id};
    return Layout_RunGeometryEdit(layout,0,edit_relationship,&edit,history,context);
}
cJSON* Layout_RelationshipsToJson(const LayoutObjectStore* store, const char* entity) {
    if (!store || store->relationship_count>LAYOUT_MAX_RELATIONSHIPS) return NULL;
    cJSON* array=cJSON_CreateArray();
    if (!array) return NULL;
    for (size_t i=0; i<store->relationship_count; ++i) {
        const LayoutRelationship* r=&store->relationships[i];
        if (entity && entity[0] && strcmp(entity,r->source) && strcmp(entity,r->target)) continue;
        cJSON* item=cJSON_CreateObject();
        const char* type=Layout_RelationshipType(r->kind);
        if (!item || !type || !cJSON_AddStringToObject(item,"id",r->id) || !cJSON_AddStringToObject(item,"source",r->source) ||
            !cJSON_AddStringToObject(item,"target",r->target) || !cJSON_AddStringToObject(item,"type",type)) {
            cJSON_Delete(item);cJSON_Delete(array);return NULL;
        }
        if (!cJSON_AddItemToArray(array,item)) {cJSON_Delete(item);cJSON_Delete(array);return NULL;}
    }
    return array;
}
bool Layout_RelationshipsWriteJson(const Layout* layout, cJSON* engineering) {
    cJSON* array=Layout_RelationshipsToJson(&layout->objectStore,NULL);
    if (!array) return false;
    if (!cJSON_AddItemToObject(engineering,"relationships",array)) {cJSON_Delete(array);return false;}
    return cJSON_AddNumberToObject(engineering,"nextRelationshipId",layout->objectStore.next_relationship_id ? layout->objectStore.next_relationship_id : 1)!=NULL;
}
static bool read_string(const cJSON* item, const char* key, char value[64]) {
    const cJSON* text=cJSON_GetObjectItemCaseSensitive(item,key);
    if (!cJSON_IsString(text) || !text->valuestring || strlen(text->valuestring)>=64) return false;
    snprintf(value,64,"%s",text->valuestring);return true;
}
bool Layout_RelationshipsReadJson(Layout* layout, const cJSON* engineering, bool required) {
    const cJSON* array=cJSON_GetObjectItemCaseSensitive(engineering,"relationships");
    const cJSON* next=cJSON_GetObjectItemCaseSensitive(engineering,"nextRelationshipId");
    if (!array && !next) return !required;
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array)>LAYOUT_MAX_RELATIONSHIPS ||
        !cJSON_IsNumber(next) || !isfinite(next->valuedouble) || next->valuedouble<1 || next->valuedouble>UINT32_MAX || floor(next->valuedouble)!=next->valuedouble) return false;
    if (!required && cJSON_GetArraySize(array)) return false; /* Refuse downgraded nonempty graph. */
    layout->objectStore.next_relationship_id=(uint32_t)next->valuedouble;
    layout->objectStore.relationship_count=(size_t)cJSON_GetArraySize(array);
    for (size_t i=0; i<layout->objectStore.relationship_count; ++i) {
        LayoutRelationship* r=&layout->objectStore.relationships[i];const cJSON* item=cJSON_GetArrayItem(array,(int)i);char type[64];
        if (!read_string(item,"id",r->id) || !read_string(item,"source",r->source) || !read_string(item,"target",r->target) || !read_string(item,"type",type)) return false;
        bool found=false;
        for (int k=0; k<=LAYOUT_RELATIONSHIP_CONTAINED_BY; ++k) if (!strcmp(type,Layout_RelationshipType((LayoutRelationshipKind)k))) {r->kind=(LayoutRelationshipKind)k;found=true;}
        if (!found) return false;
    }
    return Layout_ValidateRelationships(layout,NULL,0);
}
