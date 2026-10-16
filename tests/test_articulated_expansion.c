#include "test_layout_internal.h"
#include "Layout/layout_motion.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_telescoping_stay.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/ui_panel_measurement.h"
#include "Layout/layout_guide_groups.h"
#include <math.h>
static const LayoutConstraint* rule(const Layout* l,const char* id){for(size_t i=0;i<l->objectStore.constraintCount;++i)if(!strcmp(id,l->objectStore.constraints[i].id))return &l->objectStore.constraints[i];return NULL;}
static bool articulation_native_sweeps_history_roundtrip(void){
    const char* scene=getenv("SCULPT_ARTICULATED_EXPANSION_LAYOUT");if(!scene)return true;
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&s->layout,scene));Global_OnLayoutLoaded(scene);Editor_ClearHistory(&s->editor);
    char* baseline=Layout_SaveToString(&s->layout);TEST_ASSERT(baseline);
    size_t count=s->layout.objectStore.count;Object3D* original=malloc(count*sizeof(*original));TEST_ASSERT(original);memcpy(original,s->layout.objectStore.items,count*sizeof(*original));
    size_t checked=0,stays=0;
    for(size_t index=0;index<s->layout.objectStore.constraintCount;++index){
        LayoutConstraint c=s->layout.objectStore.constraints[index];if(strncmp(c.id,"ax1_",4))continue;++checked;if(c.telescoping_stay.enabled)++stays;
        Layout_BeginGeometryGesture(&s->layout);
        for(int sample=0;sample<=180;++sample){
            double position=c.travel_min+(c.travel_max-c.travel_min)*sample/180;
            bool moved=Layout_SetTravelPosition(&s->layout,c.id,position,Layout_GeometryHistory,NULL);
            if(!moved)fprintf(stderr,"%s sample %d: %s\n",c.id,sample,s->layout.geometryMessage);
            TEST_ASSERT(moved);
            TEST_ASSERT(Layout_TelescopingStaysValidate(&s->layout,true));
            for(size_t i=0;i<count;++i){const Object3D* o=&s->layout.objectStore.items[i];
                if(!Layout_MotionScopeContains(&s->layout.objectStore,&c,o))TEST_ASSERT(!memcmp(&o->transform,&original[i].transform,sizeof(o->transform)));
                if(o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE)TEST_ASSERT(!memcmp(&o->meshInstance,&original[i].meshInstance,sizeof(o->meshInstance)));
            }
            if(sample==0||sample==90||sample==180){Layout loaded={0};Layout_Init(&loaded,.05f);char* saved=Layout_SaveToString(&s->layout);TEST_ASSERT(saved&&Layout_LoadFromString(&loaded,saved)&&Layout_TelescopingStaysValidate(&loaded,true));free(saved);Layout_Free(&loaded);}
        }
        Layout_EndGeometryGesture(&s->layout);TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));Editor_ClearHistory(&s->editor);
        TEST_ASSERT(Layout_SetTravelPosition(&s->layout,c.id,c.travel_max,Layout_GeometryHistory,NULL));TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
        TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));char* restored=Layout_SaveToString(&s->layout);TEST_ASSERT(restored&&!strcmp(restored,baseline));free(restored);
        TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));TEST_ASSERT(fabs(rule(&s->layout,c.id)->target-c.travel_max)<1e-5);
        TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));Editor_ClearHistory(&s->editor);
    }
    TEST_ASSERT(checked>=9);if(getenv("SCULPT_ARTICULATION_REQUIRE_STAY"))TEST_ASSERT(stays>0);
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(after&&!strcmp(after,baseline));free(after);free(baseline);free(original);ld_test_shutdown_runtime();return true;
}
static bool covering_parent_child_and_restore_contract(void){
    const char* scene=getenv("SCULPT_ARTICULATED_EXPANSION_LAYOUT");if(!scene)return true;
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&s->layout,scene));Global_OnLayoutLoaded(scene);Editor_ClearHistory(&s->editor);
    LayoutObjectStore* store=&s->layout.objectStore;char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    const char* child="@coverings.drawers.dresser.dresser_2";
    TEST_ASSERT(Layout_CoveringCollectionValid(store,child));
    TEST_ASSERT(Layout_SetEntityHidden(store,child,true));TEST_ASSERT(Layout_EntityVisibilityState(store,child)==0);
    TEST_ASSERT(Layout_SetEntityHidden(store,"@coverings",true));TEST_ASSERT(Layout_EntityVisibilityState(store,"@coverings")==0);
    TEST_ASSERT(Layout_SetEntityHidden(store,"@coverings",false));TEST_ASSERT(Layout_EntityVisibilityState(store,child)==0);TEST_ASSERT(Layout_EntityVisibilityState(store,"@coverings.drawers")==2);
    Layout snapshot={0};Layout_Init(&snapshot,.05f);TEST_ASSERT(Layout_LoadFromString(&snapshot,before));
    Layout_RestoreViewVisibility(&snapshot.objectStore,store);TEST_ASSERT(Layout_EntityVisibilityState(&snapshot.objectStore,child)==0);Layout_Free(&snapshot);
    for(size_t i=0;i<store->count;++i){const Object3D* o=&store->items[i];if(!Layout_CoveringCollectionContains(store,"@coverings",o->coreMeta.object_id))TEST_ASSERT(Layout_ObjectShown(store,o)==o->coreMeta.flags.visible);}
    TEST_ASSERT(Layout_IsolateEntity(store,child));TEST_ASSERT(Layout_EntityVisibilityState(store,child)==1);
    Layout_ShowAllViews(store);TEST_ASSERT(Layout_EntityVisibilityState(store,"@coverings")==1);
    LDVisibilityTree_Reset();LDVisibilityTree_Build(store,"");TEST_ASSERT(LDVisibilityTree_Hide(store,child,true));TEST_ASSERT(Layout_EntityVisibilityState(store,child)==0);
    if(store->saved_view_count>13){
        Layout_ToggleView(store,"ax1_cover_view_0");
        TEST_ASSERT(Layout_ViewHidden(store,"ax1_cover_view_0"));
        TEST_ASSERT(Layout_EntityVisibilityState(store,"@coverings")==0);
        TEST_ASSERT(Layout_SetEntityHidden(store,"obj3d_369",false));
        TEST_ASSERT(!Layout_EntityShown(store,"obj3d_369"));
        Layout_CoveringReset(store);TEST_ASSERT(!Layout_ViewHidden(store,"ax1_cover_view_0"));
    }
    TEST_ASSERT(LDVisibilityTree_Hide(store,"@guides",true));
    TEST_ASSERT(Layout_IsolateEntity(store,child));TEST_ASSERT(Layout_EntityVisibilityState(store,child)==1);
    Layout_CoveringReset(store);TEST_ASSERT(Layout_EntityVisibilityState(store,"@coverings")==1);
    TEST_ASSERT(LDVisibilityTree_State(store,"@guides")==0);
    for(size_t i=0;i<store->count;++i)if(Layout_GuideCategory(store,&store->items[i]))TEST_ASSERT(!Layout_ObjectShown(store,&store->items[i]));
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(after&&!strcmp(before,after)&&Editor_UndoCount(&s->editor)==0);
    free(before);free(after);Layout_ShowAllViews(store);ld_test_shutdown_runtime();return true;
}
static bool service_interlock_refusal_roundtrip(void) {
    const char* scene=getenv("SCULPT_ARTICULATED_EXPANSION_LAYOUT");if(!scene)return true;
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&s->layout,scene));Global_OnLayoutLoaded(scene);Editor_ClearHistory(&s->editor);
    const char* ids[]={"ax1_battery_service","af1_sink_door"};
    const LayoutConstraint* guarded=rule(&s->layout,ids[0]);TEST_ASSERT(guarded&&!strcmp(guarded->closed_neighbor,ids[1]));
    for(size_t sequence=0;sequence<2;++sequence) {
        const char* opened=ids[sequence];const char* blocked=ids[1-sequence];
        TEST_ASSERT(Layout_SetTravelPosition(&s->layout,opened,rule(&s->layout,opened)->travel_max,Layout_GeometryHistory,NULL));
        char* snapshot=Layout_SaveToString(&s->layout);size_t undo=Editor_UndoCount(&s->editor);TEST_ASSERT(snapshot);
        TEST_ASSERT(!Layout_SetTravelPosition(&s->layout,blocked,rule(&s->layout,blocked)->travel_max,Layout_GeometryHistory,NULL));
        char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(after&&!strcmp(snapshot,after)&&Editor_UndoCount(&s->editor)==undo);free(after);
        Layout loaded={0};Layout_Init(&loaded,.05f);TEST_ASSERT(Layout_LoadFromString(&loaded,snapshot));
        TEST_ASSERT(!Layout_SetTravelPosition(&loaded,blocked,rule(&loaded,blocked)->travel_max,NULL,NULL));Layout_Free(&loaded);free(snapshot);
        TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));Editor_ClearHistory(&s->editor);
    }
    ld_test_shutdown_runtime();return true;
}
bool articulated_expansion_run_tests(void){const TestCase cases[]={{"service_interlock_atomic_roundtrip",service_interlock_refusal_roundtrip},{"native_rigid_sweeps_history_roundtrip",articulation_native_sweeps_history_roundtrip},{"covering_parent_child_restore",covering_parent_child_and_restore_contract}};return run_test_cases("ArticulatedExpansion",cases,sizeof(cases)/sizeof(cases[0]));}
