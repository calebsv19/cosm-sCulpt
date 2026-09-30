#include "test_layout_internal.h"
#include "Layout/layout_spatial.h"
#include "UI/ui_panel_spatial.h"
#include "UI/ui_panel_shell.h"
#include "Input/input_handler.h"
#include "core_scene_compile.h"

static Layout* layout(void){return &Global_Get()->layout;}
static uint32_t part(const char* id,Vec3 center,float width,float height,float depth,float angle) {
    float a=angle*.017453292519943295f;
    RectPrismPrimitiveCreateParams p={.width=width,.height=height,.depth=depth,.useExplicitFrame=true,
        .explicitFrame={.origin=center,.axisU={cosf(a),sinf(a),0},.axisV={-sinf(a),cosf(a),0},.normal={0,0,1}}};
    uint32_t handle=0;if(!Layout_CreateRectPrismPrimitive(layout(),&p,&handle,NULL))return 0;
    Object3D* o=Layout_ObjectStore_Find(&layout()->objectStore,handle);
    if(core_object_set_identity(&o->coreMeta,id,"rect_prism_primitive").code!=CORE_OK)return 0;
    return handle;
}
static LayoutVolumeEdit spec(void) {
    LayoutVolumeEdit v={.role=LAYOUT_VOLUME_SERVICE,.size_meters={1,1,1}};snprintf(v.name,96,"Fuse access");return v;
}
static LayoutSpatialRule rule(const char* a,const char* b,double clearance) {
    LayoutSpatialRule r={.kind=LAYOUT_SPATIAL_CLEARANCE,.clearance_meters=clearance};snprintf(r.source,64,"%s",a);snprintf(r.target,64,"%s",b);return r;
}
static bool same(const char* before) {char* after=Layout_SaveToString(layout());bool result=after && !strcmp(before,after);Layout_FreeString(after);return result;}
static bool deny_history(const Layout* l,void* context){(void)l;(void)context;return false;}
static bool test_volume_atomic_history_precision(void) {
    ld_test_init_runtime();Editor_ClearHistory(&Global_Get()->editor);LayoutVolumeEdit v=spec();char* before=Layout_SaveToString(layout());
    TEST_ASSERT(!Layout_EditVolume(layout(),&v,false,deny_history,NULL) && same(before) && !layout()->objectStore.count);Layout_FreeString(before);
    TEST_ASSERT(Layout_EditVolume(layout(),&v,false,Layout_GeometryHistory,NULL));
    Object3D o=layout()->objectStore.items[0];v.object_id=o.objectId;TEST_ASSERT(o.info.volume_role==LAYOUT_VOLUME_SERVICE && Editor_UndoCount(&Global_Get()->editor)==1);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,layout()) && !Layout_ObjectStore_LiveCount(&layout()->objectStore));
    TEST_ASSERT(Editor_Redo(&Global_Get()->editor,layout()) && !strcmp(layout()->objectStore.items[0].coreMeta.object_id,o.coreMeta.object_id));
    v.size_meters[0]=.3;v.center_meters[1]=.025;
    TEST_ASSERT(Layout_EditVolume(layout(),&v,false,Layout_GeometryHistory,NULL));
    TEST_ASSERT(fabs(layout()->objectStore.items[0].rectPrism.width-.3)<1e-6 && fabs(layout()->objectStore.items[0].transform.position.y-.025)<1e-6);
    before=Layout_SaveToString(layout());size_t undo=Editor_UndoCount(&Global_Get()->editor);
    TEST_ASSERT(Layout_EditVolume(layout(),&v,false,Layout_GeometryHistory,NULL) && Editor_UndoCount(&Global_Get()->editor)==undo);
    v.size_meters[1]=-1;TEST_ASSERT(!Layout_EditVolume(layout(),&v,false,Layout_GeometryHistory,NULL) && same(before));
    v.size_meters[1]=1;snprintf(v.owner,64,"missing");TEST_ASSERT(!Layout_EditVolume(layout(),&v,false,Layout_GeometryHistory,NULL) && same(before));
    v.owner[0]=0;v.role=(LayoutVolumeRole)99;TEST_ASSERT(!Layout_EditVolume(layout(),&v,false,Layout_GeometryHistory,NULL) && same(before));
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_oriented_distance_contact_panels_mesh(void) {
    ld_test_init_runtime();TEST_ASSERT(part("A",(Vec3){0},1,1,1,0) && part("B",(Vec3){2,2,0},1,1,1,0));
    Object3D* a=&layout()->objectStore.items[0],*b=&layout()->objectStore.items[1];double d;bool hit,approx;
    TEST_ASSERT(Layout_SpatialDistance(layout(),a,b,&d,&hit,&approx) && !hit && !approx && fabs(d-sqrt(2))<1e-6);
    TEST_ASSERT(Layout_SetObject3DPosition(layout(),b->objectId,(Vec3){1,0,0},NULL));
    TEST_ASSERT(Layout_SpatialDistance(layout(),a,b,&d,&hit,&approx) && hit && d==0);
    /* Two diagonal bars have overlapping world AABBs but remain separated. */
    TEST_ASSERT(part("C",(Vec3){0},4,.1f,.2f,45) && part("D",(Vec3){-.212132f,.212132f,0},4,.1f,.2f,45));
    a=&layout()->objectStore.items[2];b=&layout()->objectStore.items[3];
    TEST_ASSERT(Layout_SpatialDistance(layout(),a,b,&d,&hit,&approx) && !hit && fabs(d-.2)<2e-6);
    layout()->metersPerWorldUnit=.01;TEST_ASSERT(Layout_SpatialDistance(layout(),a,b,&d,&hit,&approx) && fabs(d-.002)<1e-7);layout()->metersPerWorldUnit=1;
    PlanePrimitiveCreateParams panel={.width=1,.height=1,.useExplicitFrame=true,.explicitFrame={.origin={0,0,2},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};uint32_t id;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout(),&panel,&id,NULL));a=&layout()->objectStore.items[0];b=Layout_ObjectStore_Find(&layout()->objectStore,id);
    TEST_ASSERT(Layout_SpatialDistance(layout(),a,b,&d,&hit,&approx) && !hit && fabs(d-1.5)<1e-6);
    Object3D mesh=*a;mesh.kind=OBJECT3D_KIND_MESH_ASSET_INSTANCE;mesh.meshInstance=(MeshAssetInstance3D){.localBoundsMin={-.5,-.5,-.5},.localBoundsMax={.5,.5,.5},.vertexCount=8,.triangleCount=12};
    snprintf(mesh.meshInstance.assetId,64,"mesh");snprintf(mesh.meshInstance.runtimePath,512,"/tmp/proxy.json");
    TEST_ASSERT(Layout_SpatialDistance(layout(),&mesh,b,&d,&hit,&approx) && approx && fabs(d-1.5)<1e-6);
    ld_test_shutdown_runtime();return true;
}
static bool test_automatic_service_owner_filter_reference(void) {
    ld_test_init_runtime();uint32_t own=part("fuse",(Vec3){0},.2f,.2f,.2f,0);TEST_ASSERT(own && part("cabinet",(Vec3){.4f,0,0},.2f,.2f,.2f,0));
    LayoutVolumeEdit v=spec();snprintf(v.owner,64,"fuse");TEST_ASSERT(Layout_EditVolume(layout(),&v,false,Layout_GeometryHistory,NULL));
    LayoutSpatialResult results[8];TEST_ASSERT(Layout_CheckSpatial(layout(),results,8)==1 && results[0].severity==LAYOUT_SPATIAL_ERROR && !strcmp(results[0].target,"cabinet"));
    snprintf(layout()->objectStore.view_query.entity_type,64,"Hidden");layout()->objectStore.items[1].coreMeta.flags.visible=false;
    TEST_ASSERT(Layout_CheckSpatial(layout(),results,8)==1); /* Visibility cannot hide an obstruction. */
    LayoutEntityInfo info=layout()->objectStore.items[1].info;info.reference=true;TEST_ASSERT(Layout_SetEntityInfo(layout(),"cabinet",&info,NULL,NULL));
    TEST_ASSERT(Layout_CheckSpatial(layout(),results,8)==0 && !Layout_CanDeleteObject(&layout()->objectStore,own));
    LayoutSpatialRule r=rule("fuse","cabinet",.05);TEST_ASSERT(Layout_EditSpatialRule(layout(),&r,NULL,NULL,NULL));
    TEST_ASSERT(Layout_CheckSpatial(layout(),results,8)==1 && results[0].severity==LAYOUT_SPATIAL_PASS);
    ld_test_shutdown_runtime();return true;
}
static bool test_rules_assembly_history_bounds_report(void) {
    ld_test_init_runtime();TEST_ASSERT(part("A",(Vec3){0},1,1,1,0) && part("B",(Vec3){1.03f,0,0},1,1,1,0));
    LayoutAssembly assembly={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};snprintf(assembly.info.entity_type,64,"Assembly");
    TEST_ASSERT(Layout_EditAssembly(layout(),&assembly,NULL,NULL,NULL));char id[64];snprintf(id,64,"%s",layout()->objectStore.assemblies[0].id);
    LayoutEntityInfo info=layout()->objectStore.items[0].info;snprintf(info.parent_id,64,"%s",id);TEST_ASSERT(Layout_SetEntityInfo(layout(),"A",&info,NULL,NULL));
    LayoutSpatialRule r=rule(id,"B",.05);TEST_ASSERT(Layout_EditSpatialRule(layout(),&r,NULL,Layout_GeometryHistory,NULL));LayoutSpatialResult results[1];
    TEST_ASSERT(Layout_CheckSpatial(layout(),results,1)==1 && results[0].severity==LAYOUT_SPATIAL_ERROR && fabs(results[0].distance_meters-.03)<1e-6 && !strcmp(results[0].source,"A"));
    TEST_ASSERT(!Layout_CanDeleteObject(&layout()->objectStore,layout()->objectStore.items[1].objectId));
    r=layout()->objectStore.spatial_rules[0];r.clearance_meters=.02;TEST_ASSERT(Layout_EditSpatialRule(layout(),&r,NULL,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_CheckSpatial(layout(),results,1)==1 && results[0].severity==LAYOUT_SPATIAL_PASS);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,layout()) && Layout_CheckSpatial(layout(),results,1)==1 && results[0].severity==LAYOUT_SPATIAL_ERROR);
    cJSON* report=Layout_SpatialReportJson(layout());TEST_ASSERT(report && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(report,"truncated")));cJSON_Delete(report);
    TEST_ASSERT(Layout_SaveToFile(layout(),"/private/tmp/ld-s3bc-cli-layout.json"));
    LayoutSpatialRule duplicate=rule("B",id,.1);char* before=Layout_SaveToString(layout());
    TEST_ASSERT(!Layout_EditSpatialRule(layout(),&duplicate,NULL,NULL,NULL) && same(before));duplicate=rule("A","missing",0);TEST_ASSERT(!Layout_EditSpatialRule(layout(),&duplicate,NULL,NULL,NULL) && same(before));Layout_FreeString(before);
    TEST_ASSERT(Layout_EditSpatialRule(layout(),NULL,"spatial_1",Layout_GeometryHistory,NULL));
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,layout()) && Layout_FindSpatialRule(&layout()->objectStore,"spatial_1"));
    ld_test_shutdown_runtime();return true;
}
static bool test_schema17_migration_export_atomic(void) {
    ld_test_init_runtime();TEST_ASSERT(part("A",(Vec3){0},1,1,1,0) && part("B",(Vec3){2,0,0},1,1,1,0));
    char* legacy=Layout_SaveToString(layout());cJSON* old=cJSON_Parse(legacy);cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(old,"file"),"schemaVersion",cJSON_CreateNumber(16));
    cJSON* eng=cJSON_GetObjectItemCaseSensitive(old,"engineering");cJSON_DeleteItemFromObjectCaseSensitive(eng,"spatialChecks");cJSON_DeleteItemFromObjectCaseSensitive(eng,"nextSpatialCheckId");
    cJSON* entities=cJSON_GetObjectItemCaseSensitive(eng,"entities");for(int i=0;i<2;++i){cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetArrayItem(entities,i),"volumeRole");cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetArrayItem(entities,i),"volumeOwner");}
    char* downgraded=cJSON_PrintUnformatted(old);TEST_ASSERT(Layout_LoadFromString(layout(),downgraded));free(downgraded);cJSON_Delete(old);Layout_FreeString(legacy);
    LayoutVolumeEdit v=spec();TEST_ASSERT(Layout_EditVolume(layout(),&v,false,NULL,NULL));LayoutSpatialRule r=rule("A","B",.1);TEST_ASSERT(Layout_EditSpatialRule(layout(),&r,NULL,NULL,NULL));
    char* before=Layout_SaveToString(layout());TEST_ASSERT(Layout_LoadFromString(layout(),before) && same(before));
    for(int bad=0;bad<9;++bad) {
        cJSON* root=cJSON_Parse(before);eng=cJSON_GetObjectItemCaseSensitive(root,"engineering");cJSON* checks=cJSON_GetObjectItemCaseSensitive(eng,"spatialChecks");cJSON* first=cJSON_GetArrayItem(checks,0);entities=cJSON_GetObjectItemCaseSensitive(eng,"entities");cJSON* vol=cJSON_GetArrayItem(entities,2);
        if(bad==0)cJSON_DeleteItemFromObjectCaseSensitive(eng,"spatialChecks");
        if(bad==1)cJSON_ReplaceItemInObjectCaseSensitive(eng,"nextSpatialCheckId",cJSON_CreateNumber(1.5));
        if(bad==2)cJSON_ReplaceItemInObjectCaseSensitive(first,"source",cJSON_CreateString("missing"));
        if(bad==3)cJSON_ReplaceItemInObjectCaseSensitive(first,"clearanceMeters",cJSON_CreateNumber(-1));
        if(bad==4)cJSON_AddItemToArray(checks,cJSON_Duplicate(first,1));
        if(bad==5)cJSON_ReplaceItemInObjectCaseSensitive(vol,"volumeOwner",cJSON_CreateString("missing"));
        if(bad==6)cJSON_DeleteItemFromObjectCaseSensitive(vol,"volumeRole");
        if(bad==7)cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(16));
        if(bad==8)cJSON_ReplaceItemInObjectCaseSensitive(vol,"volumeRole",cJSON_CreateNumber(99));
        char* text=cJSON_PrintUnformatted(root);TEST_ASSERT(!Layout_LoadFromString(layout(),text) && same(before));free(text);cJSON_Delete(root);
    }
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(layout(),"s3bc_test");cJSON* scene=authored?cJSON_Parse(authored):NULL;TEST_ASSERT(scene);
    char* compiled=NULL;char diagnostic[256];TEST_ASSERT(core_scene_compile_authoring_to_runtime(authored,&compiled,diagnostic,sizeof(diagnostic)).code==CORE_OK);
    cJSON* runtime=cJSON_Parse(compiled);TEST_ASSERT(runtime && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(runtime,"objects"))==5);
    cJSON* preserved=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(runtime,"extensions"),"line_drawing"),"layout_snapshot");
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(preserved,"engineering"),"spatialChecks"))==1);
    cJSON_Delete(runtime);free(compiled);free(authored);
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(scene,"objects"))==5); /* Three existing layout carriers + two physical prisms. */
    cJSON* snapshot=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(scene,"extensions"),"line_drawing"),"layout_snapshot");
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(snapshot,"engineering"),"entities"))==3);
    cJSON_Delete(scene);Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool click(int action) {
    SDL_Rect r;TEST_ASSERT(UIPanel_PartsControlRect(action,&r));TEST_ASSERT(r.y>=UIPanel_Get()->rightBodyRect.y && r.y+r.h<=UIPanel_Get()->rightBodyRect.y+UIPanel_Get()->rightBodyRect.h);
    AppContext c={0};SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+r.w/2;e.button.y=r.y+r.h/2;Input_Handle(&c,&e);return true;
}
static bool text(int action,const char* value) {
    TEST_ASSERT(click(action));SDL_Event e={.type=SDL_TEXTINPUT};snprintf(e.text.text,sizeof(e.text.text),"%s",value);TEST_ASSERT(UIPanel_PartsEvent(&e));
    e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;TEST_ASSERT(UIPanel_PartsEvent(&e));return true;
}
static bool test_mouse_forms_checks_history_stale(void) {
    ld_test_init_runtime();TEST_ASSERT(part("A",(Vec3){0},1,1,1,0) && part("B",(Vec3){2,0,0},1,1,1,0));
    Global_SetWindowSize(1500,1600);UIPanel_OnWindowResized(1500,1600);TEST_ASSERT(UIPanel_PartsSelectEntity("A"));Editor_ClearHistory(&Global_Get()->editor);
    TEST_ASSERT(click(PARTS_VOLUMES) && text(PARTS_VOLUME_NAME,"Door travel") && text(PARTS_VOLUME_W,"300 mm") && click(PARTS_VOLUME_SAVE));
    TEST_ASSERT(layout()->objectStore.count==3 && fabs(layout()->objectStore.items[2].rectPrism.width-.3)<1e-6 && Editor_UndoCount(&Global_Get()->editor)==1);
    TEST_ASSERT(click(PARTS_VOLUME_SAVE) && Editor_UndoCount(&Global_Get()->editor)==1);
    TEST_ASSERT(click(PARTS_CHECKS) && click(PARTS_CHECK_RUN) && UIPanel_Get()->spatial.result_count==1 && UIPanel_Get()->spatial.results[0].severity==LAYOUT_SPATIAL_ERROR);
    TEST_ASSERT(click(9000) && click(PARTS_CHECK_SELECT_B) && Global_Get()->editor.selectedObject3DId==layout()->objectStore.items[0].objectId);
    TEST_ASSERT(click(PARTS_CHECK_RULES) && click(PARTS_CHECK_SOURCE) && click(1000) && click(PARTS_CHECK_TARGET) && click(1001) && click(PARTS_CHECK_KIND) && click(1001) && text(PARTS_CHECK_DISTANCE,"2 m") && click(PARTS_CHECK_SAVE));
    TEST_ASSERT(layout()->objectStore.spatial_rule_count==1 && !UIPanel_PartsControlRect(9000,NULL)); /* Stale results disappear. */
    TEST_ASSERT(click(PARTS_CHECK_RUN) && UIPanel_Get()->spatial.result_count==2);
    TEST_ASSERT(click(PARTS_CHECK_REMOVE) && click(PARTS_SPATIAL_CANCEL) && layout()->objectStore.spatial_rule_count==1);
    TEST_ASSERT(click(PARTS_CHECK_REMOVE) && click(PARTS_CHECK_CONFIRM_REMOVE) && !layout()->objectStore.spatial_rule_count);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,layout()));UIPanel_LayoutParts();TEST_ASSERT(click(8000) && !strcmp(UIPanel_Get()->spatial.rule.id,"spatial_1"));
    TEST_ASSERT(click(PARTS_VOLUMES) && click(PARTS_VOLUME_SELECT) && click(1002) && click(PARTS_VOLUME_REMOVE) && click(PARTS_VOLUME_CONFIRM_REMOVE));
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&layout()->objectStore)==2 && Editor_Undo(&Global_Get()->editor,layout()) && Layout_ObjectStore_LiveCount(&layout()->objectStore)==3);
    ld_test_shutdown_runtime();return true;
}
static bool test_capacity_unresolved_approximate(void) {
    ld_test_init_runtime();
    for(int i=0;i<9;++i){char id[64];snprintf(id,64,"part_%d",i);TEST_ASSERT(part(id,(Vec3){i*2.0f,0,0},1,1,1,0));}
    for(int a=0;a<9 && layout()->objectStore.spatial_rule_count<32;++a)for(int b=a+1;b<9 && layout()->objectStore.spatial_rule_count<32;++b) {
        char x[64],y[64];snprintf(x,64,"part_%d",a);snprintf(y,64,"part_%d",b);LayoutSpatialRule r=rule(x,y,.05);TEST_ASSERT(Layout_EditSpatialRule(layout(),&r,NULL,NULL,NULL));
    }
    LayoutSpatialRule r=rule("part_7","part_8",.05);char* before=Layout_SaveToString(layout());
    TEST_ASSERT(!Layout_EditSpatialRule(layout(),&r,NULL,NULL,NULL) && same(before));Layout_FreeString(before);
    LayoutSpatialResult out[1];TEST_ASSERT(Layout_CheckSpatial(layout(),out,1)==32 && out[0].severity==LAYOUT_SPATIAL_PASS);
    Object3D* mesh=&layout()->objectStore.items[0];mesh->kind=OBJECT3D_KIND_MESH_ASSET_INSTANCE;mesh->meshInstance=(MeshAssetInstance3D){.localBoundsMin={-.5,-.5,-.5},.localBoundsMax={.5,.5,.5},.vertexCount=8,.triangleCount=12};
    snprintf(mesh->meshInstance.assetId,64,"mesh");snprintf(mesh->meshInstance.runtimePath,512,"/tmp/proxy.json");
    TEST_ASSERT(Layout_CheckSpatial(layout(),out,1)==32 && out[0].severity==LAYOUT_SPATIAL_WARNING && out[0].approximate);
    mesh->kind=OBJECT3D_KIND_UNKNOWN;TEST_ASSERT(Layout_CheckSpatial(layout(),out,1)==32 && out[0].severity==LAYOUT_SPATIAL_WARNING && !out[0].measurable);
    ld_test_shutdown_runtime();return true;
}
static bool test_owner_assembly_movement_reserved_integrity(void) {
    ld_test_init_runtime();TEST_ASSERT(part("owner",(Vec3){0},.2f,.2f,.2f,0));
    LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};snprintf(a.info.entity_type,64,"Assembly");TEST_ASSERT(Layout_EditAssembly(layout(),&a,NULL,NULL,NULL));
    char id[64];snprintf(id,64,"%s",layout()->objectStore.assemblies[0].id);
    LayoutEntityInfo info=layout()->objectStore.items[0].info;snprintf(info.parent_id,64,"%s",id);TEST_ASSERT(Layout_SetEntityInfo(layout(),"owner",&info,NULL,NULL));
    LayoutVolumeEdit v=spec();snprintf(v.owner,64,"%s",id);TEST_ASSERT(Layout_EditVolume(layout(),&v,false,NULL,NULL));Object3D* volume=&layout()->objectStore.items[1];
    TEST_ASSERT(!Layout_CheckSpatial(layout(),NULL,0));info=volume->info;snprintf(info.parent_id,64,"%s",id);TEST_ASSERT(Layout_SetEntityInfo(layout(),volume->coreMeta.object_id,&info,NULL,NULL));
    TEST_ASSERT(Layout_MoveAssembly(layout(),id,(double[]){1,0,0},(Vec3){0,0,30},Layout_GeometryHistory,NULL));
    TEST_ASSERT(fabs(layout()->objectStore.items[1].transform.position.x-1)<1e-6 && !Layout_CheckSpatial(layout(),NULL,0));
    char* before=Layout_SaveToString(layout());Object3D next=layout()->objectStore.items[1];next.rectPrism.depth=0;
    TEST_ASSERT(!Layout_ReplaceGeometryObject(layout(),&next,Layout_GeometryHistory,NULL) && same(before));
    info=next.info;info.reference=true;TEST_ASSERT(!Layout_SetEntityInfo(layout(),next.coreMeta.object_id,&info,NULL,NULL) && same(before));Layout_FreeString(before);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,layout()) && fabs(layout()->objectStore.items[1].transform.position.x)<1e-6);
    ld_test_shutdown_runtime();return true;
}
bool spatial_run_tests(void) {
    const TestCase cases[]={
        {"volume_atomic_history_precision",test_volume_atomic_history_precision},
        {"oriented_distance_contact_panels_mesh",test_oriented_distance_contact_panels_mesh},
        {"automatic_owner_filter_reference",test_automatic_service_owner_filter_reference},
        {"rules_assembly_history_report",test_rules_assembly_history_bounds_report},
        {"schema17_migration_export_atomic",test_schema17_migration_export_atomic},
        {"mouse_forms_checks_history_stale",test_mouse_forms_checks_history_stale},
        {"capacity_unresolved_approximate",test_capacity_unresolved_approximate},
        {"owner_assembly_movement_integrity",test_owner_assembly_movement_reserved_integrity}
    };return run_test_cases("Spatial",cases,sizeof(cases)/sizeof(cases[0]));
}
