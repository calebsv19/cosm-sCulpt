#include "Layout/layout_saved_views.h"
#include "Layout/layout_routes.h"
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
    s->hidden_view_count=0;
    for (size_t i=0;i<previous->hidden_view_count && i<LAYOUT_MAX_SAVED_VIEWS;++i)
        if (find(s,previous->hidden_view_ids[i])) snprintf(s->hidden_view_ids[s->hidden_view_count++],64,"%s",previous->hidden_view_ids[i]);
    s->isolated_view_id[0]=0;
    if (find(s,previous->isolated_view_id)) snprintf(s->isolated_view_id,64,"%s",previous->isolated_view_id);
}
bool Layout_EntityShown(const LayoutObjectStore* s, const char* id) {
    const LayoutEntityInfo* info = Layout_EntityInfo(s,id);
    const LayoutPhysicalRoute* route = Layout_FindRoute(s,id);
    if (!info && route) info = &route->info;
    if (!info || !Layout_EntityInfoValid(info) || !Layout_QueryMatchesResolved(s,id,info,route,&s->view_query)) return false;
    bool classified=false,shown=false;
    for (size_t i=0;i<s->saved_view_count;++i) {
        const LayoutSavedView* v=&s->saved_views[i];bool matches=false;
        for (size_t j=0;j<v->query_count;++j) if (Layout_QueryMatchesResolved(s,id,info,route,&v->queries[j])) {matches=true;break;}
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
        const char* names[]={"OEM","Furniture","Devices","Wiring","Plumbing","Walkways","Keepouts","Service","Motion","Corridors"};
        for (size_t i=0;i<10;++i) {LayoutSavedView* v=&s->saved_views[s->saved_view_count++];memset(v,0,sizeof(*v));snprintf(v->id,64,"view_%zu",i+1);snprintf(v->name,96,"%s",names[i]);}
        s->saved_views[0].queries[0].designation=2;s->saved_views[0].query_count=1;
        const char* furniture[]={"Furniture","Cabinet","Panel","StructuralMember","Rail","PhysicalObject"};
        for (size_t i=0;i<6;++i) {query_type(&s->saved_views[1],furniture[i]);s->saved_views[1].queries[i].designation=1;}
        const char* devices[]={"Device","Controller","Sensor","Actuator","Battery","Fuse","Connector","PowerBus"};
        for (size_t i=0;i<8;++i) query_type(&s->saved_views[2],devices[i]);
        query_type(&s->saved_views[3],"Cable");query_type(&s->saved_views[4],"Pipe");query_type(&s->saved_views[4],"Tank");
        query_type(&s->saved_views[5],"Walkway");query_type(&s->saved_views[6],"KeepoutVolume");
        query_type(&s->saved_views[7],"ServiceVolume");query_type(&s->saved_views[8],"MotionEnvelope");query_type(&s->saved_views[9],"RoutingCorridor");
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
