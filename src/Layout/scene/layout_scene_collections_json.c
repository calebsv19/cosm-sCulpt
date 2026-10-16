#include "Layout/layout_scene_collections.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool text(const cJSON* item,const char* key,char* out,size_t capacity) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(item,key);
    if(!cJSON_IsString(v)||strlen(v->valuestring)>=capacity)return false;
    snprintf(out,capacity,"%s",v->valuestring);return true;
}
static bool write_ids(cJSON* item,const char* key,const char (*ids)[64],size_t count) {
    cJSON* a=cJSON_AddArrayToObject(item,key);if(!a)return false;
    for(size_t i=0;i<count;++i)if(!cJSON_AddItemToArray(a,cJSON_CreateString(ids[i])))return false;
    return true;
}
static bool read_ids(const cJSON* item,const char* key,char (*ids)[64],size_t* count,size_t capacity) {
    const cJSON* a=cJSON_GetObjectItemCaseSensitive(item,key);
    if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)>(int)capacity)return false;
    *count=(size_t)cJSON_GetArraySize(a);
    for(size_t i=0;i<*count;++i) {
        const cJSON* v=cJSON_GetArrayItem(a,(int)i);
        if(!cJSON_IsString(v)||!v->valuestring[0]||strlen(v->valuestring)>=64)return false;
        snprintf(ids[i],64,"%s",v->valuestring);
    }
    return true;
}
bool Layout_SceneCollectionsWriteJson(const Layout* l,cJSON* eng) {
    if(!Layout_ValidateSceneCollections(l))return false;
    const LayoutObjectStore* s=&l->objectStore;
    /* Legacy documents stay byte-compatible until a collection/view is authored. */
    if(!s->scene_collection_count&&!s->working_view_count)return true;
    cJSON* a=cJSON_AddArrayToObject(eng,"sceneCollections");
    if(!a||!cJSON_AddNumberToObject(eng,"nextSceneCollectionId",s->next_scene_collection_id?s->next_scene_collection_id:1))return false;
    for(size_t i=0;i<s->scene_collection_count;++i) {
        const LayoutSceneCollection* c=&s->scene_collections[i];cJSON* item=cJSON_CreateObject();
        if(!item||!cJSON_AddItemToArray(a,item)||!cJSON_AddStringToObject(item,"id",c->id)||
            !cJSON_AddStringToObject(item,"name",c->name)||!cJSON_AddStringToObject(item,"parent",c->parent))return false;
        if(c->inspection_frame[0]&&!cJSON_AddStringToObject(item,"inspectionFrame",c->inspection_frame))return false;
        cJSON* members=cJSON_AddArrayToObject(item,"members");if(!members)return false;
        for(size_t j=0;j<s->collection_member_count;++j)
            if(!strcmp(s->collection_members[j].collection,c->id)&&!cJSON_AddItemToArray(members,cJSON_CreateString(s->collection_members[j].entity)))return false;
    }
    a=cJSON_AddArrayToObject(eng,"workingViews");if(!a)return false;
    for(size_t i=0;i<s->working_view_count;++i) {
        const LayoutWorkingView* v=&s->working_views[i];cJSON* item=cJSON_CreateObject();
        if(!item||!cJSON_AddItemToArray(a,item)||!cJSON_AddStringToObject(item,"id",v->id)||!cJSON_AddStringToObject(item,"name",v->name)||
            !cJSON_AddStringToObject(item,"only",v->only)||!cJSON_AddStringToObject(item,"isolatedView",v->isolated_view)||!cJSON_AddBoolToObject(item,"guides",v->guides)||
            !write_ids(item,"hidden",v->hidden,v->hidden_count)||!write_ids(item,"shown",v->shown,v->shown_count)||!write_ids(item,"hiddenViews",v->hidden_views,v->hidden_view_count))return false;
        if(v->revealed_count&&!write_ids(item,"revealed",v->revealed,v->revealed_count))return false;
    }
    return true;
}
bool Layout_SceneCollectionsReadJson(Layout* l,const cJSON* eng) {
    const cJSON* a=cJSON_GetObjectItemCaseSensitive(eng,"sceneCollections");
    const cJSON* views=cJSON_GetObjectItemCaseSensitive(eng,"workingViews");
    if(!a&&!views)return true;
    if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)>LAYOUT_MAX_SCENE_COLLECTIONS||!cJSON_IsArray(views)||cJSON_GetArraySize(views)>LAYOUT_MAX_WORKING_VIEWS)return false;
    const cJSON* next=cJSON_GetObjectItemCaseSensitive(eng,"nextSceneCollectionId");
    if(!cJSON_IsNumber(next)||next->valuedouble<1||next->valuedouble>UINT32_MAX||floor(next->valuedouble)!=next->valuedouble)return false;
    LayoutObjectStore* s=&l->objectStore;s->next_scene_collection_id=(uint32_t)next->valuedouble;
    s->scene_collection_count=(size_t)cJSON_GetArraySize(a);
    for(size_t i=0;i<s->scene_collection_count;++i) {
        LayoutSceneCollection* c=&s->scene_collections[i];const cJSON* item=cJSON_GetArrayItem(a,(int)i);
        const cJSON* members=cJSON_GetObjectItemCaseSensitive(item,"members");
        if(!text(item,"id",c->id,64)||!text(item,"name",c->name,96)||!text(item,"parent",c->parent,64)||!cJSON_IsArray(members))return false;
        if(cJSON_GetObjectItemCaseSensitive(item,"inspectionFrame")&&!text(item,"inspectionFrame",c->inspection_frame,64))return false;
        for(int j=0;j<cJSON_GetArraySize(members);++j) {
            const cJSON* v=cJSON_GetArrayItem(members,j);
            if(!cJSON_IsString(v)||strlen(v->valuestring)>=64||s->collection_member_count==LAYOUT_MAX_COLLECTION_MEMBERS)return false;
            LayoutSceneCollectionMember* m=&s->collection_members[s->collection_member_count++];
            snprintf(m->collection,64,"%s",c->id);snprintf(m->entity,64,"%s",v->valuestring);
        }
    }
    s->working_view_count=(size_t)cJSON_GetArraySize(views);
    for(size_t i=0;i<s->working_view_count;++i) {
        LayoutWorkingView* v=&s->working_views[i];const cJSON* item=cJSON_GetArrayItem(views,(int)i);
        const cJSON* guides=cJSON_GetObjectItemCaseSensitive(item,"guides");
        if(!text(item,"id",v->id,64)||!text(item,"name",v->name,96)||!text(item,"only",v->only,64)||!text(item,"isolatedView",v->isolated_view,64)||!cJSON_IsBool(guides)||
            !read_ids(item,"hidden",v->hidden,&v->hidden_count,LAYOUT_MAX_WORKING_MASKS)||!read_ids(item,"shown",v->shown,&v->shown_count,LAYOUT_MAX_WORKING_MASKS)||
            !read_ids(item,"hiddenViews",v->hidden_views,&v->hidden_view_count,LAYOUT_MAX_SAVED_VIEWS))return false;
        if(cJSON_GetObjectItemCaseSensitive(item,"revealed")&&!read_ids(item,"revealed",v->revealed,&v->revealed_count,LAYOUT_MAX_WORKING_MASKS))return false;
        v->guides=cJSON_IsTrue(guides);
    }
    Layout_SceneCollectionInvalidate();
    return Layout_ValidateSceneCollections(l);
}
