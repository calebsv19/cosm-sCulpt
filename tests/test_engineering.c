#include "test_layout_internal.h"
#include "Layout/layout_engineering.h"
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_right_scroll.h"
#include "Input/input_handler.h"
#include "core_scene_compile.h"
#include "Tools/canonical_scene_export.h"
#include <unistd.h>

static uint32_t part(const char* id,Vec3 origin) {
    RectPrismPrimitiveCreateParams p={.width=1,.height=.5f,.depth=.2f,.useExplicitFrame=true,
        .explicitFrame={.origin=origin,.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t handle=0;
    if(!Layout_CreateRectPrismPrimitive(&Global_Get()->layout,&p,&handle,NULL))return 0;
    Object3D* o=Layout_ObjectStore_Find(&Global_Get()->layout.objectStore,handle);
    if(core_object_set_identity(&o->coreMeta,id,"rect_prism_primitive").code!=CORE_OK)return 0;
    return handle;
}
static bool create(const char* name,const char* parent,Vec3 origin,char id[64]) {
    LayoutAssembly a={.frame={.origin=origin,.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(a.info.label,sizeof(a.info.label),"%s",name);snprintf(a.info.entity_type,64,"Assembly");snprintf(a.info.parent_id,64,"%s",parent?parent:"");
    Layout* l=&Global_Get()->layout;
    if(!Layout_EditAssembly(l,&a,NULL,Layout_GeometryHistory,NULL))return false;
    snprintf(id,64,"%s",l->objectStore.assemblies[l->objectStore.assembly_count-1].id);return true;
}
static bool parent(const char* id,const char* assembly) {
    Layout* l=&Global_Get()->layout;const LayoutEntityInfo* saved=Layout_EntityInfo(&l->objectStore,id);if(!saved)return false;
    LayoutEntityInfo info=*saved;snprintf(info.parent_id,64,"%s",assembly);
    return Layout_SetEntityInfo(l,id,&info,Layout_GeometryHistory,NULL);
}
static bool same_json(Layout* l,const char* before) {
    char* after=Layout_SaveToString(l);bool same=after && !strcmp(before,after);Layout_FreeString(after);return same;
}
static bool test_semantics_query_identity_history(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(part("node",(Vec3){0}));
    LayoutEntityInfo info={.reference=true,.property_count=4};snprintf(info.label,96,"Water node");snprintf(info.entity_type,64,"Controller");
    info.properties[0]=(LayoutProperty){.kind=LAYOUT_PROPERTY_TEXT};snprintf(info.properties[0].key,48,"subsystem");snprintf(info.properties[0].text,128,"electrical.data");
    info.properties[1]=(LayoutProperty){.kind=LAYOUT_PROPERTY_LENGTH,.number=.1};snprintf(info.properties[1].key,48,"clearance");
    info.properties[2]=(LayoutProperty){.kind=LAYOUT_PROPERTY_BOOL,.number=1};snprintf(info.properties[2].key,48,"serviceable");
    info.properties[3]=(LayoutProperty){.kind=LAYOUT_PROPERTY_NUMBER,.number=24};snprintf(info.properties[3].key,48,"voltage");
    Editor_ClearHistory(&s->editor);TEST_ASSERT(Layout_SetEntityInfo(&s->layout,"node",&info,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1 && !strcmp(s->layout.objectStore.items[0].coreMeta.object_id,"node"));
    LayoutEntityQuery q={.designation=2};snprintf(q.entity_type,64,"Controller");snprintf(q.property_key,48,"subsystem");snprintf(q.property_value,128,"electrical.data");
    char ids[1][64];TEST_ASSERT(Layout_QueryEntities(&s->layout.objectStore,&q,ids,1)==1 && !strcmp(ids[0],"node"));
    q.designation=1;TEST_ASSERT(Layout_QueryEntities(&s->layout.objectStore,&q,NULL,0)==0);q.designation=2;
    snprintf(q.property_key,48,"clearance");snprintf(q.property_value,128,"0.1");TEST_ASSERT(Layout_QueryMatches(&s->layout.objectStore,"node",&q));
    snprintf(q.property_key,48,"serviceable");snprintf(q.property_value,128,"true");TEST_ASSERT(Layout_QueryMatches(&s->layout.objectStore,"node",&q));
    snprintf(q.property_key,48,"voltage");snprintf(q.property_value,128,"24");TEST_ASSERT(Layout_QueryMatches(&s->layout.objectStore,"node",&q));
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));TEST_ASSERT(!Layout_EntityInfo(&s->layout.objectStore,"node")->label[0]);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));TEST_ASSERT(!strcmp(Layout_EntityInfo(&s->layout.objectStore,"node")->label,"Water node"));
    char* before=Layout_SaveToString(&s->layout);size_t count=Editor_UndoCount(&s->editor);
    info.properties[1].number=NAN;TEST_ASSERT(!Layout_SetEntityInfo(&s->layout,"node",&info,Layout_GeometryHistory,NULL));
    TEST_ASSERT(same_json(&s->layout,before) && Editor_UndoCount(&s->editor)==count);Layout_FreeString(before);
    ld_test_shutdown_runtime();return true;
}
static bool test_nested_move_reparent_local_frames(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char root[64],child[64],other[64];
    TEST_ASSERT(part("panel",(Vec3){2,0,0}));TEST_ASSERT(create("Van",NULL,(Vec3){0},root));TEST_ASSERT(create("Cabinet",root,(Vec3){1,0,0},child));TEST_ASSERT(create("Water",NULL,(Vec3){10,0,0},other));TEST_ASSERT(parent("panel",child));
    PlaneFrame3 local;TEST_ASSERT(Layout_EntityLocalFrame(&s->layout,"panel",&local) && ld_test_vec3_nearly_equal(local.origin,(Vec3){1,0,0}));
    Editor_ClearHistory(&s->editor);double delta[]={.025,0,0};
    TEST_ASSERT(Layout_MoveAssembly(&s->layout,root,delta,(Vec3){0,0,90},Layout_GeometryHistory,NULL));
    TEST_ASSERT(ld_test_vec3_nearly_equal(s->layout.objectStore.items[0].transform.position,(Vec3){.025f,2,0}));
    TEST_ASSERT(ld_test_vec3_nearly_equal(Layout_FindAssembly(&s->layout.objectStore,child)->frame.origin,(Vec3){.025f,1,0}));
    TEST_ASSERT(Layout_EntityLocalFrame(&s->layout,"panel",&local) && ld_test_vec3_nearly_equal(local.origin,(Vec3){1,0,0}));
    TEST_ASSERT(ld_test_vec3_nearly_equal(local.axisU,(Vec3){1,0,0}) && Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && s->layout.objectStore.items[0].transform.position.x==2);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));Object3D pose=s->layout.objectStore.items[0];
    TEST_ASSERT(parent("panel",other));TEST_ASSERT(ld_test_vec3_nearly_equal(pose.transform.position,s->layout.objectStore.items[0].transform.position));
    TEST_ASSERT(!strcmp(s->layout.objectStore.items[0].coreMeta.object_id,"panel"));
    LayoutEntityQuery q={0};snprintf(q.assembly_id,64,"%s",other);TEST_ASSERT(Layout_QueryEntities(&s->layout.objectStore,&q,NULL,0)==2);
    LayoutEntityInfo info=*Layout_EntityInfo(&s->layout.objectStore,child);snprintf(info.parent_id,64,"%s",other);TEST_ASSERT(Layout_SetEntityInfo(&s->layout,child,&info,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_IsDescendant(&s->layout.objectStore,child,other));
    ld_test_shutdown_runtime();return true;
}
static LayoutConstraint gap(const char* a,const char* b,double meters) {
    LayoutConstraint c={.axis={1,0,0},.target=meters};snprintf(c.a.entity_id,64,"%s",a);snprintf(c.b.entity_id,64,"%s",b);return c;
}
static bool test_group_constraints_bounds_rollback(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char group[64],nested[64];
    uint32_t a=part("A",(Vec3){0}),b=part("B",(Vec3){1,0,0});TEST_ASSERT(a && b);
    TEST_ASSERT(create("Group",NULL,(Vec3){0},group) && create("Nested",group,(Vec3){0},nested));TEST_ASSERT(parent("A",nested));
    LayoutConstraint c=gap("A","B",1);TEST_ASSERT(Layout_ConstraintEdit(&s->layout,&c,NULL,Layout_GeometryHistory,NULL));
    Editor_ClearHistory(&s->editor);double delta[]={.5,0,0};TEST_ASSERT(Layout_MoveAssembly(&s->layout,group,delta,(Vec3){0},Layout_GeometryHistory,NULL));
    TEST_ASSERT(s->layout.objectStore.items[0].transform.position.x==.5f && s->layout.objectStore.items[1].transform.position.x==1.5f && Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(parent("B",group));char* before=Layout_SaveToString(&s->layout);size_t undo=Editor_UndoCount(&s->editor);
    TEST_ASSERT(!Layout_MoveAssembly(&s->layout,group,(double[]){0,0,0},(Vec3){0,0,90},Layout_GeometryHistory,NULL));
    TEST_ASSERT(same_json(&s->layout,before) && Editor_UndoCount(&s->editor)==undo);
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,a);o->coreMeta.flags.locked=true;Layout_FreeString(before);before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(!Layout_MoveAssembly(&s->layout,group,delta,(Vec3){0},Layout_GeometryHistory,NULL) && same_json(&s->layout,before));Layout_FreeString(before);
    o->coreMeta.flags.locked=false;o->rectPrism.lockToBounds=true;s->layout.scene3d.bounds=(SceneBounds3D){.enabled=true,.clampOnEdit=true,.min={-2,-2,-2},.max={2,2,2}};
    before=Layout_SaveToString(&s->layout);TEST_ASSERT(!Layout_MoveAssembly(&s->layout,group,(double[]){20,0,0},(Vec3){0},Layout_GeometryHistory,NULL) && same_json(&s->layout,before));Layout_FreeString(before);
    ld_test_shutdown_runtime();return true;
}
static bool test_cycles_deletion_capacity_history_failure(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char root[64],child[64];TEST_ASSERT(part("A",(Vec3){0}));
    TEST_ASSERT(create("Root",NULL,(Vec3){0},root) && create("Child",root,(Vec3){0},child));TEST_ASSERT(parent("A",child));
    LayoutEntityInfo info=*Layout_EntityInfo(&s->layout.objectStore,root);snprintf(info.parent_id,64,"%s",child);
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(!Layout_SetEntityInfo(&s->layout,root,&info,Layout_GeometryHistory,NULL));
    TEST_ASSERT(!Layout_EditAssembly(&s->layout,NULL,root,Layout_GeometryHistory,NULL));TEST_ASSERT(!Layout_EditAssembly(&s->layout,NULL,child,Layout_GeometryHistory,NULL));TEST_ASSERT(same_json(&s->layout,before));Layout_FreeString(before);
    TEST_ASSERT(parent("A",""));TEST_ASSERT(Layout_EditAssembly(&s->layout,NULL,child,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_EditAssembly(&s->layout,NULL,root,Layout_GeometryHistory,NULL));TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(Layout_FindAssembly(&s->layout.objectStore,root));
    info=*Layout_EntityInfo(&s->layout.objectStore,"A");snprintf(info.parent_id,64,"missing");before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(!Layout_SetEntityInfo(&s->layout,"A",&info,Layout_GeometryHistory,NULL) && same_json(&s->layout,before));Layout_FreeString(before);
    info.parent_id[0]=0;info.property_count=2;snprintf(info.properties[0].key,48,"duplicate");snprintf(info.properties[1].key,48,"duplicate");
    TEST_ASSERT(!Layout_SetEntityInfo(&s->layout,"A",&info,Layout_GeometryHistory,NULL));
    for(int i=(int)s->layout.objectStore.assembly_count;i<LAYOUT_MAX_ASSEMBLIES;++i){char id[64];TEST_ASSERT(create("Group",NULL,(Vec3){0},id));}
    before=Layout_SaveToString(&s->layout);TEST_ASSERT(!create("Overflow",NULL,(Vec3){0},child) && same_json(&s->layout,before));Layout_FreeString(before);
    ld_test_shutdown_runtime();return true;
}
static bool test_persistence_export_and_malformed_atomic(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char root[64],child[64];s->layout.metersPerWorldUnit=.01;
    TEST_ASSERT(part("panel",(Vec3){100,0,0}));TEST_ASSERT(create("Van",NULL,(Vec3){0},root) && create("Cabinet",root,(Vec3){100,0,0},child));TEST_ASSERT(parent("panel",child));
    LayoutEntityInfo info=*Layout_EntityInfo(&s->layout.objectStore,"panel");snprintf(info.label,96,"Left panel");snprintf(info.entity_type,64,"Panel");info.property_count=1;info.properties[0]=(LayoutProperty){.kind=LAYOUT_PROPERTY_LENGTH,.number=.0127};snprintf(info.properties[0].key,48,"thickness");
    TEST_ASSERT(Layout_SetEntityInfo(&s->layout,"panel",&info,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_MoveAssembly(&s->layout,root,(double[]){.025,0,0},(Vec3){0,0,30},Layout_GeometryHistory,NULL));
    char* json=Layout_SaveToString(&s->layout);TEST_ASSERT(json);Layout loaded;Layout_Init(&loaded,1);TEST_ASSERT(Layout_LoadFromString(&loaded,json));
    TEST_ASSERT(loaded.objectStore.assembly_count==2 && !strcmp(loaded.objectStore.items[0].info.parent_id,child) && loaded.objectStore.items[0].info.properties[0].number==.0127);
    for(int bad=0;bad<7;++bad) {
        cJSON* o=cJSON_Parse(json);cJSON* engineering=cJSON_GetObjectItemCaseSensitive(o,"engineering");cJSON* assemblies=cJSON_GetObjectItemCaseSensitive(engineering,"assemblies");cJSON* first=cJSON_GetArrayItem(assemblies,0);cJSON* entity=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(engineering,"entities"),0);
        if(bad==0)cJSON_ReplaceItemInObjectCaseSensitive(first,"parent",cJSON_CreateString(child));
        if(bad==1)cJSON_ReplaceItemInObjectCaseSensitive(entity,"parent",cJSON_CreateString("missing"));
        if(bad==2)cJSON_ReplaceItemInObjectCaseSensitive(first,"id",cJSON_CreateString("panel"));
        if(bad==3)cJSON_ReplaceItemInArray(cJSON_GetObjectItemCaseSensitive(first,"worldFrame"),3,cJSON_CreateNumber(2));
        if(bad==4)cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(engineering,"entities"),cJSON_Duplicate(entity,1));
        if(bad==5)cJSON_DeleteItemFromObjectCaseSensitive(o,"engineering");
        if(bad==6)cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(o,"file"),"schemaVersion",cJSON_CreateNumber(14));
        char* broken=cJSON_PrintUnformatted(o);TEST_ASSERT(!Layout_LoadFromString(&loaded,broken));TEST_ASSERT(same_json(&loaded,json));cJSON_free(broken);cJSON_Delete(o);
    }
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(&s->layout,"s2_review"),*compiled=NULL;char diagnostics[256];
    TEST_ASSERT(authored && core_scene_compile_authoring_to_runtime(authored,&compiled,diagnostics,sizeof(diagnostics)).code==CORE_OK);
    cJSON* runtime=cJSON_Parse(compiled);TEST_ASSERT(runtime);cJSON* objects=cJSON_GetObjectItemCaseSensitive(runtime,"objects");const cJSON* projected=NULL;
    for(int i=0;i<cJSON_GetArraySize(objects);++i){const cJSON* item=cJSON_GetArrayItem(objects,i);const cJSON* id=cJSON_GetObjectItemCaseSensitive(item,"object_id");if(cJSON_IsString(id) && !strcmp(id->valuestring,"panel"))projected=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(item,"extensions"),"engineering");}
    TEST_ASSERT(projected && !strcmp(cJSON_GetObjectItemCaseSensitive(projected,"name")->valuestring,"Left panel"));
    free(authored);free(compiled);cJSON_Delete(runtime);Layout_Free(&loaded);Layout_FreeString(json);
    ld_test_shutdown_runtime();return true;
}
static bool click(int action) {
    if ((action >= PARTS_OBJECTS && action <= PARTS_LINKS) || action == PARTS_VOLUMES || action == PARTS_CHECKS)
        return ld_test_context_part(action);

    SDL_Rect rect;TEST_ASSERT(UIPanel_PartsControlRect(action,&rect));TEST_ASSERT(rect.y>=UIPanel_Get()->rightBodyRect.y && rect.y+rect.h<=UIPanel_Get()->rightBodyRect.y+UIPanel_Get()->rightBodyRect.h);
    AppContext ctx={0};SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=rect.x+rect.w/2;e.button.y=rect.y+rect.h/2;Input_Handle(&ctx,&e);return true;
}
static bool text(int action,const char* value) {
    TEST_ASSERT(click(action));AppContext ctx={0};SDL_Event e={.type=SDL_TEXTINPUT};snprintf(e.text.text,sizeof(e.text.text),"%s",value);Input_Handle(&ctx,&e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;Input_Handle(&ctx,&e);return true;
}
static bool test_parts_mouse_workflow_filter_history(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();uint32_t a=part("A",(Vec3){0});TEST_ASSERT(a && part("B",(Vec3){2,0,0}));
    Global_SetWindowSize(1400,1400);UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_PARTS);UIPanel_OnWindowResized(1400,1400);s->editor.selectedObject3DId=a;UIPanel_LayoutParts();
    TEST_ASSERT(text(PARTS_NAME,"Water controller"));TEST_ASSERT(click(PARTS_TYPE) && click(1005)); /* Controller */
    TEST_ASSERT(click(PARTS_PROPERTIES));TEST_ASSERT(text(PARTS_KEY,"subsystem") && text(PARTS_VALUE,"electrical"));TEST_ASSERT(click(PARTS_SET_PROPERTY) && click(PARTS_SAVE));
    TEST_ASSERT(!strcmp(Layout_EntityInfo(&s->layout.objectStore,"A")->entity_type,"Controller"));
    TEST_ASSERT(click(PARTS_ASSEMBLIES) && click(PARTS_NEW));TEST_ASSERT(text(PARTS_NAME,"Water cabinet") && click(PARTS_SAVE));char id[64];snprintf(id,64,"%s",UIPanel_Get()->parts.id);
    TEST_ASSERT(click(PARTS_ADD_SELECTED) && !strcmp(Layout_EntityInfo(&s->layout.objectStore,"A")->parent_id,id));
    TEST_ASSERT(click(PARTS_MOVE));TEST_ASSERT(text(PARTS_DX,"25 mm") && text(PARTS_DY,"0") && text(PARTS_DZ,"0") && text(PARTS_ANGLE,"30"));
    Editor_ClearHistory(&s->editor);TEST_ASSERT(click(PARTS_APPLY_MOVE));TEST_ASSERT(fabs(s->layout.objectStore.items[0].transform.position.x-.025)<1e-6 && Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));UIPanel_LayoutParts();TEST_ASSERT(s->layout.objectStore.items[0].transform.position.x==0);
    TEST_ASSERT(click(PARTS_FILTERS) && click(PARTS_CUSTOM_FILTER));TEST_ASSERT(text(PARTS_KEY,"subsystem") && text(PARTS_VALUE,"electrical"));s->layoutDirtySinceSave=false;size_t undo=Editor_UndoCount(&s->editor);TEST_ASSERT(click(PARTS_APPLY_FILTER));
    TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,&s->layout.objectStore.items[0]) && !Layout_ObjectShown(&s->layout.objectStore,&s->layout.objectStore.items[1]));
    TEST_ASSERT(!s->layoutDirtySinceSave && Editor_UndoCount(&s->editor)==undo);
    TEST_ASSERT(click(PARTS_CLEAR_FILTER) && Layout_ObjectShown(&s->layout.objectStore,&s->layout.objectStore.items[1]));
    TEST_ASSERT(click(PARTS_OBJECTS) && click(PARTS_SELECT) && click(1000));UIPanel_LayoutParts();TEST_ASSERT(text(PARTS_NAME,"Renamed") && click(PARTS_SAVE));TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));UIPanel_LayoutParts();TEST_ASSERT(!strcmp(UIPanel_Get()->parts.draft.label,"Water controller"));
    ld_test_shutdown_runtime();return true;
}
static bool reject_history(const Layout* l,void* context) {(void)l;(void)context;return false;}
static bool test_history_reservation_noop_and_legacy(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(part("A",(Vec3){0}));char group[64];TEST_ASSERT(create("Group",NULL,(Vec3){0},group) && parent("A",group));
    Editor_ClearHistory(&s->editor);s->layoutDirtySinceSave=false;char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(!Layout_MoveAssembly(&s->layout,group,(double[]){.1,0,0},(Vec3){0},reject_history,NULL));
    TEST_ASSERT(same_json(&s->layout,before) && !s->layoutDirtySinceSave && !Editor_UndoCount(&s->editor));
    TEST_ASSERT(Layout_MoveAssembly(&s->layout,group,(double[]){0,0,0},(Vec3){0},Layout_GeometryHistory,NULL));
    TEST_ASSERT(same_json(&s->layout,before) && !Editor_UndoCount(&s->editor));Layout_FreeString(before);
    TEST_ASSERT(parent("A","") && Layout_EditAssembly(&s->layout,NULL,group,Layout_GeometryHistory,NULL));
    char* json=Layout_SaveToString(&s->layout);cJSON* root=cJSON_Parse(json);cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(14));cJSON_DeleteItemFromObjectCaseSensitive(root,"engineering");char* legacy=cJSON_PrintUnformatted(root);
    TEST_ASSERT(Layout_LoadFromString(&s->layout,legacy) && !Layout_HasEngineeringData(&s->layout));cJSON_Delete(root);cJSON_free(legacy);Layout_FreeString(json);
    /* Generated assembly identities must not reuse a deleted object's imported ID. */
    Object3D* o=&s->layout.objectStore.items[0];TEST_ASSERT(core_object_set_identity(&o->coreMeta,"assembly_1","rect_prism_primitive").code==CORE_OK);
    TEST_ASSERT(Layout_ObjectStore_Delete(&s->layout.objectStore,o->objectId));TEST_ASSERT(create("Unique",NULL,(Vec3){0},group) && !strcmp(group,"assembly_2"));
    ld_test_shutdown_runtime();return true;
}
static bool test_query_hitboxes_and_filter_persistence(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();uint32_t a=part("A",(Vec3){0});TEST_ASSERT(a);
    Global_RebuildHitboxesIfDirty();Vec2 point=WorldToScreen(Vec3_ProjectToView((Vec3){0},s->activePlane,&s->freeViewCamera),&s->grid);
    Hitbox hit=HitboxSystem_GetHitAtOfType((int)point.x,(int)point.y,HITBOX_OBJECT3D);TEST_ASSERT(hit.type==HITBOX_OBJECT3D && hit.index==(int)a);
    char* before=Layout_SaveToString(&s->layout);snprintf(s->layout.objectStore.view_query.entity_type,64,"Controller");Global_FlagHitboxesDirty();Global_RebuildHitboxesIfDirty();
    hit=HitboxSystem_GetHitAtOfType((int)point.x,(int)point.y,HITBOX_OBJECT3D);TEST_ASSERT(hit.type!=HITBOX_OBJECT3D && same_json(&s->layout,before));
    TEST_ASSERT(Layout_LoadFromString(&s->layout,before) && !strcmp(s->layout.objectStore.view_query.entity_type,"Controller"));
    memset(&s->layout.objectStore.view_query,0,sizeof(LayoutEntityQuery));Layout_FreeString(before);
    TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,&s->layout.objectStore.items[0]));
    ld_test_shutdown_runtime();return true;
}
static bool test_mesh_rigid_nested_frame(void) {
    const char* path="/private/tmp/ld_s2_mesh.runtime.json";
    const char* json="{\"schema_variant\":\"mesh_asset_runtime_v1\",\"asset_id\":\"s2_mesh\",\"source_asset_id\":\"s2_source\",\"vertex_count\":4,\"triangle_count\":2,\"local_bounds\":{\"min\":{\"x\":-1,\"y\":-1,\"z\":-1},\"max\":{\"x\":1,\"y\":1,\"z\":1}}}";
    ld_test_init_runtime();GlobalState* s=Global_Get();char root[64],child[64];uint32_t handle=0;
    Transform3D transform=Layout_Transform3D_Default();transform.position=(Vec3){2,1,0};transform.rotationDeg=(Vec3){20,30,40};transform.scale=(Vec3){2,1,.5f};
    TEST_ASSERT(ld_test_write_text_file_basic(path,json));
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&s->layout,path,&transform,&handle,NULL,0));
    char id[64];snprintf(id,64,"%s",s->layout.objectStore.items[0].coreMeta.object_id);
    TEST_ASSERT(create("Root",NULL,(Vec3){0},root) && create("Child",root,(Vec3){1,0,0},child) && parent(id,child));
    PlaneFrame3 before,after;TEST_ASSERT(Layout_EntityLocalFrame(&s->layout,id,&before));Editor_ClearHistory(&s->editor);
    TEST_ASSERT(Layout_MoveAssembly(&s->layout,root,(double[]){.025,0,0},(Vec3){30,20,40},Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_EntityLocalFrame(&s->layout,id,&after));
    TEST_ASSERT(ld_test_vec3_nearly_equal(before.origin,after.origin) && ld_test_vec3_nearly_equal(before.axisU,after.axisU) && ld_test_vec3_nearly_equal(before.axisV,after.axisV) && ld_test_vec3_nearly_equal(before.normal,after.normal));
    TEST_ASSERT(ld_test_vec3_nearly_equal(transform.scale,s->layout.objectStore.items[0].transform.scale) && Editor_UndoCount(&s->editor)==1);
    char* saved=Layout_SaveToString(&s->layout);TEST_ASSERT(saved && Layout_LoadFromString(&s->layout,saved));Layout_FreeString(saved);
    TEST_ASSERT(Layout_EntityLocalFrame(&s->layout,id,&after) && ld_test_vec3_nearly_equal(before.axisU,after.axisU));
    saved=Layout_SaveToString(&s->layout);size_t count=Editor_UndoCount(&s->editor);
    TEST_ASSERT(!Layout_MoveAssembly(&s->layout,root,(double[]){0,0,0},(Vec3){181,0,0},Layout_GeometryHistory,NULL) && same_json(&s->layout,saved) && Editor_UndoCount(&s->editor)==count);
    Layout_FreeString(saved);ld_test_shutdown_runtime();unlink(path);return true;
}
bool engineering_run_tests(void) {
    const TestCase cases[]={
        {"mesh_rigid_nested_frame",test_mesh_rigid_nested_frame},
        {"history_reservation_noop_legacy",test_history_reservation_noop_and_legacy},
        {"query_hitboxes_view_only",test_query_hitboxes_and_filter_persistence},
        {"semantics_query_identity_history",test_semantics_query_identity_history},
        {"nested_move_reparent_local_frames",test_nested_move_reparent_local_frames},
        {"group_constraints_bounds_rollback",test_group_constraints_bounds_rollback},
        {"cycles_deletion_capacity",test_cycles_deletion_capacity_history_failure},
        {"persistence_export_malformed_atomic",test_persistence_export_and_malformed_atomic},
        {"parts_mouse_filter_history",test_parts_mouse_workflow_filter_history}
    };
    return run_test_cases("Engineering",cases,sizeof(cases)/sizeof(cases[0]));
}
