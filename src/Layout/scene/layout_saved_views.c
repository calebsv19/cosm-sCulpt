#include "Layout/layout_saved_views.h"
#include "Layout/layout_routes.h"
#include "Layout/layout_guide_groups.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_scene_collections.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const LayoutSavedView* find(const LayoutObjectStore* store, const char* id) {
    for (size_t i=0;i<store->saved_view_count && i<LAYOUT_MAX_SAVED_VIEWS;++i)
        if (!strcmp(store->saved_views[i].id,id)) return &store->saved_views[i];
    return NULL;
}
static bool identifier(const char* id) {
    if (!memchr(id,0,64) || !id[0]) return false;
    for (const unsigned char* p=(const unsigned char*)id;*p;++p)
        if (!isalnum(*p) && *p!='_' && *p!='-' && *p!='.') return false;
    return true;
}
bool Layout_ValidateSavedViews(const Layout* layout) {
    const LayoutObjectStore* s=&layout->objectStore;
    if (s->saved_view_count>LAYOUT_MAX_SAVED_VIEWS) return false;
    for (size_t i=0;i<s->saved_view_count;++i) {
        const LayoutSavedView* v=&s->saved_views[i];
        if (!identifier(v->id) || !memchr(v->name,0,96) || !v->name[0] ||
            !v->query_count || v->query_count>LAYOUT_MAX_VIEW_QUERIES) return false;
        for (size_t j=0;j<i;++j) if (!strcmp(v->id,s->saved_views[j].id)) return false;
        for (size_t j=0;j<v->query_count;++j) {
            const LayoutEntityQuery* q=&v->queries[j];
            if (!memchr(q->entity_type,0,64) || !memchr(q->assembly_id,0,64) ||
                !memchr(q->property_key,0,48) || !memchr(q->property_value,0,128) ||
                q->designation<0 || q->designation>2) return false;
        }
    }
    return true;
}
bool Layout_ViewHidden(const LayoutObjectStore* store, const char* id) {
    for (size_t i=0;i<store->hidden_view_count;++i) if (!strcmp(store->hidden_view_ids[i],id)) return true;
    return false;
}
void Layout_ShowAllViews(LayoutObjectStore* s) {
    Layout_RestoreEntityVisibility(s);
    s->hidden_view_count=0;s->isolated_view_id[0]=0;memset(&s->view_query,0,sizeof(s->view_query));
}
void Layout_ToggleView(LayoutObjectStore* s, const char* id) {
    if (!find(s,id)) return;
    s->isolated_view_id[0]=0;
    for (size_t i=0;i<s->hidden_view_count;++i) if (!strcmp(s->hidden_view_ids[i],id)) {
        memmove(s->hidden_view_ids[i],s->hidden_view_ids[i+1],(s->hidden_view_count-i-1)*64);
        --s->hidden_view_count;return;
    }
    if (s->hidden_view_count<LAYOUT_MAX_SAVED_VIEWS) snprintf(s->hidden_view_ids[s->hidden_view_count++],64,"%s",id);
}
void Layout_IsolateView(LayoutObjectStore* s, const char* id) {
    if (find(s,id)) {snprintf(s->isolated_view_id,64,"%s",id);memset(&s->view_query,0,sizeof(s->view_query));}
}
void Layout_RestoreViewVisibility(LayoutObjectStore* s, const LayoutObjectStore* previous) {
    Layout_RestoreEntityVisibility(s);
    /* History copies exact overrides without replaying group actions (which replace descendants). */
    for(size_t i=0;i<previous->hidden_entity_count && i<1024;++i)
        if(Layout_EntityInfo(s,previous->hidden_entity_ids[i]) || Layout_CoveringCollectionValid(s,previous->hidden_entity_ids[i]) || Layout_FindSceneCollection(s,previous->hidden_entity_ids[i]) || !strcmp(previous->hidden_entity_ids[i],"@layer.guides") || !strcmp(previous->hidden_entity_ids[i],"@layer.references") || !strcmp(previous->hidden_entity_ids[i],"@unassigned"))snprintf(s->hidden_entity_ids[s->hidden_entity_count++],64,"%s",previous->hidden_entity_ids[i]);
    for(size_t i=0;i<previous->shown_entity_count && i<1024;++i)
        if(Layout_EntityInfo(s,previous->shown_entity_ids[i]) || !strcmp(previous->shown_entity_ids[i],"@layer.guides"))snprintf(s->shown_entity_ids[s->shown_entity_count++],64,"%s",previous->shown_entity_ids[i]);
    if(Layout_EntityInfo(s,previous->isolated_entity_id) || Layout_CoveringCollectionValid(s,previous->isolated_entity_id) || Layout_FindSceneCollection(s,previous->isolated_entity_id))snprintf(s->isolated_entity_id,64,"%s",previous->isolated_entity_id);
    for(size_t i=0;i<previous->revealed_entity_count&&i<LAYOUT_MAX_WORKING_MASKS;++i)
        if(Layout_EntityInfo(s,previous->revealed_entity_ids[i])||Layout_FindSceneCollection(s,previous->revealed_entity_ids[i])||Layout_CoveringCollectionValid(s,previous->revealed_entity_ids[i]))
            snprintf(s->revealed_entity_ids[s->revealed_entity_count++],64,"%s",previous->revealed_entity_ids[i]);
    s->hidden_view_count=0;
    for (size_t i=0;i<previous->hidden_view_count && i<LAYOUT_MAX_SAVED_VIEWS;++i)
        if (find(s,previous->hidden_view_ids[i])) snprintf(s->hidden_view_ids[s->hidden_view_count++],64,"%s",previous->hidden_view_ids[i]);
    s->isolated_view_id[0]=0;
    if (find(s,previous->isolated_view_id)) snprintf(s->isolated_view_id,64,"%s",previous->isolated_view_id);
}
bool Layout_EntityShown(const LayoutObjectStore* s, const char* id) {
    if(!s || !id)return false;
    const Object3D* object=NULL;
    for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!strcmp(s->items[i].coreMeta.object_id,id)){object=&s->items[i];break;}
    const LayoutEntityInfo* info=object?&object->info:Layout_EntityInfo(s,id);
    if(!info){const LayoutPhysicalRoute* route=Layout_FindRoute(s,id);if(route)info=&route->info;}
    return info&&Layout_EntityInfoValid(info)&&Layout_EntityShownResolved(s,id,info,object);
}
bool Layout_EntityShownResolved(const LayoutObjectStore* s,const char* id,const LayoutEntityInfo* info,const Object3D* object) {
    int inspection=Layout_EntityInspectionOverrideResolved(s,id,info,object);
    if(inspection<0)return false;
    if(inspection>0&&Layout_EntityRevealed(s,object))return true;
    if(Layout_GuideHiddenByPreset(s,object)||Layout_CoveringHiddenByPreset(s,id))return false;
    if(inspection>0)return true;
    const LayoutPhysicalRoute* route = Layout_FindRoute(s,id);
    if (!info && route) info = &route->info;
    if (!info || !Layout_QueryMatchesResolved(s,id,info,route,&s->view_query)) return false;
    bool classified=false,shown=false;
    for (size_t i=0;i<s->saved_view_count;++i) {
        const LayoutSavedView* v=&s->saved_views[i];bool matches=false;
        if(Layout_IsGuideView(v)) matches=Layout_GuideViewMatches(s,object,v);
        else for (size_t j=0;j<v->query_count;++j) if (Layout_QueryMatchesResolved(s,id,info,route,&v->queries[j])) {matches=true;break;}
        if (!matches) continue;
        classified=true;
        if (s->isolated_view_id[0]) {if (!strcmp(s->isolated_view_id,v->id)) shown=true;}
        else if (!Layout_ViewHidden(s,v->id)) shown=true;
    }
    return shown || (!classified && !s->isolated_view_id[0]);
}
typedef struct {const LayoutSavedView* view;const char* remove;bool defaults;} Edit;
static void query_type(LayoutSavedView* v,const char* type) {snprintf(v->queries[v->query_count++].entity_type,64,"%s",type);}
static bool edit(Layout* layout,void* context) {
    Edit* e=context;LayoutObjectStore* s=&layout->objectStore;
    if (e->defaults) {
        if (s->saved_view_count) {snprintf(layout->geometryMessage,sizeof(layout->geometryMessage),"Defaults require an empty view list; existing views are preserved.");return false;}
        const char* names[]={"OEM","Furniture","Devices","Wiring","Plumbing","Walkways","Keepouts","Service","Motion","Corridors","Guides","Annotations"};
        for (size_t i=0;i<12;++i) {LayoutSavedView* v=&s->saved_views[s->saved_view_count++];memset(v,0,sizeof(*v));snprintf(v->id,64,"view_%zu",i+1);snprintf(v->name,96,"%s",names[i]);}
        s->saved_views[0].queries[0].designation=2;s->saved_views[0].query_count=1;
        const char* furniture[]={"Furniture","Cabinet","Panel","StructuralMember","Rail","PhysicalObject"};
        for (size_t i=0;i<6;++i) {query_type(&s->saved_views[1],furniture[i]);s->saved_views[1].queries[i].designation=1;}
        const char* devices[]={"Device","Controller","Sensor","Actuator","Battery","Fuse","Connector","PowerBus"};
        for (size_t i=0;i<8;++i) query_type(&s->saved_views[2],devices[i]);
        query_type(&s->saved_views[3],"Cable");query_type(&s->saved_views[4],"Pipe");query_type(&s->saved_views[4],"Tank");
        const char* categories[]={"walkways","keepouts","service","motion","corridors","all","annotations"};
        for(size_t i=0;i<7;++i) {
            LayoutSavedView* v=&s->saved_views[i+5];v->query_count=1;
            snprintf(v->queries[0].property_key,48,"guide_category");
            snprintf(v->queries[0].property_value,128,"%s",categories[i]);
        }
        return true;
    }
    const char* id=e->remove?e->remove:e->view->id;
    for (size_t i=0;i<s->saved_view_count;++i) if (!strcmp(s->saved_views[i].id,id)) {
        if (e->remove) {if(!strcmp(s->isolated_view_id,id))s->isolated_view_id[0]=0;
            memmove(&s->saved_views[i],&s->saved_views[i+1],(s->saved_view_count-i-1)*sizeof(LayoutSavedView));memset(&s->saved_views[--s->saved_view_count],0,sizeof(LayoutSavedView));}
        else s->saved_views[i]=*e->view;
        return true;
    }
    if (e->remove || s->saved_view_count==LAYOUT_MAX_SAVED_VIEWS) return false;
    s->saved_views[s->saved_view_count++]=*e->view;return true;
}
bool Layout_EditSavedView(Layout* l,const LayoutSavedView* v,const char* remove,LayoutGeometryBeforePublish h,void* context) {
    if (!l || (!v && !remove)) return false;
    Edit e={v,remove,false};return Layout_RunGeometryEdit(l,0,edit,&e,h,context);
}
bool Layout_InstallDefaultViews(Layout* l,LayoutGeometryBeforePublish h,void* context) {
    Edit e={.defaults=true};return Layout_RunGeometryEdit(l,0,edit,&e,h,context);
}
bool Layout_SavedViewsWriteJson(const Layout* l,cJSON* eng) {
    if (!Layout_ValidateSavedViews(l)) return false;
    cJSON* array=cJSON_AddArrayToObject(eng,"savedViews");if (!array) return false;
    for (size_t i=0;i<l->objectStore.saved_view_count;++i) {
        const LayoutSavedView* v=&l->objectStore.saved_views[i];cJSON* item=cJSON_CreateObject();
        if (!item || !cJSON_AddItemToArray(array,item) || !cJSON_AddStringToObject(item,"id",v->id) || !cJSON_AddStringToObject(item,"name",v->name)) return false;
        cJSON* queries=cJSON_AddArrayToObject(item,"queries");if (!queries) return false;
        for (size_t j=0;j<v->query_count;++j) {
            const LayoutEntityQuery* q=&v->queries[j];cJSON* value=cJSON_CreateObject();
            if (!value || !cJSON_AddItemToArray(queries,value) || !cJSON_AddStringToObject(value,"type",q->entity_type) ||
                !cJSON_AddNumberToObject(value,"designation",q->designation) || !cJSON_AddStringToObject(value,"assembly",q->assembly_id) ||
                !cJSON_AddStringToObject(value,"key",q->property_key) || !cJSON_AddStringToObject(value,"value",q->property_value)) return false;
        }
    }
    return true;
}
static bool string(const cJSON* object,const char* key,char* out,size_t capacity) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(object,key);
    if (!cJSON_IsString(v) || strlen(v->valuestring)>=capacity) return false;
    snprintf(out,capacity,"%s",v->valuestring);return true;
}
bool Layout_SavedViewsReadJson(Layout* l,const cJSON* eng,bool required) {
    const cJSON* array=cJSON_GetObjectItemCaseSensitive(eng,"savedViews");
    if (!array) return !required;
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array)>LAYOUT_MAX_SAVED_VIEWS) return false;
    l->objectStore.saved_view_count=(size_t)cJSON_GetArraySize(array);
    for (size_t i=0;i<l->objectStore.saved_view_count;++i) {
        LayoutSavedView* v=&l->objectStore.saved_views[i];const cJSON* item=cJSON_GetArrayItem(array,(int)i);
        const cJSON* queries=cJSON_GetObjectItemCaseSensitive(item,"queries");
        if (!string(item,"id",v->id,64) || !string(item,"name",v->name,96) || !cJSON_IsArray(queries) || cJSON_GetArraySize(queries)>LAYOUT_MAX_VIEW_QUERIES) return false;
        v->query_count=(size_t)cJSON_GetArraySize(queries);
        for (size_t j=0;j<v->query_count;++j) {
            LayoutEntityQuery* q=&v->queries[j];const cJSON* value=cJSON_GetArrayItem(queries,(int)j);
            const cJSON* designation=cJSON_GetObjectItemCaseSensitive(value,"designation");
            if (!string(value,"type",q->entity_type,64) || !string(value,"assembly",q->assembly_id,64) ||
                !string(value,"key",q->property_key,48) || !string(value,"value",q->property_value,128) ||
                !cJSON_IsNumber(designation) || designation->valuedouble<0 || designation->valuedouble>2 || designation->valuedouble!=(int)designation->valuedouble) return false;
            q->designation=designation->valueint;
        }
    }
    return Layout_ValidateSavedViews(l);
}

bool Layout_InspectionContains(const LayoutObjectStore* s,const char* root,const char* id) {
    if(!strcmp(root,"@layer.references")){const LayoutEntityInfo* info=Layout_EntityInfo(s,id);return info&&info->reference;}
    if(!strcmp(root,"@unassigned"))return !Layout_SceneCollectionAssigned(s,id);
    if(Layout_FindSceneCollection(s,root))return Layout_SceneCollectionContains(s,root,id);
    /* Membership already rejects unknown covering IDs; never validate a whole
     * collection again for every object visited by a row state calculation. */
    if(!strncmp(root,"@coverings",10)) {
        if(!strncmp(id,"@coverings",10)){size_t n=strlen(root);return !strncmp(root,id,n)&&(id[n]==0||id[n]=='.');}
        return Layout_CoveringCollectionContains(s,root,id);
    }
    const LayoutEntityInfo* info=Layout_EntityInfo(s,id);
    return !strcmp(root,id) || (info && Layout_IsDescendant(s,info->parent_id,root));
}
int Layout_EntityInspectionOverride(const LayoutObjectStore* s,const char* id) {
    if(!s || !id)return -1;
    const Object3D* object=NULL;
    if(s->hidden_entity_count||s->shown_entity_count||s->revealed_entity_count||s->isolated_entity_id[0])
        for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!strcmp(s->items[i].coreMeta.object_id,id)){object=&s->items[i];break;}
    return Layout_EntityInspectionOverrideResolved(s,id,object?&object->info:Layout_EntityInfo(s,id),object);
}
int Layout_EntityInspectionOverrideResolved(const LayoutObjectStore* s,const char* id,const LayoutEntityInfo* layer_info,const Object3D* layer_object) {
    if(!s || !id)return -1;
    if(!s->hidden_entity_count&&!s->shown_entity_count&&!s->revealed_entity_count&&!s->isolated_entity_id[0])return 0;
    if(s->isolated_entity_id[0] && !Layout_InspectionContains(s,s->isolated_entity_id,id))return -1;
    for(size_t i=0;i<s->hidden_entity_count;++i) {
        const char* gate=s->hidden_entity_ids[i];
        if((!strcmp(gate,"@layer.guides")&&Layout_GuideCategory(s,layer_object)) ||
           (!strcmp(gate,"@layer.references")&&layer_info&&layer_info->reference))return -1;
    }
    if(Layout_EntityRevealed(s,layer_object)) {
        /* Child hides added after Reveal remain local to that child. Reveal itself
         * removes pre-existing exclusions wholly contained in its selected scope. */
        for(size_t r=0;r<s->revealed_entity_count;++r)if(Layout_InspectionContains(s,s->revealed_entity_ids[r],id))
            for(size_t h=0;h<s->hidden_entity_count;++h)
                if(Layout_InspectionContains(s,s->revealed_entity_ids[r],s->hidden_entity_ids[h])&&
                   Layout_InspectionContains(s,s->hidden_entity_ids[h],id))return -1;
        return 1;
    }
    for(size_t i=0;i<s->hidden_entity_count;++i) {
        const char* gate=s->hidden_entity_ids[i];
        if(Layout_SceneCollectionContains(s,gate,id) ||
            (!strcmp(gate,"@layer.guides")&&Layout_GuideCategory(s,layer_object)) ||
            (!strcmp(gate,"@layer.references")&&layer_info&&layer_info->reference) ||
            (!strcmp(gate,"@unassigned")&&!Layout_GuideCategory(s,layer_object)&&!Layout_SceneCollectionAssigned(s,id)))return -1;
    }
    for(size_t i=0;i<s->shown_entity_count;++i)if(!strcmp(s->shown_entity_ids[i],"@layer.guides")&&Layout_GuideCategory(s,layer_object))return 1;
    if(!s->hidden_entity_count && !s->shown_entity_count)return 0;
    for(size_t i=0;i<s->hidden_entity_count;++i)if(Layout_CoveringInfoContains(layer_info,s->hidden_entity_ids[i]))return -1;
    /* Nearest explicit object/ancestor wins; parent Show retains child exclusions. */
    const char* current=id;
    for(size_t depth=0;current && current[0] && depth<=s->assembly_count;++depth) {
        for(size_t i=0;i<s->hidden_entity_count;++i)if(!strcmp(current,s->hidden_entity_ids[i]))return -1;
        for(size_t i=0;i<s->shown_entity_count;++i)if(!strcmp(current,s->shown_entity_ids[i]))return 1;
        const LayoutEntityInfo* info=!strcmp(current,id)?layer_info:Layout_EntityInfo(s,current);current=info?info->parent_id:NULL;
    }
    return 0;
}
bool Layout_EntityInspectionShown(const LayoutObjectStore* s,const char* id) {return Layout_EntityInspectionOverride(s,id)>=0;}
static bool set_entity_hidden(LayoutObjectStore* s,const char* id,bool hidden) {
    if(!s || !id)return false;
    if(!strcmp(id,"@layer.guides")) {
        size_t i=0;while(i<s->shown_entity_count&&strcmp(s->shown_entity_ids[i],id))++i;
        if(hidden&&i<s->shown_entity_count)memmove(s->shown_entity_ids+i,s->shown_entity_ids+i+1,(--s->shown_entity_count-i)*64);
        else if(!hidden&&i==s->shown_entity_count){if(i==1024)return false;snprintf(s->shown_entity_ids[s->shown_entity_count++],64,"%s",id);}
    }
    if(Layout_FindSceneCollection(s,id)||!strcmp(id,"@layer.guides")||!strcmp(id,"@layer.references")||!strcmp(id,"@unassigned")) {
        size_t at=0;while(at<s->hidden_entity_count&&strcmp(s->hidden_entity_ids[at],id))++at;
        if(at<s->hidden_entity_count) {if(!hidden)memmove(s->hidden_entity_ids+at,s->hidden_entity_ids+at+1,(--s->hidden_entity_count-at)*64);return true;}
        if(!hidden)return true;
        if(s->hidden_entity_count==1024)return false;
        snprintf(s->hidden_entity_ids[s->hidden_entity_count++],64,"%s",id);return true;
    }
    if(Layout_CoveringCollectionValid(s,id))return Layout_CoveringSetHidden(s,id,hidden);
    if(!Layout_EntityInfo(s,id))return false;
    /* Preserve atomic refusal if full and this command cannot free an entry. */
    char (*target)[64]=hidden?s->hidden_entity_ids:s->shown_entity_ids;
    size_t* count=hidden?&s->hidden_entity_count:&s->shown_entity_count;
    bool present=false;for(size_t i=0;i<*count;++i)if(!strcmp(id,target[i]))present=true;
    if(*count==1024&&!present)return false;
    char copy[64];snprintf(copy,sizeof(copy),"%s",id);
    /* Only this row's opposite override is removed. Child exclusions survive parent toggles. */
    char (*opposite)[64]=hidden?s->shown_entity_ids:s->hidden_entity_ids;
    size_t* opposite_count=hidden?&s->shown_entity_count:&s->hidden_entity_count;
    for(size_t i=0;i<*opposite_count;)if(!strcmp(opposite[i],copy))memmove(opposite+i,opposite+i+1,(--*opposite_count-i)*64);else ++i;
    if(hidden) {for(size_t i=0;i<s->shown_entity_count;) {if(Layout_InspectionContains(s,copy,s->shown_entity_ids[i]))memmove(s->shown_entity_ids+i,s->shown_entity_ids+i+1,(--s->shown_entity_count-i)*64);else ++i;}}
    if(!present)snprintf(target[(*count)++],64,"%s",copy);return true;
}
bool Layout_SetEntityHidden(LayoutObjectStore* s,const char* id,bool hidden) {
    if(!set_entity_hidden(s,id,hidden))return false;
    if(hidden)for(size_t i=0;i<s->revealed_entity_count;) {
        bool remove=Layout_InspectionContains(s,id,s->revealed_entity_ids[i]);
        /* A covering-layer Hide cancels overlapping area Reveal exceptions. */
        if(!remove&&!strncmp(id,"@coverings",10))for(size_t o=0;o<s->count&&!remove;++o)
            remove=!s->items[o].isDeleted&&Layout_CoveringInfoContains(&s->items[o].info,id)&&
                Layout_InspectionContains(s,s->revealed_entity_ids[i],s->items[o].coreMeta.object_id);
        if(remove)memmove(s->revealed_entity_ids+i,s->revealed_entity_ids+i+1,(--s->revealed_entity_count-i)*64);
        else ++i;
    }
    return true;
}
bool Layout_IsolateEntity(LayoutObjectStore* s,const char* id) {
    if(!s || !id || (!Layout_EntityInfo(s,id) && !Layout_CoveringCollectionValid(s,id) && !Layout_FindSceneCollection(s,id) && strcmp(id,"@layer.references") && strcmp(id,"@unassigned")))return false;
    char copy[64];snprintf(copy,sizeof(copy),"%s",id);
    if(Layout_FindSceneCollection(s,copy)) {
        /* Only removes scene gates, while independent coverings/guide exclusions survive. */
        for(size_t i=0;i<s->hidden_entity_count;) {
            if(Layout_FindSceneCollection(s,s->hidden_entity_ids[i]))memmove(s->hidden_entity_ids+i,s->hidden_entity_ids+i+1,(--s->hidden_entity_count-i)*64);
            else ++i;
        }
        s->shown_entity_count=0;s->revealed_entity_count=0;
        s->isolated_view_id[0]=0;memset(&s->view_query,0,sizeof(s->view_query));
    }else if(Layout_CoveringCollectionValid(s,copy))Layout_CoveringReset(s);
    else Layout_RestoreEntityVisibility(s);
    snprintf(s->isolated_entity_id,64,"%s",copy);
    return Layout_SetEntityHidden(s,copy,false);
}
void Layout_RestoreEntityVisibility(LayoutObjectStore* s) {s->hidden_entity_count=0;s->shown_entity_count=0;s->revealed_entity_count=0;s->isolated_entity_id[0]=0;}
int Layout_EntityVisibilityState(const LayoutObjectStore* s,const char* id) {
    bool visible=false,hidden=false;
    for(size_t i=0;i<s->count;++i) {const Object3D* o=&s->items[i];if(o->isDeleted || (!strncmp(id,"@coverings",10)?!Layout_CoveringInfoContains(&o->info,id):!Layout_InspectionContains(s,id,o->coreMeta.object_id)))continue;
        if(Layout_FindSceneCollection(s,id)&&!o->coreMeta.flags.visible&&Layout_EntityInspectionOverride(s,o->coreMeta.object_id)<=0)continue;
        if(Layout_ObjectShown(s,o))visible=true;else hidden=true;
        if(visible && hidden)return 2;
    }
    return visible?1:0;
}
typedef struct {const char* id;bool visible;} VisibilityEdit;
static bool visibility_edit(Layout* l,void* context) {
    VisibilityEdit* e=context;bool found=false;
    if(!Layout_EntityInfo(&l->objectStore,e->id))return false;
    for(size_t i=0;i<l->objectStore.count;++i) {
        Object3D* o=&l->objectStore.items[i];
        if(!o->isDeleted && Layout_InspectionContains(&l->objectStore,e->id,o->coreMeta.object_id)) {o->coreMeta.flags.visible=e->visible;found=true;}
    }
    return found;
}
bool Layout_SetAuthoredVisibility(Layout* l,const char* id,bool visible,LayoutGeometryBeforePublish history,void* context) {
    if(!l || !id)return false;
    VisibilityEdit e={id,visible};return Layout_RunGeometryEdit(l,0,visibility_edit,&e,history,context);
}
