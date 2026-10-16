#include "UI/topbar/visibility_tree.h"
#include "UI/topbar/visibility_guide_groups.h"
#include "Core/global_state.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "Layout/layout_saved_views.h"
#include "Layout/layout_guide_groups.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_scene_collections.h"
/* UI rows borrow current document storage only until the next Build. */
static LDVisibilityRow rows[17024];static size_t count;
static char expanded[512][64];static size_t expanded_count;
typedef struct {char id[64];LayoutEntityInfo info;bool guide;int category;} Area;
static const char* guide_folder="@guides";
static LayoutEntityInfo guide_info={.label="Guides / nonphysical space",.entity_type="Area"};
static bool helpers_saved,previous_helpers;
static char previous_guide_views[LAYOUT_MAX_SAVED_VIEWS][64];
static size_t previous_guide_view_count;
static void remember_helpers(const LayoutObjectStore* s) {
    if(helpers_saved)return;
    previous_helpers=Global_Get()->authoringGuides;helpers_saved=true;previous_guide_view_count=0;
    for(size_t i=0;i<s->saved_view_count;++i) if(Layout_IsGuideView(&s->saved_views[i]) && Layout_ViewHidden(s,s->saved_views[i].id))
        snprintf(previous_guide_views[previous_guide_view_count++],64,"%s",s->saved_views[i].id);
}
static const char* assembly_folder="@assemblies";
static LayoutEntityInfo assembly_info={.label="Assemblies / existing areas",.entity_type="Area"};
static Area areas[64];static size_t area_count;static int object_area[16384];
static void build_areas(const LayoutObjectStore* s){
    area_count=0;
    for(size_t i=0;i<s->count && i<16384;++i){const Object3D* o=&s->items[i];object_area[i]=-1;int guide=LDVisibilityGuide_Category(s,o);if(o->isDeleted||(o->info.parent_id[0]&&!guide))continue;
        const char* label=guide?LDVisibilityGuide_Label(guide):o->info.reference?"Reference objects":"Ungrouped objects (no area metadata)";
        for(size_t p=0;!guide&&p<o->info.property_count;++p)if(!strcmp(o->info.properties[p].key,"subsystem")&&o->info.properties[p].kind==LAYOUT_PROPERTY_TEXT&&o->info.properties[p].text[0]){label=o->info.properties[p].text;break;}
        size_t a=0;while(a<area_count&&(strcmp(areas[a].info.label,label)||areas[a].guide!=(guide!=0)))++a;
        if(a==area_count){if(area_count==64)continue;if(guide)snprintf(areas[a].id,64,"@guide_%d",guide);else snprintf(areas[a].id,64,"@area_%zu",a);memset(&areas[a].info,0,sizeof(areas[a].info));areas[a].guide=guide!=0;areas[a].category=guide;snprintf(areas[a].info.label,96,"%s",label);snprintf(areas[a].info.entity_type,64,"Area");++area_count;}object_area[i]=(int)a;
    }
}
static int area_index(const char* id){for(size_t i=0;i<area_count;++i)if(!strcmp(id,areas[i].id))return (int)i;return -1;}
int LDVisibilityTree_State(const LayoutObjectStore* s,const char* id){
    if(!strcmp(id,"@layer.references")){bool shown=false,hidden=false;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&s->items[i].info.reference){if(Layout_ObjectShown(s,&s->items[i]))shown=true;else hidden=true;}return shown?hidden?2:1:0;}
    if(!strcmp(id,"@unassigned")){bool shown=false,hidden=false;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!LDVisibilityGuide_Category(s,&s->items[i])&&!Layout_SceneCollectionAssigned(s,s->items[i].coreMeta.object_id)){if(Layout_ObjectShown(s,&s->items[i]))shown=true;else hidden=true;}return shown?hidden?2:1:0;}
if(!strcmp(id,guide_folder)){bool shown=Global_Get()->authoringGuides,hidden=false;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&LDVisibilityGuide_Category(s,&s->items[i])){if(Layout_ObjectShown(s,&s->items[i]))shown=true;else hidden=true;}return shown?hidden?2:1:0;}if(!strcmp(id,assembly_folder)){bool shown=false,hidden=false;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&s->items[i].info.parent_id[0]&&!LDVisibilityGuide_Category(s,&s->items[i])){if(Layout_ObjectShown(s,&s->items[i]))shown=true;else hidden=true;}return shown?hidden?2:1:0;}int a=area_index(id);if(a<0)return Layout_EntityVisibilityState(s,id);bool shown=false,hidden=false;for(size_t i=0;i<s->count&&i<16384;++i)if(object_area[i]==a){if(Layout_ObjectShown(s,&s->items[i]))shown=true;else hidden=true;}return shown?hidden?2:1:0;}
bool LDVisibilityTree_Hide(LayoutObjectStore* s,const char* id,bool hidden){if(!strcmp(id,guide_folder)){
    if(s->scene_collection_count){remember_helpers(s);if(!hidden)Layout_ShowGuidePresets(s);Global_Get()->authoringGuides=!hidden;return Layout_SetEntityHidden(s,"@layer.guides",hidden);}
    size_t members=0;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&LDVisibilityGuide_Category(s,&s->items[i]))++members;if(members+(hidden?s->hidden_entity_count:s->shown_entity_count)>1024)return false;remember_helpers(s);if(!hidden)Layout_ShowGuidePresets(s);for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&LDVisibilityGuide_Category(s,&s->items[i])&&!Layout_SetEntityHidden(s,s->items[i].coreMeta.object_id,hidden))return false;remember_helpers(s);Global_Get()->authoringGuides=!hidden;return true;}if(!strcmp(id,assembly_folder)){size_t members=0;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&s->items[i].info.parent_id[0]&&!LDVisibilityGuide_Category(s,&s->items[i]))++members;if(members+(hidden?s->hidden_entity_count:s->shown_entity_count)>1024)return false;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&s->items[i].info.parent_id[0]&&!LDVisibilityGuide_Category(s,&s->items[i])&&!Layout_SetEntityHidden(s,s->items[i].coreMeta.object_id,hidden))return false;return true;}int a=area_index(id);if(a<0)return Layout_SetEntityHidden(s,id,hidden);size_t members=0;for(size_t i=0;i<s->count&&i<16384;++i)if(object_area[i]==a)++members;
    if(members+(hidden?s->hidden_entity_count:s->shown_entity_count)>1024)return false;
    for(size_t i=0;i<s->count&&i<16384;++i)if(object_area[i]==a&&!Layout_SetEntityHidden(s,s->items[i].coreMeta.object_id,hidden))return false;return true;}
bool LDVisibilityTree_Only(LayoutObjectStore* s,const char* id){if(!strcmp(id,guide_folder)){if(s->count>1024)return false;Layout_RestoreEntityVisibility(s);for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!Layout_SetEntityHidden(s,s->items[i].coreMeta.object_id,!LDVisibilityGuide_Category(s,&s->items[i])))return false;remember_helpers(s);Global_Get()->authoringGuides=true;return true;}if(!strcmp(id,assembly_folder)){size_t outside=0;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!s->items[i].info.parent_id[0])++outside;if(outside>1024)return false;Layout_RestoreEntityVisibility(s);if(!LDVisibilityTree_Hide(s,id,false))return false;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!s->items[i].info.parent_id[0]&&!Layout_SetEntityHidden(s,s->items[i].coreMeta.object_id,true))return false;return true;}int a=area_index(id);if(a<0)return Layout_IsolateEntity(s,id);if(s->count>1024)return false;Layout_RestoreEntityVisibility(s);
    for(size_t i=0;i<s->count&&i<16384;++i)if(!s->items[i].isDeleted&&!Layout_SetEntityHidden(s,s->items[i].coreMeta.object_id,object_area[i]!=a))return false;return true;}
static bool contains(const char* a,const char* b) {
    if(!b[0])return true;for(;*a;++a){const char* p=a,*q=b;while(*p&&*q&&tolower((unsigned char)*p)==tolower((unsigned char)*q)){++p;++q;}if(!*q)return true;}return false;
}
static bool matches(const char* id,const LayoutEntityInfo* i,const char* q) {
    if(contains(id,q)||contains(i->label,q)||contains(Layout_EntityType(i),q))return true;
    for(size_t n=0;n<i->property_count;++n)if(contains(i->properties[n].key,q)||contains(i->properties[n].text,q))return true;return false;
}
void LDVisibilityTree_Reset(void){expanded_count=0;count=0;helpers_saved=false;}
void LDVisibilityTree_Restore(LayoutObjectStore* s){Layout_RestoreEntityVisibility(s);if(helpers_saved){Global_Get()->authoringGuides=previous_helpers;Layout_ShowGuidePresets(s);
    for(size_t i=0;i<previous_guide_view_count && s->hidden_view_count<LAYOUT_MAX_SAVED_VIEWS;++i)
        if(!Layout_ViewHidden(s,previous_guide_views[i]))snprintf(s->hidden_view_ids[s->hidden_view_count++],64,"%s",previous_guide_views[i]);}
    helpers_saved=false;}
bool LDVisibilityTree_Expanded(const char* id){for(size_t i=0;i<expanded_count;++i)if(!strcmp(expanded[i],id))return true;return false;}
void LDVisibilityTree_Expand(const char* id){
    for(size_t i=0;i<expanded_count;++i)if(!strcmp(expanded[i],id)){memmove(expanded[i],expanded[i+1],(--expanded_count-i)*64);return;}
    if(expanded_count<512)snprintf(expanded[expanded_count++],64,"%s",id);
}
static bool ancestor_matches(const LayoutObjectStore* s,const LayoutEntityInfo* info,const char* q) {
    const LayoutAssembly* a=Layout_FindAssembly(s,info->parent_id);
    for(size_t n=0;a && n<s->assembly_count;++n){if(matches(a->id,&a->info,q))return true;a=Layout_FindAssembly(s,a->info.parent_id);}return false;
}
static bool branch_matches(const LayoutObjectStore* s,const LayoutAssembly* a,const char* q) {
    if(matches(a->id,&a->info,q)||ancestor_matches(s,&a->info,q))return true;
    for(size_t i=0;i<s->assembly_count;++i)if(Layout_IsDescendant(s,s->assemblies[i].info.parent_id,a->id)&&matches(s->assemblies[i].id,&s->assemblies[i].info,q))return true;
    for(size_t i=0;i<s->count;++i){const Object3D* o=&s->items[i];if(!o->isDeleted&&Layout_IsDescendant(s,o->info.parent_id,a->id)&&matches(o->coreMeta.object_id,&o->info,q))return true;}return false;
}
static void emit(const LayoutObjectStore* s,const char* parent,unsigned depth,const char* q) {
    if(depth>s->assembly_count)return;
    for(size_t i=0;i<s->assembly_count;++i){const LayoutAssembly* a=&s->assemblies[i];
        if(strcmp(a->info.parent_id,parent)|| (q[0]&&!branch_matches(s,a,q)))continue;
        if(count<17024)rows[count++]=(LDVisibilityRow){a->id,&a->info,true,depth};
        if(q[0]||LDVisibilityTree_Expanded(a->id))emit(s,a->id,depth+1,q);
    }
    for(size_t i=0;i<s->count;++i){const Object3D* o=&s->items[i];
        if(o->isDeleted||(i<16384 && object_area[i]>=0 && (!parent[0]||areas[object_area[i]].guide))||strcmp(o->info.parent_id,parent)||(q[0]&&!matches(o->coreMeta.object_id,&o->info,q)&&!ancestor_matches(s,&o->info,q)))continue;
        if(count<17024)rows[count++]=(LDVisibilityRow){o->coreMeta.object_id,&o->info,false,depth};
    }
}
static LayoutCoveringCollection covering_collections[LAYOUT_MAX_COVERING_COLLECTIONS];
static size_t covering_count;
static bool covering_matches(const LayoutObjectStore* s,const LayoutCoveringCollection* c,const char* q){
    if(!q[0]||matches(c->id,&c->info,q))return true;
    size_t length=strlen(c->id);
    for(size_t i=0;i<covering_count;++i)if(!strncmp(covering_collections[i].id,c->id,length) &&
        (covering_collections[i].id[length]==0 || covering_collections[i].id[length]=='.') && matches(covering_collections[i].id,&covering_collections[i].info,q))return true;
    for(size_t i=0;i<s->count;++i)if(Layout_CoveringCollectionContains(s,c->id,s->items[i].coreMeta.object_id)&&matches(s->items[i].coreMeta.object_id,&s->items[i].info,q))return true;
    return false;
}
static void emit_coverings(const LayoutObjectStore* s,const char* parent,unsigned depth,const char* q){
    if(depth>3)return;
    for(size_t i=0;i<covering_count;++i){const LayoutCoveringCollection* c=&covering_collections[i];
        if(strcmp(c->parent,parent)||!covering_matches(s,c,q))continue;
        if(count<17024)rows[count++]=(LDVisibilityRow){c->id,&c->info,true,depth};
        if(q[0]||LDVisibilityTree_Expanded(c->id))emit_coverings(s,c->id,depth+1,q);
    }
}
size_t LDVisibilityTree_Build(const LayoutObjectStore* s,const char* search) {
    count=0;const char* q=search?search:"";build_areas(s);
    covering_count=Layout_CoveringCollections(s,covering_collections,LAYOUT_MAX_COVERING_COLLECTIONS);
    emit_coverings(s,"",0,q);
    size_t roots=0;for(size_t i=0;i<s->assembly_count;++i)if(!s->assemblies[i].info.parent_id[0])++roots;
    if(roots>1){bool match=!q[0];for(size_t i=0;!match&&i<s->assembly_count;++i)if(branch_matches(s,&s->assemblies[i],q))match=true;
        if(match){rows[count++]=(LDVisibilityRow){assembly_folder,&assembly_info,true,0};if(q[0]||LDVisibilityTree_Expanded(assembly_folder))emit(s,"",1,q);}
    }else emit(s,"",0,q);
    bool guide_match=matches(guide_folder,&guide_info,q),have_guides=false;for(size_t a=0;a<area_count;++a)if(areas[a].guide){if(!q[0]||guide_match||matches(areas[a].id,&areas[a].info,q))have_guides=true;for(size_t i=0;i<s->count&&i<16384;++i)if(object_area[i]==(int)a&&(matches(s->items[i].coreMeta.object_id,&s->items[i].info,q)||ancestor_matches(s,&s->items[i].info,q)))have_guides=true;}
    if(have_guides || (!q[0]&&Global_Get()->authoringGuides)){rows[count++]=(LDVisibilityRow){guide_folder,&guide_info,true,0};}
    /* Keep functional children contiguous, in a predictable category order. */
    for(int pass=1;pass<=8;++pass)for(size_t a=0;a<area_count;++a){if(areas[a].category!=(pass==8?0:pass))continue;if(areas[a].guide&&!q[0]&&!LDVisibilityTree_Expanded(guide_folder))continue;bool match=!q[0]||matches(areas[a].id,&areas[a].info,q);
        for(size_t i=0;!match&&i<s->count&&i<16384;++i)if(object_area[i]==(int)a&&(matches(s->items[i].coreMeta.object_id,&s->items[i].info,q)||ancestor_matches(s,&s->items[i].info,q)))match=true;
        if(areas[a].guide&&guide_match)match=true;if(!match)continue;if(count<17024)rows[count++]=(LDVisibilityRow){areas[a].id,&areas[a].info,true,areas[a].guide?1:0};
        if(q[0]||LDVisibilityTree_Expanded(areas[a].id))for(size_t i=0;i<s->count&&i<16384;++i)if(object_area[i]==(int)a && (!q[0]||matches(areas[a].id,&areas[a].info,q)||matches(s->items[i].coreMeta.object_id,&s->items[i].info,q)||ancestor_matches(s,&s->items[i].info,q)||(areas[a].guide&&guide_match)))if(count<17024)rows[count++]=(LDVisibilityRow){s->items[i].coreMeta.object_id,&s->items[i].info,false,areas[a].guide?2:1};
    }return count;
}
bool LDVisibilityTree_Row(size_t index,LDVisibilityRow* row){if(index>=count||!row)return false;*row=rows[index];return true;}

/* Scene organization never changes transform parents or motion scopes. */
#include "Layout/layout_scene_collections.h"
static LayoutEntityInfo collection_info[LAYOUT_MAX_SCENE_COLLECTIONS];
static LayoutEntityInfo unassigned_info={.label="Unassigned",.entity_type="Area"};
static bool collection_match(const LayoutObjectStore* s,const LayoutSceneCollection* c,const char* q) {
    if(!q[0]||contains(c->name,q)||contains(c->id,q))return true;
    for(size_t i=0;i<s->scene_collection_count;++i)
        if(Layout_SceneCollectionContains(s,c->id,s->scene_collections[i].id)&&contains(s->scene_collections[i].name,q))return true;
    for(size_t i=0;i<s->count;++i)
        if(!s->items[i].isDeleted&&Layout_SceneCollectionContains(s,c->id,s->items[i].coreMeta.object_id)&&matches(s->items[i].coreMeta.object_id,&s->items[i].info,q))return true;
    return false;
}
static void emit_scene(const LayoutObjectStore* s,const char* parent,unsigned depth,const char* q,bool ancestor_match) {
    if(depth>s->scene_collection_count)return;
    for(size_t i=0;i<s->scene_collection_count;++i) {
        const LayoutSceneCollection* c=&s->scene_collections[i];
        if(strcmp(c->parent,parent)||(!ancestor_match&&!collection_match(s,c,q)))continue;
        memset(&collection_info[i],0,sizeof(collection_info[i]));
        snprintf(collection_info[i].label,96,"%s",c->name);
        snprintf(collection_info[i].entity_type,64,"SceneCollection");
        rows[count++]=(LDVisibilityRow){c->id,&collection_info[i],true,depth};
        if(!q[0]&&!LDVisibilityTree_Expanded(c->id))continue;
        bool match=ancestor_match||(q[0]&&(contains(c->name,q)||contains(c->id,q)));
        emit_scene(s,c->id,depth+1,q,match);
        for(size_t o=0;o<s->count;++o) {
            const Object3D* item=&s->items[o];
            if(item->isDeleted||LDVisibilityGuide_Category(s,item))continue;
            bool direct=false;
            for(size_t m=0;m<s->collection_member_count&&!direct;++m) {
                const LayoutSceneCollectionMember* member=&s->collection_members[m];
                direct=!strcmp(member->collection,c->id)&&(!strcmp(member->entity,item->coreMeta.object_id)||Layout_IsDescendant(s,item->info.parent_id,member->entity));
            }
            if(!direct||(!match&&q[0]&&!matches(item->coreMeta.object_id,&item->info,q)))continue;
            if(count<17024)rows[count++]=(LDVisibilityRow){item->coreMeta.object_id,&item->info,false,depth+1};
        }
    }
}
size_t LDVisibilityTree_BuildScene(const LayoutObjectStore* s,const char* search) {
    if(!s->scene_collection_count)return LDVisibilityTree_Build(s,search);
    const char* q=search?search:"";count=0;emit_scene(s,"",0,q,false);
    size_t unassigned=0;
    for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!LDVisibilityGuide_Category(s,&s->items[i])&&!Layout_SceneCollectionAssigned(s,s->items[i].coreMeta.object_id))++unassigned;
    if(unassigned&&(!q[0]||contains("Unassigned",q)))rows[count++]=(LDVisibilityRow){"@unassigned",&unassigned_info,true,0};
    if(q[0]||LDVisibilityTree_Expanded("@unassigned"))for(size_t i=0;i<s->count;++i) {
        const Object3D* o=&s->items[i];if(o->isDeleted||LDVisibilityGuide_Category(s,o)||Layout_SceneCollectionAssigned(s,o->coreMeta.object_id)||!matches(o->coreMeta.object_id,&o->info,q))continue;
        if(count<17024)rows[count++]=(LDVisibilityRow){o->coreMeta.object_id,&o->info,false,1};
    }
    return count;
}
size_t LDVisibilityTree_BuildLayers(const LayoutObjectStore* s,const char* search) {
    LDVisibilityTree_Build(s,search);size_t write=0;
    for(size_t i=0;i<count;++i) {
        const char* id=rows[i].id;
        if(!strncmp(id,"@coverings",10)||!strncmp(id,"@guide",6))rows[write++]=rows[i];
        else if(!rows[i].assembly) {
            for(size_t o=0;o<s->count;++o)if(!strcmp(id,s->items[o].coreMeta.object_id)&&LDVisibilityGuide_Category(s,&s->items[o])) {rows[write++]=rows[i];break;}
        }
    }
    count=write;
    static LayoutEntityInfo references={.label="Reference geometry",.entity_type="Layer"};
    const char* q=search?search:"";bool have=false;for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&s->items[i].info.reference)have=true;
    if(have&&(!q[0]||contains(references.label,q)))rows[count++]=(LDVisibilityRow){"@layer.references",&references,true,0};
    if(have&&(q[0]||LDVisibilityTree_Expanded("@layer.references")))for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&s->items[i].info.reference&&matches(s->items[i].coreMeta.object_id,&s->items[i].info,q)&&count<17024)rows[count++]=(LDVisibilityRow){s->items[i].coreMeta.object_id,&s->items[i].info,false,1};
    return count;
}
