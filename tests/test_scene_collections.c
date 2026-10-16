#include "test_layout_internal.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_saved_views.h"
#include "Layout/layout_guide_groups.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/panel/ui_panel_visibility.h"
#include "UI/panel/ui_panel_stl_options.h"

static bool group(Layout* l,const char* id,const char* name,const char* parent,bool history) {
    LayoutSceneCollection c={0};snprintf(c.id,64,"%s",id);snprintf(c.name,96,"%s",name);if(parent)snprintf(c.parent,64,"%s",parent);
    return Layout_EditSceneCollection(l,&c,NULL,history?Layout_GeometryHistory:NULL,NULL);
}
static const Object3D* object(const LayoutObjectStore* s,const char* id) {
    for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!strcmp(s->items[i].coreMeta.object_id,id))return &s->items[i];return NULL;
}
static bool create(Layout* l,char out[64]) {
    uint32_t id;RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);
    if(!Layout_CreateRectPrismPrimitive(l,&p,&id,NULL))return false;
    snprintf(out,64,"%s",Layout_ObjectStore_FindConst(&l->objectStore,id)->coreMeta.object_id);return true;
}
static bool persistent_membership_and_single_undo(void) {
    ld_test_init_runtime();GlobalState* g=Global_Get();Layout* l=&g->layout;char id[64];TEST_ASSERT(create(l,id));Editor_ClearHistory(&g->editor);
    TEST_ASSERT(group(l,"@scene.cabinet","Complete cabinet",NULL,true));TEST_ASSERT(Editor_UndoCount(&g->editor)==1);
    TEST_ASSERT(Layout_AssignSceneCollection(l,"@scene.cabinet",id,true,Layout_GeometryHistory,NULL));TEST_ASSERT(Editor_UndoCount(&g->editor)==2);
    Transform3D original=object(&l->objectStore,id)->transform;TEST_ASSERT(Layout_SceneCollectionContains(&l->objectStore,"@scene.cabinet",id));
    TEST_ASSERT(Editor_Undo(&g->editor,l));TEST_ASSERT(!Layout_SceneCollectionContains(&l->objectStore,"@scene.cabinet",id));
    TEST_ASSERT(Editor_Redo(&g->editor,l));TEST_ASSERT(Layout_SceneCollectionContains(&l->objectStore,"@scene.cabinet",id));
    TEST_ASSERT(!memcmp(&original,&object(&l->objectStore,id)->transform,sizeof(original)));
    char* saved=Layout_SaveToString(l);TEST_ASSERT(saved&&strstr(saved,"sceneCollections"));
    TEST_ASSERT(Layout_LoadFromString(l,saved));TEST_ASSERT(Layout_SceneCollectionContains(&l->objectStore,"@scene.cabinet",id));free(saved);
    TEST_ASSERT(Layout_AssignSceneCollection(l,"@scene.cabinet",id,false,Layout_GeometryHistory,NULL));TEST_ASSERT(!Layout_SceneCollectionAssigned(&l->objectStore,id));
    ld_test_shutdown_runtime();return true;
}
static bool gates_only_and_authored_hidden(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;LayoutObjectStore* s=&l->objectStore;char a[64],b[64],c[64];TEST_ASSERT(create(l,a)&&create(l,b)&&create(l,c));
    TEST_ASSERT(group(l,"@scene.shell","Shell",NULL,false)&&group(l,"@scene.skin","Roof skin","@scene.shell",false)&&group(l,"@scene.equipment","Roof equipment",NULL,false));
    TEST_ASSERT(Layout_AssignSceneCollection(l,"@scene.skin",a,true,NULL,NULL)&&Layout_AssignSceneCollection(l,"@scene.equipment",b,true,NULL,NULL)&&Layout_AssignSceneCollection(l,"@scene.equipment",c,true,NULL,NULL));
    Object3D* obsolete=(Object3D*)object(s,c);obsolete->coreMeta.flags.visible=false;
    char* before=Layout_SaveToString(l);TEST_ASSERT(Layout_SetEntityHidden(s,"@scene.shell",true));TEST_ASSERT(!Layout_ObjectShown(s,object(s,a))&&Layout_ObjectShown(s,object(s,b)));
    TEST_ASSERT(Layout_SetEntityHidden(s,"@scene.skin",true)&&Layout_SetEntityHidden(s,"@scene.shell",false));TEST_ASSERT(!Layout_ObjectShown(s,object(s,a)));
    TEST_ASSERT(Layout_SetEntityHidden(s,"@scene.skin",false)&&Layout_ObjectShown(s,object(s,a)));
    TEST_ASSERT(Layout_SetEntityHidden(s,"@layer.references",true));((Object3D*)object(s,b))->info.reference=true;
    TEST_ASSERT(Layout_IsolateEntity(s,"@scene.equipment"));TEST_ASSERT(!Layout_ObjectShown(s,object(s,a))&&!Layout_ObjectShown(s,object(s,b))&&!Layout_ObjectShown(s,object(s,c)));
    ((Object3D*)object(s,b))->info.reference=false;TEST_ASSERT(Layout_SetEntityHidden(s,"@layer.references",false));TEST_ASSERT(Layout_ObjectShown(s,object(s,b))&&!Layout_ObjectShown(s,obsolete));
    Layout_RestoreEntityVisibility(s);char* after=Layout_SaveToString(l);TEST_ASSERT(before&&after&&!strcmp(before,after));free(before);free(after);ld_test_shutdown_runtime();return true;
}
static bool assembly_inheritance_and_atomic_cycles(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};snprintf(a.info.label,96,"Native cabinet");snprintf(a.info.entity_type,64,"Assembly");TEST_ASSERT(Layout_EditAssembly(l,&a,NULL,NULL,NULL));
    char parent[64];snprintf(parent,64,"%s",l->objectStore.assemblies[0].id);TEST_ASSERT(group(l,"@scene.cabinet","Cabinet",NULL,false));
    TEST_ASSERT(Layout_AssignSceneCollection(l,"@scene.cabinet",parent,true,NULL,NULL));char id[64];TEST_ASSERT(create(l,id));
    Object3D* o=(Object3D*)object(&l->objectStore,id);LayoutEntityInfo info=o->info;snprintf(info.parent_id,64,"%s",parent);TEST_ASSERT(Layout_SetEntityInfo(l,id,&info,NULL,NULL));
    TEST_ASSERT(Layout_SceneCollectionContains(&l->objectStore,"@scene.cabinet",id));
    TEST_ASSERT(group(l,"@scene.child","Child","@scene.cabinet",false));char* before=Layout_SaveToString(l);
    TEST_ASSERT(!group(l,"@scene.cabinet","Bad cycle","@scene.child",true));TEST_ASSERT(!Layout_EditSceneCollection(l,NULL,"@scene.cabinet",Layout_GeometryHistory,NULL));
    char* after=Layout_SaveToString(l);TEST_ASSERT(before&&after&&!strcmp(before,after));free(before);free(after);ld_test_shutdown_runtime();return true;
}
static bool working_views_roundtrip(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;char id[64];TEST_ASSERT(create(l,id));TEST_ASSERT(group(l,"@scene.shell","Shell",NULL,false));TEST_ASSERT(Layout_AssignSceneCollection(l,"@scene.shell",id,true,NULL,NULL));
    TEST_ASSERT(Layout_SetEntityHidden(&l->objectStore,"@scene.shell",true));LayoutWorkingView v={0};snprintf(v.id,64,"interior");snprintf(v.name,96,"Interior - shell hidden");TEST_ASSERT(Layout_CaptureWorkingView(&l->objectStore,&v,false));TEST_ASSERT(Layout_SaveWorkingView(l,&v,NULL,Layout_GeometryHistory,NULL));
    char* saved=Layout_SaveToString(l);TEST_ASSERT(saved&&Layout_LoadFromString(l,saved));Layout_RestoreEntityVisibility(&l->objectStore);TEST_ASSERT(Layout_ObjectShown(&l->objectStore,object(&l->objectStore,id)));
    bool guides=true;TEST_ASSERT(Layout_ApplyWorkingView(&l->objectStore,"interior",&guides)&&!guides&&!Layout_ObjectShown(&l->objectStore,object(&l->objectStore,id)));
    char* after=Layout_SaveToString(l);TEST_ASSERT(after&&!strcmp(saved,after));free(saved);free(after);ld_test_shutdown_runtime();return true;
}
static bool mapped_van_exact_behavior(void) {
    const char* path=getenv("SCULPT_COLLECTION_SCENE");if(!path)return true;
    ld_test_init_runtime();GlobalState* g=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&g->layout,path));LayoutObjectStore* s=&g->layout.objectStore;
    LDVisibilityTree_Reset();TEST_ASSERT(s->scene_collection_count==32&&s->working_view_count==6);TEST_ASSERT(LDVisibilityTree_BuildScene(s,"")==8);
    const char* roots[]={"@scene.shell","@scene.roof","@scene.driver","@scene.passenger","@scene.rear","@scene.bed","@scene.electrical","@scene.water"};
    for(size_t i=0;i<8;++i){LDVisibilityRow row;TEST_ASSERT(LDVisibilityTree_Row(i,&row)&&!strcmp(row.id,roots[i]));}
    for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted&&!Layout_GuideCategory(s,&s->items[i]))TEST_ASSERT(Layout_SceneCollectionAssigned(s,s->items[i].coreMeta.object_id));
    char* before=Layout_SaveToString(&g->layout);size_t history=Editor_UndoCount(&g->editor);
    TEST_ASSERT(Layout_SetEntityHidden(s,"@scene.shell",true));const char* shell[]={"obj3d_736","obj3d_740","obj3d_702","obj3d_703","oem_driver_wall"};for(size_t i=0;i<5;++i)TEST_ASSERT(!Layout_ObjectShown(s,object(s,shell[i])));
    const char* remain[]={"obj3d_644","obj3d_704","obj3d_718","obj3d_735","obj3d_748","obj3d_259","obj3d_781"};for(size_t i=0;i<7;++i)TEST_ASSERT(Layout_ObjectShown(s,object(s,remain[i])));
    TEST_ASSERT(Layout_ApplyWorkingView(s,"working_driver",&g->authoringGuides));TEST_ASSERT(Layout_ObjectShown(s,object(s,"obj3d_748"))&&Layout_ObjectShown(s,object(s,"upper_cabinet_driver"))&&!Layout_ObjectShown(s,object(s,"obj3d_259")));
    TEST_ASSERT(Layout_ApplyWorkingView(s,"working_sink",&g->authoringGuides));const char* sink[]={"obj3d_259","obj3d_261","obj3d_262","obj3d_242","obj3d_766","obj3d_781"};for(size_t i=0;i<6;++i)TEST_ASSERT(Layout_ObjectShown(s,object(s,sink[i])));
    for(size_t i=0;i<s->count;++i)if(Layout_GuideCategory(s,&s->items[i]))TEST_ASSERT(!Layout_ObjectShown(s,&s->items[i]));
    char* after=Layout_SaveToString(&g->layout);TEST_ASSERT(before&&after&&!strcmp(before,after)&&history==Editor_UndoCount(&g->editor));free(before);free(after);ld_test_shutdown_runtime();return true;
}
static bool panel_controls_and_viewport_continuation(void) {
    ld_test_init_runtime();Global_SetWindowSize(1280,720);LDViewControls_Close();LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;TEST_ASSERT(LDViewControls_Rect(LD_VIEW_VISIBILITY,&r)&&LDViewControls_Click(r.x+2,r.y+2));
    TEST_ASSERT(LDViewControls_Rect(LD_VIEW_SEARCH,&r));TEST_ASSERT(r.x>600&&r.x+r.w<=1280);
    TEST_ASSERT(!LDViewControls_Click(100,500));TEST_ASSERT(LDViewControls_Rect(LD_VIEW_SEARCH,&r));
    TEST_ASSERT(LDViewControls_Click(r.x+2,r.y+2));SDL_Event e={.type=SDL_TEXTINPUT};snprintf(e.text.text,32,"cabinet");TEST_ASSERT(LDViewControls_Event(&e));
    LDViewControls_Close();TEST_ASSERT(!LDViewControls_Rect(LD_VIEW_SEARCH,&r));ld_test_shutdown_runtime();return true;
}
static bool scoped_reveal_and_parent_exclusions(void) {
    ld_test_init_runtime();GlobalState* g=Global_Get();Layout* l=&g->layout;LayoutObjectStore* s=&l->objectStore;
    char a[64],b[64],obsolete[64];TEST_ASSERT(create(l,a)&&create(l,b)&&create(l,obsolete));
    TEST_ASSERT(group(l,"@scene.cabinet","Cabinet",NULL,false)&&group(l,"@scene.other","Other cabinet",NULL,false));
    TEST_ASSERT(Layout_AssignSceneCollection(l,"@scene.cabinet",a,true,NULL,NULL)&&Layout_AssignSceneCollection(l,"@scene.cabinet",obsolete,true,NULL,NULL)&&Layout_AssignSceneCollection(l,"@scene.other",b,true,NULL,NULL));
    Object3D* drawer=(Object3D*)object(s,a);Object3D* other=(Object3D*)object(s,b);
    drawer->info.property_count=other->info.property_count=2;
    snprintf(drawer->info.properties[0].key,48,"cover_group");snprintf(drawer->info.properties[0].text,128,"drawers/kitchen/one");
    snprintf(other->info.properties[0].key,48,"cover_group");snprintf(other->info.properties[0].text,128,"drawers/desk/two");
    snprintf(drawer->info.properties[1].key,48,"cover_part");snprintf(drawer->info.properties[1].text,128,"body");
    snprintf(other->info.properties[1].key,48,"cover_part");snprintf(other->info.properties[1].text,128,"body");
    ((Object3D*)object(s,obsolete))->coreMeta.flags.visible=false;Editor_ClearHistory(&g->editor);
    char* before=Layout_SaveToString(l);TEST_ASSERT(before);
    TEST_ASSERT(Layout_SetEntityHidden(s,"@coverings.drawers",true));
    TEST_ASSERT(Layout_SetEntityHidden(s,"@scene.cabinet",false));TEST_ASSERT(!Layout_ObjectShown(s,drawer));
    char reason[160];Layout_VisibilityExplain(s,"@scene.cabinet",reason,sizeof(reason));TEST_ASSERT(strstr(reason,"Drawers layer"));
    TEST_ASSERT(Layout_RevealEntity(s,"@scene.cabinet"));TEST_ASSERT(Layout_ObjectShown(s,drawer)&&!Layout_ObjectShown(s,other)&&!Layout_ObjectShown(s,object(s,obsolete)));
    TEST_ASSERT(Layout_EntityShown(s,a)); /* ID and resolved paths agree. */
    TEST_ASSERT(Layout_SetEntityHidden(s,a,true)&&s->revealed_entity_count==1&&!Layout_ObjectShown(s,drawer));
    TEST_ASSERT(Layout_RevealEntity(s,"@scene.cabinet")&&Layout_ObjectShown(s,drawer));
    TEST_ASSERT(Layout_SetEntityHidden(s,"@coverings.drawers",true)&&s->revealed_entity_count==0&&!Layout_ObjectShown(s,drawer));
    TEST_ASSERT(Layout_RevealEntity(s,"@scene.cabinet")&&Layout_ObjectShown(s,drawer));
    TEST_ASSERT(Layout_SetEntityHidden(s,"@layer.references",true));drawer->info.reference=true;TEST_ASSERT(!Layout_ObjectShown(s,drawer));drawer->info.reference=false;
    TEST_ASSERT(Layout_SetEntityHidden(s,"@scene.cabinet",true));TEST_ASSERT(!Layout_ObjectShown(s,drawer)&&s->revealed_entity_count==0);
    TEST_ASSERT(Layout_SetEntityHidden(s,"@scene.cabinet",false));TEST_ASSERT(!Layout_ObjectShown(s,drawer));
    TEST_ASSERT(Layout_IsolateEntity(s,"@scene.other")&&Layout_RevealEntity(s,"@scene.cabinet"));TEST_ASSERT(!Layout_ObjectShown(s,drawer));
    Layout_VisibilityExplain(s,"@scene.cabinet",reason,sizeof(reason));TEST_ASSERT(strstr(reason,"Only"));
    Layout_RestoreEntityVisibility(s);char* after=Layout_SaveToString(l);TEST_ASSERT(after&&!strcmp(before,after)&&Editor_UndoCount(&g->editor)==0);free(before);free(after);
    /* Transform parent Show does not discard intentional hidden children. */
    LayoutAssembly parent={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(parent.info.label,96,"Carcass");snprintf(parent.info.entity_type,64,"Assembly");
    TEST_ASSERT(Layout_EditAssembly(l,&parent,NULL,NULL,NULL));drawer=(Object3D*)object(s,a);const char* root=s->assemblies[0].id;snprintf(drawer->info.parent_id,64,"%s",root);
    TEST_ASSERT(Layout_SetEntityHidden(s,a,true)&&Layout_SetEntityHidden(s,root,true)&&Layout_SetEntityHidden(s,root,false));
    TEST_ASSERT(!Layout_ObjectShown(s,drawer));TEST_ASSERT(Layout_SetEntityHidden(s,a,false)&&Layout_ObjectShown(s,drawer));
    size_t count=s->shown_entity_count;TEST_ASSERT(Layout_SetEntityHidden(s,a,false)&&s->shown_entity_count==count);
    ld_test_shutdown_runtime();return true;
}
static bool revealed_working_view_roundtrip(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;char a[64];TEST_ASSERT(create(l,a));
    TEST_ASSERT(group(l,"@scene.cabinet","Cabinet",NULL,false)&&Layout_AssignSceneCollection(l,"@scene.cabinet",a,true,NULL,NULL));
    TEST_ASSERT(Layout_SetEntityHidden(&l->objectStore,"@scene.cabinet",true)&&Layout_RevealEntity(&l->objectStore,"@scene.cabinet"));
    LayoutWorkingView view={0};snprintf(view.id,64,"reveal_view");snprintf(view.name,96,"Service reveal");
    TEST_ASSERT(Layout_CaptureWorkingView(&l->objectStore,&view,false)&&view.revealed_count==1);
    TEST_ASSERT(Layout_SaveWorkingView(l,&view,NULL,NULL,NULL));char* saved=Layout_SaveToString(l);TEST_ASSERT(saved&&strstr(saved,"revealed"));
    TEST_ASSERT(Layout_LoadFromString(l,saved)&&Layout_ApplyWorkingView(&l->objectStore,"reveal_view",NULL));
    TEST_ASSERT(Layout_ObjectShown(&l->objectStore,object(&l->objectStore,a)));
    Layout_RestoreEntityVisibility(&l->objectStore);TEST_ASSERT(!l->objectStore.revealed_entity_count);free(saved);ld_test_shutdown_runtime();return true;
}
bool scene_collections_run_tests(void) {
    const TestCase tests[]={{"scoped_reveal_parent_exclusions",scoped_reveal_and_parent_exclusions},{"revealed_working_view_roundtrip",revealed_working_view_roundtrip},{"persistent_membership_and_single_undo",persistent_membership_and_single_undo},{"gates_only_and_authored_hidden",gates_only_and_authored_hidden},{"assembly_inheritance_and_atomic_cycles",assembly_inheritance_and_atomic_cycles},{"working_views_roundtrip",working_views_roundtrip},{"mapped_van_exact_behavior",mapped_van_exact_behavior},{"panel_controls_and_viewport_continuation",panel_controls_and_viewport_continuation}};
    return run_test_cases("SceneCollections",tests,sizeof(tests)/sizeof(tests[0]));
}
