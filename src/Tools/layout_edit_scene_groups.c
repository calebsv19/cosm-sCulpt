#include "Tools/layout_edit_scene_groups.h"
#include "Layout/layout_scene_collections.h"
#include <stdio.h>
#include <string.h>
static const char* text(const cJSON* row,const char* key){const cJSON* v=cJSON_GetObjectItemCaseSensitive(row,key);return cJSON_IsString(v)?v->valuestring:NULL;}
static bool copy(char* out,size_t size,const char* value){if(!value||strlen(value)>=size)return false;snprintf(out,size,"%s",value);return true;}
bool LayoutEditSceneGroups_Handles(const char* op){return op&&(!strcmp(op,"scene_collection")||!strcmp(op,"collection_member")||!strcmp(op,"working_view"));}
bool LayoutEditSceneGroups_Apply(Layout* l,const cJSON* row,cJSON* result){
    const char* op=text(row,"operation");(void)result;
    if(!strcmp(op,"scene_collection")) {
        LayoutSceneCollection c={0};const char* parent=text(row,"parent");
        if(!copy(c.id,64,text(row,"collection_id"))||!copy(c.name,96,text(row,"name"))||(parent&&!copy(c.parent,64,parent)))return false;
        if(!Layout_EditSceneCollection(l,&c,NULL,NULL,NULL))return false;
        const cJSON* members=cJSON_GetObjectItemCaseSensitive(row,"members");if(members&&!cJSON_IsArray(members))return false;
        const cJSON* member;cJSON_ArrayForEach(member,members)
            if(!cJSON_IsString(member)||!Layout_AssignSceneCollection(l,c.id,member->valuestring,true,NULL,NULL))return false;
        return true;
    }
    if(!strcmp(op,"collection_member")) {
        const cJSON* present=cJSON_GetObjectItemCaseSensitive(row,"present");
        return cJSON_IsBool(present)&&Layout_AssignSceneCollection(l,text(row,"collection_id"),text(row,"entity_id"),cJSON_IsTrue(present),NULL,NULL);
    }
    LayoutWorkingView v={0};if(!copy(v.id,64,text(row,"view_id"))||!copy(v.name,96,text(row,"name")))return false;
    const char* only=text(row,"only");if(only&&!copy(v.only,64,only))return false;
    const cJSON* guides=cJSON_GetObjectItemCaseSensitive(row,"guides");v.guides=cJSON_IsTrue(guides);
    const cJSON* hidden=cJSON_GetObjectItemCaseSensitive(row,"hidden");if(!cJSON_IsArray(hidden)||cJSON_GetArraySize(hidden)>LAYOUT_MAX_WORKING_MASKS)return false;
    const cJSON* id;cJSON_ArrayForEach(id,hidden){if(!cJSON_IsString(id)||!copy(v.hidden[v.hidden_count++],64,id->valuestring))return false;}
    return Layout_SaveWorkingView(l,&v,NULL,NULL,NULL);
}
