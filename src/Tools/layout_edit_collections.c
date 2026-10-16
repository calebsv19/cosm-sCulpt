#include "Tools/layout_edit_collections.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_saved_views.h"
#include <stdio.h>
#include <string.h>
static const char* text(const cJSON* o,const char* key){const cJSON* v=cJSON_GetObjectItemCaseSensitive(o,key);return cJSON_IsString(v)?v->valuestring:NULL;}
static bool copy(char* to,size_t size,const char* from){if(!from||!from[0]||strlen(from)>=size)return false;snprintf(to,size,"%s",from);return true;}
bool LayoutEditCollections_Handles(const char* op){return op&&(!strcmp(op,"create_assembly")||!strcmp(op,"set_properties")||!strcmp(op,"save_covering_preset")||!strcmp(op,"save_guide_preset"));}
bool LayoutEditCollections_Apply(Layout* l,const cJSON* row,cJSON* result){
    const char* op=text(row,"operation"),*id=text(row,"entity_id");
    if(!strcmp(op,"create_assembly")){
        LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};const char* parent=text(row,"parent_id");
        if(!copy(a.info.label,96,text(row,"name")))return false;
        snprintf(a.info.entity_type,64,"Assembly");
        if(parent&&parent[0]){if(!Layout_FindAssembly(&l->objectStore,parent)||!copy(a.info.parent_id,64,parent))return false;}
        if(!Layout_EditAssembly(l,&a,NULL,NULL,NULL))return false;cJSON_AddStringToObject(result,"assembly_id",l->objectStore.assemblies[l->objectStore.assembly_count-1].id);return true;
    }
    if(!strcmp(op,"save_guide_preset")){
        LayoutSavedView view={.query_count=1};const char* category=text(row,"category");
        const char* allowed[]={"all","motion","walkways","keepouts","service","corridors","guides","annotations"};bool valid=false;
        for(size_t i=0;i<sizeof(allowed)/sizeof(allowed[0]);++i)if(category&&!strcmp(category,allowed[i]))valid=true;
        if(!valid||!copy(view.id,64,text(row,"view_id"))||!copy(view.name,96,text(row,"name"))||!copy(view.queries[0].property_value,128,category))return false;
        snprintf(view.queries[0].property_key,48,"guide_category");return Layout_EditSavedView(l,&view,NULL,NULL,NULL);
    }
    if(!strcmp(op,"save_covering_preset")){
        LayoutSavedView view={.query_count=1};const char* collection=text(row,"collection_id");
        if(!Layout_CoveringCollectionValid(&l->objectStore,collection)||!copy(view.id,64,text(row,"view_id"))||!copy(view.name,96,text(row,"name")) || !copy(view.queries[0].property_value,128,collection))return false;
        snprintf(view.queries[0].property_key,48,"covering_collection");return Layout_EditSavedView(l,&view,NULL,NULL,NULL);
    }
    const LayoutEntityInfo* old=Layout_EntityInfo(&l->objectStore,id);const cJSON* values=cJSON_GetObjectItemCaseSensitive(row,"properties");
    if(!old||!cJSON_IsObject(values))return false;LayoutEntityInfo info=*old;const cJSON* value;
    const cJSON* label=cJSON_GetObjectItemCaseSensitive(row,"name");
    if(label&&(!cJSON_IsString(label)||!copy(info.label,sizeof(info.label),label->valuestring)))return false;
    cJSON_ArrayForEach(value,values){
        if(!cJSON_IsString(value)||!value->string)return false;
        size_t i=0;while(i<info.property_count&&strcmp(info.properties[i].key,value->string))++i;
        if(i==info.property_count){if(i==LAYOUT_MAX_PROPERTIES)return false;++info.property_count;}
        LayoutProperty* p=&info.properties[i];memset(p,0,sizeof(*p));p->kind=LAYOUT_PROPERTY_TEXT;
        if(!copy(p->key,sizeof(p->key),value->string)||!copy(p->text,sizeof(p->text),value->valuestring))return false;
    }
    return Layout_SetEntityInfo(l,id,&info,NULL,NULL);
}

bool LayoutEditCollections_Inspect(Layout* l,const cJSON* command,cJSON* report){
    const char* schema=text(command,"schema");const cJSON* ops=cJSON_GetObjectItemCaseSensitive(command,"operations");
    if(!schema||strcmp(schema,"line_drawing_visibility_v1")||!cJSON_IsArray(ops)||cJSON_GetArraySize(ops)>256)return false;
    char* before=Layout_SaveToString(l);if(!before)return false;const cJSON* row;bool ok=true;
    cJSON_ArrayForEach(row,ops){
        const char* op=text(row,"operation"),*id=text(row,"collection_id");
        if(!op){ok=false;break;}
        if(!strcmp(op,"reset"))Layout_CoveringReset(&l->objectStore);
        else if(!id||(!Layout_CoveringCollectionValid(&l->objectStore,id)&&!Layout_FindSceneCollection(&l->objectStore,id)&&!Layout_EntityInfo(&l->objectStore,id))){ok=false;break;}
        else if(!strcmp(op,"hide"))ok=Layout_SetEntityHidden(&l->objectStore,id,true);
        else if(!strcmp(op,"show"))ok=Layout_SetEntityHidden(&l->objectStore,id,false);
        else if(!strcmp(op,"reveal"))ok=Layout_RevealEntity(&l->objectStore,id);
        else if(!strcmp(op,"only"))ok=Layout_IsolateEntity(&l->objectStore,id);
        else if(strcmp(op,"inspect"))ok=false;
        if(!ok)break;
        LayoutCoveringCollection collections[LAYOUT_MAX_COVERING_COLLECTIONS];size_t count=Layout_CoveringCollections(&l->objectStore,collections,LAYOUT_MAX_COVERING_COLLECTIONS);
        cJSON* result=cJSON_CreateObject(),*groups=cJSON_AddArrayToObject(result,"collections");cJSON_AddItemToArray(report,result);
        for(size_t i=0;i<count;++i){
            const LayoutCoveringCollection* c=&collections[i];if(id&&strcmp(id,c->id))continue;
            cJSON* group=cJSON_CreateObject();cJSON_AddItemToArray(groups,group);cJSON_AddStringToObject(group,"id",c->id);cJSON_AddStringToObject(group,"parent",c->parent);
            cJSON_AddNumberToObject(group,"state",Layout_EntityVisibilityState(&l->objectStore,c->id));char reason[160];Layout_VisibilityExplain(&l->objectStore,c->id,reason,sizeof(reason));cJSON_AddStringToObject(group,"visibility_reason",reason);cJSON* members=cJSON_AddArrayToObject(group,"members");
            for(size_t j=0;j<l->objectStore.count;++j)if(Layout_CoveringCollectionContains(&l->objectStore,c->id,l->objectStore.items[j].coreMeta.object_id))cJSON_AddItemToArray(members,cJSON_CreateString(l->objectStore.items[j].coreMeta.object_id));
        }
        if(id&&!Layout_CoveringCollectionValid(&l->objectStore,id)) {
            const LayoutSceneCollection* c=Layout_FindSceneCollection(&l->objectStore,id);
            cJSON* group=cJSON_CreateObject();cJSON_AddItemToArray(groups,group);
            cJSON_AddStringToObject(group,"id",id);cJSON_AddStringToObject(group,"parent",c?c->parent:"");
            cJSON_AddNumberToObject(group,"state",Layout_EntityVisibilityState(&l->objectStore,id));
            char reason[160];Layout_VisibilityExplain(&l->objectStore,id,reason,sizeof(reason));cJSON_AddStringToObject(group,"visibility_reason",reason);
            cJSON* members=cJSON_AddArrayToObject(group,"members");
            for(size_t j=0;j<l->objectStore.count;++j)if(!l->objectStore.items[j].isDeleted&&Layout_InspectionContains(&l->objectStore,id,l->objectStore.items[j].coreMeta.object_id))
                cJSON_AddItemToArray(members,cJSON_CreateString(l->objectStore.items[j].coreMeta.object_id));
        }
    }
    char* after=Layout_SaveToString(l);ok=ok&&after&&!strcmp(before,after);Layout_FreeString(before);Layout_FreeString(after);return ok;
}
