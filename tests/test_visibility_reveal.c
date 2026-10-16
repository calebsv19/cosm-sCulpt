#include "test_layout_internal.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_mesh_view_plan.h"
#include "Core/camera_view.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/visibility_tree.h"
#include "Layout/layout_guide_groups.h"
static bool click(int action) {
    LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;
    return LDViewControls_Rect(action,&r)&&LDViewControls_Click(r.x+2,r.y+2);
}
static bool inspection_reveal_precedence(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();Editor_ClearHistory(&s->editor);LDViewControls_Close();
    LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};snprintf(a.info.label,96,"Exterior");snprintf(a.info.entity_type,64,"Assembly");
    TEST_ASSERT(Layout_EditAssembly(&s->layout,&a,NULL,NULL,NULL));const char* parent=s->layout.objectStore.assemblies[0].id;
    uint32_t ids[3];for(int i=0;i<3;++i){RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&ids[i],NULL));}
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,ids[0]);Object3D* other=Layout_ObjectStore_Find(&s->layout.objectStore,ids[1]);
    o->coreMeta.flags.visible=other->coreMeta.flags.visible=false;snprintf(o->info.parent_id,64,"%s",parent);snprintf(other->info.parent_id,64,"%s",parent);
    LayoutObjectStore* store=&s->layout.objectStore;snprintf(store->view_query.entity_type,64,"No matching type");LayoutEntityQuery filter=store->view_query;
    TEST_ASSERT(Layout_InstallDefaultViews(&s->layout,NULL,NULL));
    snprintf(store->hidden_view_ids[0],64,"%s",store->saved_views[0].id);store->hidden_view_count=1;
    char saved_filter[64];snprintf(saved_filter,64,"%s",store->hidden_view_ids[0]);
    char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(Layout_EntityVisibilityState(store,parent)==0);TEST_ASSERT(Layout_SetEntityHidden(store,parent,false));
    TEST_ASSERT(Layout_ObjectShown(store,o)&&Layout_ObjectShown(store,other)&&Layout_EntityVisibilityState(store,parent)==1);
    TEST_ASSERT(Layout_SetEntityHidden(store,o->coreMeta.object_id,true));TEST_ASSERT(!Layout_ObjectShown(store,o)&&Layout_EntityVisibilityState(store,parent)==2);
    TEST_ASSERT(Layout_SetEntityHidden(store,parent,false));TEST_ASSERT(!Layout_ObjectShown(store,o)&&Layout_EntityVisibilityState(store,parent)==2);
    TEST_ASSERT(Layout_SetEntityHidden(store,parent,true));TEST_ASSERT(!Layout_ObjectShown(store,o));
    TEST_ASSERT(Layout_SetEntityHidden(store,o->coreMeta.object_id,false));TEST_ASSERT(Layout_ObjectShown(store,o)&&!Layout_ObjectShown(store,other));
    TEST_ASSERT(Layout_IsolateEntity(store,parent));TEST_ASSERT(Layout_ObjectShown(store,o)&&Layout_ObjectShown(store,other));
    Object3D* outside=Layout_ObjectStore_Find(store,ids[2]);TEST_ASSERT(Layout_SetEntityHidden(store,outside->coreMeta.object_id,false));TEST_ASSERT(!Layout_ObjectShown(store,outside));
    TEST_ASSERT(CameraView_Enter(s,false));s->editor.selectedObject3DId=ids[0];TEST_ASSERT(CameraView_Frame(s,true));CameraView_Exit(s);
    s->spaceMode=SPACE_MODE_3D;s->freeViewCamera.enabled=false;s->activePlane=(ViewPlane){.axis=VIEW_PLANE_XY};s->grid.scale=100;
    TEST_ASSERT(Layout_IsolateEntity(store,o->coreMeta.object_id));Global_FlagHitboxesDirty();Global_RebuildHitboxesIfDirty();
    Vec2 point=WorldToScreen((Vec2){o->transform.position.x,o->transform.position.y},&s->grid);
    Hitbox hit=HitboxSystem_GetHitAtOfType((int)point.x,(int)point.y,HITBOX_OBJECT3D);TEST_ASSERT(hit.type==HITBOX_OBJECT3D&&hit.index==(int)ids[0]);
    Layout_RestoreEntityVisibility(store);Global_FlagHitboxesDirty();Global_RebuildHitboxesIfDirty();
    hit=HitboxSystem_GetHitAtOfType((int)point.x,(int)point.y,HITBOX_OBJECT3D);TEST_ASSERT(hit.type!=HITBOX_OBJECT3D);
    TEST_ASSERT(!memcmp(&filter,&store->view_query,sizeof(filter)) && store->hidden_view_count==1 && !strcmp(store->hidden_view_ids[0],saved_filter));char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && !strcmp(before,after)&&Editor_UndoCount(&s->editor)==0);free(before);free(after);ld_test_shutdown_runtime();return true;
}
static bool interior_preset_header_reveal(void) {
    const char* path=getenv("SCULPT_VISIBILITY_PRESET");if(!path)return true;
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&s->layout,path));Global_OnLayoutLoaded(path);Editor_ClearHistory(&s->editor);LDViewControls_Close();
    LayoutObjectStore* store=&s->layout.objectStore;Object3D* roof=NULL;
    for(size_t i=0;i<store->count;++i)if(strstr(store->items[i].info.label,"PROVISIONAL crowned vehicle roof skin")){roof=&store->items[i];break;}
    TEST_ASSERT(roof && !roof->coreMeta.flags.visible && !Layout_ObjectShown(store,roof));char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(click(LD_VIEW_VISIBILITY)&&click(LD_VIEW_SEARCH));SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,32,"crowned vehicle roof skin");TEST_ASSERT(LDViewControls_Event(&text));
    TEST_ASSERT(LDViewControls_MatchingCount()>=1);size_t row=0;LDVisibilityRow item;while(LDVisibilityTree_Row(row,&item)&&strcmp(item.id,roof->coreMeta.object_id))++row;TEST_ASSERT(row<20);TEST_ASSERT(click(LD_VIEW_ROW+(int)row));TEST_ASSERT(Layout_ObjectShown(store,roof));
    TEST_ASSERT(click(LD_VIEW_ROW+(int)row));TEST_ASSERT(!Layout_ObjectShown(store,roof));TEST_ASSERT(click(LD_VIEW_RESTORE));TEST_ASSERT(!Layout_ObjectShown(store,roof));
    const char* group=roof->info.parent_id;TEST_ASSERT(group[0]);TEST_ASSERT(Layout_SetEntityHidden(store,group,false));TEST_ASSERT(Layout_EntityVisibilityState(store,group)==1 && Layout_ObjectShown(store,roof));
    TEST_ASSERT(Layout_SetEntityHidden(store,group,true));TEST_ASSERT(!Layout_ObjectShown(store,roof));
    TEST_ASSERT(Layout_IsolateEntity(store,group));TEST_ASSERT(Layout_ObjectShown(store,roof));
    TEST_ASSERT(CameraView_Enter(s,false));s->editor.selectedObject3DId=roof->objectId;TEST_ASSERT(CameraView_Frame(s,true));
    SpaceViewContext v=SpaceAdapter_BuildViewContext(s);SDL_Rect viewport={(int)s->cameraView.viewport[0],(int)s->cameraView.viewport[1],(int)s->cameraView.viewport[2],(int)s->cameraView.viewport[3]};
    const LayoutMeshViewPlan* plan=Layout_MeshViewPlanBuild(&s->layout,&v,&s->grid,viewport,false,false,roof->objectId,120000);
    TEST_ASSERT(plan && plan->entries[roof-store->items].visible);CameraView_Exit(s);s->editor.selectedObject3DId=0;
    TEST_ASSERT(click(LD_VIEW_RESTORE));TEST_ASSERT(!Layout_ObjectShown(store,roof)&&!roof->coreMeta.flags.visible);
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before && after && !strcmp(before,after)&&Editor_UndoCount(&s->editor)==0);
    free(before);free(after);LDViewControls_Close();Layout_MeshViewPlanShutdown();ld_test_shutdown_runtime();return true;
}
static bool functional_presets_exclude_overlapping_physical_views(void) {
    const char* path=getenv("SCULPT_VISIBILITY_PRESET");if(!path)return true;
    ld_test_init_runtime();GlobalState* state=Global_Get();
    TEST_ASSERT(Layout_LoadFromFile(&state->layout,path));Editor_ClearHistory(&state->editor);
    LayoutObjectStore* store=&state->layout.objectStore;
    char* before=Layout_SaveToString(&state->layout);
    size_t counts[8]={0};
    for(size_t i=0;i<store->count;++i) if(!store->items[i].isDeleted) ++counts[Layout_GuideCategory(store,&store->items[i])];
    TEST_ASSERT(counts[1] && counts[2] && counts[7]);
    for(size_t v=0;v<store->saved_view_count;++v) {
        const LayoutSavedView* view=&store->saved_views[v];if(!Layout_IsGuideView(view))continue;
        Layout_ShowAllViews(store);LDVisibilityTree_Build(store,"");
        TEST_ASSERT(LDVisibilityTree_Hide(store,"@guides",false));
        Layout_ToggleView(store,view->id);
        size_t matched=0;
        for(size_t i=0;i<store->count;++i) {
            const Object3D* object=&store->items[i];if(object->isDeleted)continue;
            if(Layout_GuideViewMatches(store,object,view)) {
                ++matched;TEST_ASSERT(!Layout_ObjectShown(store,object));
                TEST_ASSERT(!Layout_EntityShown(store,object->coreMeta.object_id));
            } else if(!Layout_GuideCategory(store,object)) TEST_ASSERT(Layout_ObjectShown(store,object)==object->coreMeta.flags.visible);
        }
        TEST_ASSERT(matched>0);
        Layout_ToggleView(store,view->id);
        for(size_t i=0;i<store->count;++i) if(Layout_GuideViewMatches(store,&store->items[i],view)) TEST_ASSERT(Layout_ObjectShown(store,&store->items[i]));
    }
    Layout_ShowAllViews(store);LDVisibilityTree_Restore(store);
    char* after=Layout_SaveToString(&state->layout);
    TEST_ASSERT(before && after && !strcmp(before,after) && Editor_UndoCount(&state->editor)==0);
    free(before);free(after);ld_test_shutdown_runtime();return true;
}
bool visibility_reveal_run_tests(void) {
    const TestCase cases[]={{"inspection_reveal_precedence_pick_restore",inspection_reveal_precedence},{"interior_preset_header_roof_reveal",interior_preset_header_reveal},{"functional_guide_presets_exclude_overlapping_physical_views",functional_presets_exclude_overlapping_physical_views}};
    return run_test_cases("VisibilityReveal",cases,sizeof(cases)/sizeof(cases[0]));
}
