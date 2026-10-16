#include "Layout/layout_scene_collections.h"
#include "Layout/layout_saved_views.h"
#include "Layout/layout_guide_groups.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Core/global_state.h"

static bool terminated(const char* s,size_t n) { return memchr(s,0,n)!=NULL; }
static bool identifier(const char* s) {
    if(!terminated(s,64)||!s[0])return false;
    for(const unsigned char* p=(const unsigned char*)s;*p;++p)
        if(!isalnum(*p)&&*p!='_'&&*p!='.'&&*p!='-'&&*p!='@')return false;
    return true;
}
const LayoutSceneCollection* Layout_FindSceneCollection(const LayoutObjectStore* s,const char* id) {
    if(!s||!id)return NULL;
    for(size_t i=0;i<s->scene_collection_count;++i)if(!strcmp(s->scene_collections[i].id,id))return &s->scene_collections[i];
    return NULL;
}
static bool group_descendant(const LayoutObjectStore* s,const char* child,const char* root) {
    for(size_t n=0;child&&child[0]&&n<=s->scene_collection_count;++n) {
        if(!strcmp(child,root))return true;
        const LayoutSceneCollection* c=Layout_FindSceneCollection(s,child);child=c?c->parent:NULL;
    }
    return false;
}
/* Compile overlapping collection membership once; render queries are O(1).
 * Cache keys borrow object ID storage only until document publication invalidates it. */
typedef struct {const char* id;uint64_t mask;} MembershipSlot;
static MembershipSlot membership[32768];
static const LayoutObjectStore* cached_store;
static void* cached_items;
void Layout_SceneCollectionInvalidate(void){cached_store=NULL;cached_items=NULL;}
static size_t id_hash(const char* id){size_t h=2166136261u;for(;*id;++id)h=(h^(unsigned char)*id)*16777619u;return h&32767;}
static void compile_membership(const LayoutObjectStore* s){
    memset(membership,0,sizeof(membership));uint64_t groups[LAYOUT_MAX_SCENE_COLLECTIONS]={0};
    uint64_t member_masks[LAYOUT_MAX_COLLECTION_MEMBERS]={0};
    for(size_t i=0;i<s->scene_collection_count;++i)for(size_t j=0;j<s->scene_collection_count;++j)
        if(group_descendant(s,s->scene_collections[i].id,s->scene_collections[j].id))groups[i]|=UINT64_C(1)<<j;
    for(size_t m=0;m<s->collection_member_count;++m)for(size_t i=0;i<s->scene_collection_count;++i)
        if(!strcmp(s->collection_members[m].collection,s->scene_collections[i].id)){member_masks[m]=groups[i];break;}
    for(size_t o=0;o<s->count;++o){
        const Object3D* object=&s->items[o];if(object->isDeleted)continue;
        const char* ancestors[LAYOUT_MAX_ASSEMBLIES+1];size_t n=0;ancestors[n++]=object->coreMeta.object_id;
        const char* parent=object->info.parent_id;
        while(parent&&parent[0]&&n<=LAYOUT_MAX_ASSEMBLIES){ancestors[n++]=parent;const LayoutAssembly* a=Layout_FindAssembly(s,parent);parent=a?a->info.parent_id:NULL;}
        uint64_t mask=0;
        for(size_t m=0;m<s->collection_member_count;++m)for(size_t j=0;j<n;++j)
            if(!strcmp(ancestors[j],s->collection_members[m].entity)){mask|=member_masks[m];break;}
        size_t h=id_hash(object->coreMeta.object_id);while(membership[h].id)h=(h+1)&32767;
        membership[h]=(MembershipSlot){object->coreMeta.object_id,mask};
    }
    cached_store=s;cached_items=s->items;
}
bool Layout_SceneCollectionContains(const LayoutObjectStore* s,const char* group,const char* entity) {
    if(!Layout_FindSceneCollection(s,group)||!entity)return false;
    if(Layout_FindSceneCollection(s,entity))return group_descendant(s,entity,group);
    if(s->count>=32768){
        const LayoutEntityInfo* info=Layout_EntityInfo(s,entity);
        for(size_t i=0;i<s->collection_member_count;++i){const LayoutSceneCollectionMember* m=&s->collection_members[i];
            if(group_descendant(s,m->collection,group)&&(!strcmp(m->entity,entity)||(info&&Layout_IsDescendant(s,info->parent_id,m->entity))))return true;}
        return false;
    }
    if(cached_store!=s||cached_items!=s->items)compile_membership(s);
    size_t slot=id_hash(entity);while(membership[slot].id&&strcmp(membership[slot].id,entity))slot=(slot+1)&32767;
    if(membership[slot].id)for(size_t i=0;i<s->scene_collection_count;++i)if(!strcmp(s->scene_collections[i].id,group))return (membership[slot].mask&(UINT64_C(1)<<i))!=0;
    return false;
}
bool Layout_SceneCollectionAssigned(const LayoutObjectStore* s,const char* entity) {
    for(size_t i=0;i<s->scene_collection_count;++i)
        if(Layout_SceneCollectionContains(s,s->scene_collections[i].id,entity))return true;
    return false;
}
bool Layout_ValidateSceneCollections(const Layout* l) {
    const LayoutObjectStore* s=&l->objectStore;
    if(s->scene_collection_count>LAYOUT_MAX_SCENE_COLLECTIONS||s->collection_member_count>LAYOUT_MAX_COLLECTION_MEMBERS||s->working_view_count>LAYOUT_MAX_WORKING_VIEWS)return false;
    for(size_t i=0;i<s->scene_collection_count;++i) {
        const LayoutSceneCollection* c=&s->scene_collections[i];
        if(!identifier(c->id)||strncmp(c->id,"@scene.",7)||!terminated(c->name,96)||!c->name[0]||!terminated(c->parent,64)||!terminated(c->inspection_frame,64))return false;
        if(c->parent[0]&&(!Layout_FindSceneCollection(s,c->parent)||group_descendant(s,c->parent,c->id)))return false;
        for(size_t j=0;j<i;++j)if(!strcmp(c->id,s->scene_collections[j].id))return false;
    }
    for(size_t i=0;i<s->collection_member_count;++i) {
        const LayoutSceneCollectionMember* m=&s->collection_members[i];
        /* Missing/deleted entity IDs remain inert; undo or a returning asset can restore them. */
        if(!terminated(m->collection,64)||!identifier(m->entity)||!Layout_FindSceneCollection(s,m->collection)||Layout_FindSceneCollection(s,m->entity))return false;
        for(size_t j=0;j<i;++j)if(!strcmp(m->entity,s->collection_members[j].entity)&&!strcmp(m->collection,s->collection_members[j].collection))return false;
    }
    for(size_t i=0;i<s->working_view_count;++i) {
        const LayoutWorkingView* v=&s->working_views[i];
        if(!identifier(v->id)||!terminated(v->name,96)||!v->name[0]||!terminated(v->only,64)||!terminated(v->isolated_view,64)||
            v->hidden_count>LAYOUT_MAX_WORKING_MASKS||v->shown_count>LAYOUT_MAX_WORKING_MASKS||v->hidden_view_count>LAYOUT_MAX_SAVED_VIEWS||v->revealed_count>LAYOUT_MAX_WORKING_MASKS)return false;
        for(size_t j=0;j<i;++j)if(!strcmp(v->id,s->working_views[j].id))return false;
        for(size_t j=0;j<v->hidden_count;++j)if(!identifier(v->hidden[j]))return false;
        for(size_t j=0;j<v->shown_count;++j)if(!identifier(v->shown[j]))return false;
        for(size_t j=0;j<v->revealed_count;++j)if(!identifier(v->revealed[j]))return false;
        for(size_t j=0;j<v->hidden_view_count;++j)if(!identifier(v->hidden_views[j]))return false;
    }
    return true;
}
static bool metadata_edit(Layout* l,LayoutGeometryMutation mutate,void* edit,LayoutGeometryBeforePublish h,void* context) {
    if(l->geometryEditActive)return false;
    Layout* candidate=malloc(sizeof(*candidate));if(!candidate)return false;*candidate=*l;
    bool ok=mutate(candidate,edit)&&Layout_ValidateSceneCollections(candidate);
    LayoutObjectStore* s=&l->objectStore;LayoutObjectStore* c=&candidate->objectStore;
    bool changed=ok&&(s->scene_collection_count!=c->scene_collection_count||s->collection_member_count!=c->collection_member_count||s->working_view_count!=c->working_view_count||
        s->next_scene_collection_id!=c->next_scene_collection_id||memcmp(s->scene_collections,c->scene_collections,sizeof(s->scene_collections))||
        memcmp(s->collection_members,c->collection_members,sizeof(s->collection_members))||memcmp(s->working_views,c->working_views,sizeof(s->working_views)));
    if(changed&&h&&!h(l,context))ok=false;
    if(ok&&changed){
        memcpy(s->scene_collections,c->scene_collections,sizeof(s->scene_collections));s->scene_collection_count=c->scene_collection_count;
        memcpy(s->collection_members,c->collection_members,sizeof(s->collection_members));s->collection_member_count=c->collection_member_count;
        memcpy(s->working_views,c->working_views,sizeof(s->working_views));s->working_view_count=c->working_view_count;
        s->next_scene_collection_id=c->next_scene_collection_id;
        Layout_SceneCollectionInvalidate();
        if(l==&Global_Get()->layout){Global_FlagLayoutChanged();Global_FlagHitboxesDirty();}
    }
    free(candidate);return ok;
}
typedef struct { const LayoutSceneCollection* group;const char* remove; } GroupEdit;
static bool edit_group(Layout* l,void* context) {
    GroupEdit* e=context;LayoutObjectStore* s=&l->objectStore;
    const char* id=e->remove?e->remove:e->group->id;
    size_t at=0;while(at<s->scene_collection_count&&strcmp(s->scene_collections[at].id,id))++at;
    if(e->remove) {
        if(at==s->scene_collection_count)return false;
        for(size_t i=0;i<s->scene_collection_count;++i)if(!strcmp(s->scene_collections[i].parent,id))return false;
        for(size_t i=0;i<s->collection_member_count;++i)if(!strcmp(s->collection_members[i].collection,id))return false;
        memmove(s->scene_collections+at,s->scene_collections+at+1,(--s->scene_collection_count-at)*sizeof(*s->scene_collections));
        return true;
    }
    if(at==LAYOUT_MAX_SCENE_COLLECTIONS)return false;
    s->scene_collections[at]=*e->group;
    if(!s->scene_collections[at].id[0]) {
        if(!s->next_scene_collection_id)s->next_scene_collection_id=1;
        do {snprintf(s->scene_collections[at].id,64,"@scene.custom_%u",s->next_scene_collection_id++);}
        while(Layout_FindSceneCollection(s,s->scene_collections[at].id));
    }
    if(at==s->scene_collection_count)++s->scene_collection_count;
    return Layout_ValidateSceneCollections(l);
}
bool Layout_EditSceneCollection(Layout* l,const LayoutSceneCollection* c,const char* remove,LayoutGeometryBeforePublish h,void* context) {
    if(!l||(!c&&!remove))return false;
    GroupEdit e={c,remove};return metadata_edit(l,edit_group,&e,h,context);
}
typedef struct { const char* group;const char* entity;bool present; } MemberEdit;
static bool edit_member(Layout* l,void* context) {
    MemberEdit* e=context;LayoutObjectStore* s=&l->objectStore;
    if(!Layout_FindSceneCollection(s,e->group)||!Layout_EntityInfo(s,e->entity))return false;
    size_t at=0;while(at<s->collection_member_count&&(strcmp(s->collection_members[at].collection,e->group)||strcmp(s->collection_members[at].entity,e->entity)))++at;
    if(!e->present) {
        if(at==s->collection_member_count)return false;
        memmove(s->collection_members+at,s->collection_members+at+1,(--s->collection_member_count-at)*sizeof(*s->collection_members));return true;
    }
    if(at<s->collection_member_count)return true;
    if(at==LAYOUT_MAX_COLLECTION_MEMBERS)return false;
    snprintf(s->collection_members[at].collection,64,"%s",e->group);
    snprintf(s->collection_members[at].entity,64,"%s",e->entity);++s->collection_member_count;return true;
}
bool Layout_AssignSceneCollection(Layout* l,const char* group,const char* entity,bool present,LayoutGeometryBeforePublish h,void* context) {
    if(!l||!group||!entity)return false;
    MemberEdit e={group,entity,present};return metadata_edit(l,edit_member,&e,h,context);
}
bool Layout_CaptureWorkingView(const LayoutObjectStore* s,LayoutWorkingView* v,bool guides) {
    if(!s||!v||s->hidden_entity_count>LAYOUT_MAX_WORKING_MASKS||s->shown_entity_count>LAYOUT_MAX_WORKING_MASKS)return false;
    LayoutEntityQuery empty={0};if(memcmp(&s->view_query,&empty,sizeof(empty)))return false;
    v->revealed_count=s->revealed_entity_count;memcpy(v->revealed,s->revealed_entity_ids,v->revealed_count*64);
    v->hidden_count=s->hidden_entity_count;v->shown_count=s->shown_entity_count;v->guides=guides;
    memcpy(v->hidden,s->hidden_entity_ids,v->hidden_count*64);memcpy(v->shown,s->shown_entity_ids,v->shown_count*64);
    snprintf(v->only,64,"%s",s->isolated_entity_id);snprintf(v->isolated_view,64,"%s",s->isolated_view_id);
    v->hidden_view_count=s->hidden_view_count;memcpy(v->hidden_views,s->hidden_view_ids,v->hidden_view_count*64);return true;
}
typedef struct {const LayoutWorkingView* view;const char* remove;} ViewEdit;
static bool edit_view(Layout* l,void* context) {
    ViewEdit* e=context;LayoutObjectStore* s=&l->objectStore;const char* id=e->remove?e->remove:e->view->id;
    size_t at=0;while(at<s->working_view_count&&strcmp(s->working_views[at].id,id))++at;
    if(e->remove) {
        if(at==s->working_view_count)return false;
        memmove(s->working_views+at,s->working_views+at+1,(--s->working_view_count-at)*sizeof(*s->working_views));return true;
    }
    if(at==LAYOUT_MAX_WORKING_VIEWS)return false;
    s->working_views[at]=*e->view;if(at==s->working_view_count)++s->working_view_count;
    return Layout_ValidateSceneCollections(l);
}
bool Layout_SaveWorkingView(Layout* l,const LayoutWorkingView* v,const char* remove,LayoutGeometryBeforePublish h,void* context) {
    if(!l||(!v&&!remove))return false;ViewEdit e={v,remove};return metadata_edit(l,edit_view,&e,h,context);
}
bool Layout_ApplyWorkingView(LayoutObjectStore* s,const char* id,bool* guides) {
    for(size_t i=0;i<s->working_view_count;++i) {
        const LayoutWorkingView* v=&s->working_views[i];if(strcmp(v->id,id))continue;
        Layout_RestoreEntityVisibility(s);memset(&s->view_query,0,sizeof(s->view_query));
        memcpy(s->hidden_entity_ids,v->hidden,v->hidden_count*64);s->hidden_entity_count=v->hidden_count;
        memcpy(s->shown_entity_ids,v->shown,v->shown_count*64);s->shown_entity_count=v->shown_count;
        memcpy(s->revealed_entity_ids,v->revealed,v->revealed_count*64);s->revealed_entity_count=v->revealed_count;
        snprintf(s->isolated_entity_id,64,"%s",v->only);snprintf(s->isolated_view_id,64,"%s",v->isolated_view);
        memcpy(s->hidden_view_ids,v->hidden_views,v->hidden_view_count*64);s->hidden_view_count=v->hidden_view_count;
        if(guides)*guides=v->guides;
        return true;
    }
    return false;
}
