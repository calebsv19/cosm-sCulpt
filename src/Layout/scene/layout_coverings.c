#include "Layout/layout_coverings.h"
#include "Layout/layout_saved_views.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
static const char* property(const LayoutEntityInfo* info,const char* key){
    if(!info)return NULL;
    for(size_t i=0;i<info->property_count;++i)if(info->properties[i].kind==LAYOUT_PROPERTY_TEXT&&!strcmp(info->properties[i].key,key))return info->properties[i].text;
    return NULL;
}
static bool group_valid(const char* group){
    if(!group||!group[0]||strlen(group)>50)return false;
    const char* first=strchr(group,'/');if(!first || !first[1])return false;
    size_t n=(size_t)(first-group);
    if(!((n==5&&!strncmp(group,"doors",5))||(n==7&&!strncmp(group,"drawers",7))||(n==6&&!strncmp(group,"access",6))))return false;
    int slash=0;bool segment=false;
    for(const char* p=group;*p;++p){if(*p=='/'){if(!segment)return false;segment=false;++slash;}else{if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-')return false;segment=true;}}
    return slash==2&&segment;
}
bool Layout_CoveringInfoValid(const LayoutEntityInfo* info){
    const char* group=property(info,"cover_group"),*part=property(info,"cover_part");
    return (!group&&!part)||(group_valid(group)&&part&&(!strcmp(part,"front")||!strcmp(part,"body")));
}
static bool object_path(const LayoutEntityInfo* info,char path[64]){
    const char* group=property(info,"cover_group");if(!group_valid(group))return false;
    snprintf(path,64,"@coverings.%s",group);
    for(char* p=path;*p;++p)if(*p=='/')*p='.';return true;
}
bool Layout_CoveringInfoContains(const LayoutEntityInfo* info,const char* collection){
    if(!collection||strncmp(collection,"@coverings",10))return false;
    char path[64];if(!object_path(info,path))return false;
    size_t n=strlen(collection);return !strncmp(path,collection,n)&&(path[n]==0||path[n]=='.');
}
bool Layout_CoveringCollectionContains(const LayoutObjectStore* store,const char* collection,const char* entity){
    if(!store||!collection||!entity||strncmp(collection,"@coverings",10))return false;
    return Layout_CoveringInfoContains(Layout_EntityInfo(store,entity),collection);
}
bool Layout_CoveringCollectionValid(const LayoutObjectStore* store,const char* id){
    if(!id||strncmp(id,"@coverings",10))return false;
    for(size_t i=0;i<store->count;++i)if(!store->items[i].isDeleted&&Layout_CoveringCollectionContains(store,id,store->items[i].coreMeta.object_id))return true;return false;
}
static const char* label(const char* segment){
    if(!strcmp(segment,"@coverings"))return "Coverings";
    if(!strcmp(segment,"doors"))return "Doors";
    if(!strcmp(segment,"drawers"))return "Drawers";
    if(!strcmp(segment,"access"))return "Access panels";
    if(!strcmp(segment,"dresser"))return "Driver dresser";
    if(!strcmp(segment,"rear_desks"))return "Rear desks";
    if(!strcmp(segment,"kitchen"))return "Kitchen";
    if(!strcmp(segment,"overhead"))return "Overhead cabinets";
    if(!strcmp(segment,"electrical"))return "Battery / electrical bay";
    return segment;
}
size_t Layout_CoveringCollections(const LayoutObjectStore* store,LayoutCoveringCollection* out,size_t capacity){
    size_t count=0;
    for(size_t i=0;i<store->count;++i){
        const Object3D* o=&store->items[i];char path[64];if(o->isDeleted||!object_path(&o->info,path))continue;
        char parent[64]="";char* segment=path;
        while(segment){
            char* end=strchr(segment,'.');if(end)*end=0;
            size_t found=0;while(found<count&&strcmp(out[found].id,path))++found;
            if(found==count){
                if(count==capacity)return count;
                LayoutCoveringCollection* c=&out[count++];memset(c,0,sizeof(*c));snprintf(c->id,64,"%s",path);snprintf(c->parent,64,"%s",parent);
                snprintf(c->info.label,96,"%s",label(segment));snprintf(c->info.entity_type,64,"Collection");
            }
            snprintf(parent,64,"%s",path);if(end){*end='.';segment=end+1;}else segment=NULL;
        }
    }return count;
}
bool Layout_CoveringSetHidden(LayoutObjectStore* s,const char* id,bool hidden){
    if(!Layout_CoveringCollectionValid(s,id))return false;
    for(size_t i=0;i<s->hidden_entity_count;++i)if(!strcmp(id,s->hidden_entity_ids[i])){
        if(!hidden)memmove(s->hidden_entity_ids[i],s->hidden_entity_ids[i+1],(--s->hidden_entity_count-i)*64);return true;
    }
    if(!hidden)return true;if(s->hidden_entity_count==1024)return false;
    snprintf(s->hidden_entity_ids[s->hidden_entity_count++],64,"%s",id);return true;
}
static bool covering_view(const LayoutSavedView* view) {
    if(!view->query_count)return false;
    for(size_t i=0;i<view->query_count;++i){const LayoutEntityQuery* q=&view->queries[i];
        if(strcmp(q->property_key,"covering_collection") || q->designation || q->assembly_id[0] || q->entity_type[0])return false;
    }return true;
}
bool Layout_CoveringHiddenByPreset(const LayoutObjectStore* s,const char* entity) {
    if(!s || !s->hidden_view_count)return false;
    for(size_t i=0;i<s->saved_view_count;++i){const LayoutSavedView* v=&s->saved_views[i];
        if(!covering_view(v)||!Layout_ViewHidden(s,v->id))continue;
        for(size_t j=0;j<v->query_count;++j)if(Layout_CoveringCollectionContains(s,v->queries[j].property_value,entity))return true;
    }return false;
}
static void reset_ids(LayoutObjectStore* s,char ids[][64],size_t* count) {
    for(size_t i=0;i<*count;) {
        if(Layout_CoveringCollectionValid(s,ids[i])||Layout_CoveringCollectionContains(s,"@coverings",ids[i]))memmove(ids[i],ids[i+1],(--*count-i)*64);
        else ++i;
    }
}
void Layout_CoveringReset(LayoutObjectStore* s) {
    reset_ids(s,s->revealed_entity_ids,&s->revealed_entity_count);
    reset_ids(s,s->hidden_entity_ids,&s->hidden_entity_count);reset_ids(s,s->shown_entity_ids,&s->shown_entity_count);
    if(Layout_CoveringCollectionValid(s,s->isolated_entity_id)||Layout_CoveringCollectionContains(s,"@coverings",s->isolated_entity_id))s->isolated_entity_id[0]=0;
    for(size_t i=0;i<s->saved_view_count;++i)if(covering_view(&s->saved_views[i])){
        const char* id=s->saved_views[i].id;
        if(!strcmp(s->isolated_view_id,id))s->isolated_view_id[0]=0;
        for(size_t j=0;j<s->hidden_view_count;)if(!strcmp(id,s->hidden_view_ids[j]))memmove(s->hidden_view_ids[j],s->hidden_view_ids[j+1],(--s->hidden_view_count-j)*64);else ++j;
    }
}
