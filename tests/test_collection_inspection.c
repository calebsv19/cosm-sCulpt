#include "test_layout_internal.h"
#include "Layout/layout_collection_inspection.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_saved_views.h"
#include "UI/panel/ui_panel_collection_inspection.h"
#include "UI/ui_panel_measurement.h"
#include "Input/input_handler.h"

static uint32_t create(const char* identity,Vec3 position) {
    RectPrismPrimitiveCreateParams p={.width=1,.height=2,.depth=3,.useExplicitFrame=true,
        .explicitFrame={.origin=position,.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};uint32_t id=0;
    if(!Layout_CreateRectPrismPrimitive(&Global_Get()->layout,&p,&id,NULL))return 0;
    Object3D* o=Layout_ObjectStore_Find(&Global_Get()->layout.objectStore,id);
    if(core_object_set_identity(&o->coreMeta,identity,"rect_prism_primitive").code!=CORE_OK)return 0;
    return id;
}
static bool scope(void) {
    Layout* l=&Global_Get()->layout;LayoutSceneCollection c={0};snprintf(c.id,64,"@scene.pilot");snprintf(c.name,96,"Cabinet pilot");
    return Layout_EditSceneCollection(l,&c,NULL,NULL,NULL)&&Layout_AssignSceneCollection(l,c.id,"A",true,NULL,NULL)&&Layout_AssignSceneCollection(l,c.id,"B",true,NULL,NULL);
}
static LayoutSavedMeasurement measurement(int kind) {
    LayoutSavedMeasurement m={.kind=kind};snprintf(m.id,64,"dimension_1");snprintf(m.name,96,"Cabinet width");snprintf(m.collection,64,"@scene.pilot");snprintf(m.a.entity_id,64,"A");snprintf(m.b.entity_id,64,"B");return m;
}
static bool analytic_frames_and_visibility_independence(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;TEST_ASSERT(create("A",(Vec3){0})&&create("B",(Vec3){3,4,0})&&scope());
    Editor_ClearHistory(&Global_Get()->editor);char* before=Layout_SaveToString(l);
    LayoutCollectionInspection r=Layout_InspectCollection(l,"@scene.pilot","");TEST_ASSERT(r.status==LAYOUT_MEASUREMENT_OK&&r.parts==2);
    TEST_ASSERT(fabs(r.max_meters[0]-r.min_meters[0]-4)<1e-6&&fabs(r.max_meters[1]-r.min_meters[1]-6)<1e-6);
    TEST_ASSERT(Layout_SetEntityHidden(&l->objectStore,"@scene.pilot",true));LayoutCollectionInspection hidden=Layout_InspectCollection(l,"@scene.pilot","");
    TEST_ASSERT(!memcmp(r.min_meters,hidden.min_meters,sizeof(r.min_meters))&&!memcmp(r.max_meters,hidden.max_meters,sizeof(r.max_meters)));
    char* after=Layout_SaveToString(l);TEST_ASSERT(before&&after&&!strcmp(before,after)&&Editor_UndoCount(&Global_Get()->editor)==0);free(before);free(after);
    LayoutAssembly a={.frame={.origin={1,1,0},.axisU={0,1,0},.axisV={-1,0,0},.normal={0,0,1}}};snprintf(a.info.label,96,"Rotated datum");snprintf(a.info.entity_type,64,"Assembly");
    TEST_ASSERT(Layout_EditAssembly(l,&a,NULL,NULL,NULL));const char* frame=l->objectStore.assemblies[0].id;
    r=Layout_InspectCollection(l,"@scene.pilot",frame);TEST_ASSERT(r.status==LAYOUT_MEASUREMENT_OK&&fabs(r.max_meters[0]-r.min_meters[0]-6)<1e-6&&fabs(r.max_meters[1]-r.min_meters[1]-4)<1e-6);
    l->metersPerWorldUnit=.01;r=Layout_InspectCollection(l,"@scene.pilot",frame);TEST_ASSERT(fabs(r.max_meters[0]-r.min_meters[0]-.06)<1e-8);
    r=Layout_InspectCollection(l,"@scene.pilot","missing_frame");TEST_ASSERT(r.status==LAYOUT_MEASUREMENT_UNRESOLVED&&isnan(r.min_meters[0]));
    ld_test_shutdown_runtime();return true;
}
static bool mesh_envelopes_scale_rotation_and_datums(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;TEST_ASSERT(create("A",(Vec3){0})&&create("B",(Vec3){3,4,0})&&scope());
    Transform3D transform={.position={10,20,1},.rotationDeg={0,0,90},.scale={2,3,4}};uint32_t id=0;
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(l,"tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json",&transform,&id,NULL,0));
    Object3D* o=Layout_ObjectStore_Find(&l->objectStore,id);o->meshInstance.localBoundsMin=(Vec3){0,0,0};o->meshInstance.localBoundsMax=(Vec3){1,2,3};char entity[64];snprintf(entity,64,"%s",o->coreMeta.object_id);
    TEST_ASSERT(Layout_AssignSceneCollection(l,"@scene.pilot",entity,true,NULL,NULL));o=(Object3D*)Layout_InspectionObject(l,entity);
    double size[3];TEST_ASSERT(Layout_InspectionLocalSize(l,o,size)&&fabs(size[0]-2)<1e-6&&fabs(size[1]-6)<1e-6&&fabs(size[2]-12)<1e-6);
    Vec3 lo,hi;TEST_ASSERT(Layout_Object3D_ComputeWorldAABB(o,&lo,&hi));TEST_ASSERT(fabs((hi.x-lo.x)-6)<1e-5&&fabs((hi.y-lo.y)-2)<1e-5);
    LayoutInspectionDatum datum={.feature=3};snprintf(datum.entity_id,64,"%s",entity);double point[3];TEST_ASSERT(Layout_InspectionDatumPoint(l,&datum,point)==LAYOUT_MEASUREMENT_OK);
    TEST_ASSERT(fabs(point[0]-4)<1e-5&&fabs(point[1]-22)<1e-5&&fabs(point[2]-13)<1e-5);
    LayoutCollectionInspection r=Layout_InspectCollection(l,"@scene.pilot",entity);TEST_ASSERT(r.status==LAYOUT_MEASUREMENT_OK&&r.mesh_parts==1&&strstr(r.message,"conservative"));
    o=(Object3D*)Layout_InspectionObject(l,entity);o->coreMeta.flags.visible=false;r=Layout_InspectCollection(l,"@scene.pilot","");TEST_ASSERT(r.parts==2&&r.excluded==1);
    ld_test_shutdown_runtime();return true;
}
static bool measurements_undo_reload_and_dynamic_geometry(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;TEST_ASSERT(create("A",(Vec3){0})&&create("B",(Vec3){3,4,0})&&scope());Editor_ClearHistory(&Global_Get()->editor);
    LayoutSavedMeasurement m=measurement(3);TEST_ASSERT(Layout_SaveMeasurement(l,&m,NULL,Layout_GeometryHistory,NULL));TEST_ASSERT(Editor_UndoCount(&Global_Get()->editor)==1);
    TEST_ASSERT(fabs(Layout_EvaluateSavedMeasurement(l,&m).value-5)<1e-6);
    char* saved=Layout_SaveToString(l);TEST_ASSERT(saved&&strstr(saved,"measurements"));cJSON* root=cJSON_Parse(saved);TEST_ASSERT(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion")->valueint==29);cJSON_Delete(root);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,l)&&l->objectStore.measurement_count==0);TEST_ASSERT(Editor_Redo(&Global_Get()->editor,l)&&l->objectStore.measurement_count==1);
    TEST_ASSERT(Layout_LoadFromString(l,saved));TEST_ASSERT(fabs(Layout_EvaluateSavedMeasurement(l,&l->objectStore.measurements[0]).value-5)<1e-6);free(saved);
    Object3D* b=(Object3D*)Layout_InspectionObject(l,"B");b->transform.position.x=6;b->rectPrism.frame.origin.x=6;
    TEST_ASSERT(fabs(Layout_EvaluateSavedMeasurement(l,&m).value-sqrt(52))<1e-6);
    b->isDeleted=true;TEST_ASSERT(Layout_EvaluateSavedMeasurement(l,&m).status==LAYOUT_MEASUREMENT_UNRESOLVED);
    saved=Layout_SaveToString(l);TEST_ASSERT(saved&&Layout_LoadFromString(l,saved));free(saved);
    TEST_ASSERT(Layout_EvaluateSavedMeasurement(l,&l->objectStore.measurements[0]).status==LAYOUT_MEASUREMENT_UNRESOLVED);
    TEST_ASSERT(Layout_SaveMeasurement(l,NULL,m.id,Layout_GeometryHistory,NULL)&&l->objectStore.measurement_count==0);
    ld_test_shutdown_runtime();return true;
}
static bool frame_persistence_and_atomic_refusal(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;TEST_ASSERT(create("A",(Vec3){0})&&create("B",(Vec3){3,4,0})&&scope());
    LayoutSceneCollection c=*Layout_FindSceneCollection(&l->objectStore,"@scene.pilot");snprintf(c.inspection_frame,64,"A");TEST_ASSERT(Layout_EditSceneCollection(l,&c,NULL,NULL,NULL));
    char* before=Layout_SaveToString(l);TEST_ASSERT(before&&Layout_LoadFromString(l,before));TEST_ASSERT(!strcmp(Layout_FindSceneCollection(&l->objectStore,c.id)->inspection_frame,"A"));
    LayoutSavedMeasurement m=measurement(99);Editor_ClearHistory(&Global_Get()->editor);TEST_ASSERT(!Layout_SaveMeasurement(l,&m,NULL,Layout_GeometryHistory,NULL));
    char* after=Layout_SaveToString(l);TEST_ASSERT(!strcmp(before,after)&&Editor_UndoCount(&Global_Get()->editor)==0);free(before);free(after);
    m=measurement(4);snprintf(m.frame_entity,64,"A");TEST_ASSERT(fabs(Layout_EvaluateSavedMeasurement(l,&m).value-3)<1e-6);
    Object3D* a=(Object3D*)Layout_InspectionObject(l,"A");a->rectPrism.frame.axisU=(Vec3){0};TEST_ASSERT(Layout_EvaluateSavedMeasurement(l,&m).status==LAYOUT_MEASUREMENT_DEGENERATE);
    ld_test_shutdown_runtime();return true;
}
static bool ui_collection_focus_save_and_restore(void) {
    ld_test_init_runtime();GlobalState* g=Global_Get();Layout* l=&g->layout;TEST_ASSERT(create("A",(Vec3){0})&&create("B",(Vec3){3,4,0})&&scope());Editor_ClearHistory(&g->editor);
    char* before=Layout_SaveToString(l);TEST_ASSERT(UIPanel_BeginCollectionInspection("@scene.pilot"));SDL_Rect rect;TEST_ASSERT(UIPanel_MeasurementControlRect(INSPECT_FOCUS,&rect));
    TEST_ASSERT(UIPanel_MeasurementClick(rect.x+2,rect.y+2));TEST_ASSERT(g->cameraView.active&&!strcmp(l->objectStore.isolated_entity_id,"@scene.pilot"));
    char* after=Layout_SaveToString(l);TEST_ASSERT(!strcmp(before,after)&&Editor_UndoCount(&g->editor)==0);free(before);free(after);
    TEST_ASSERT(UIPanel_MeasurementControlRect(INSPECT_NAME,&rect));UIPanel_Get()->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx+=(float)(rect.y-UIPanel_Get()->rightBodyRect.y);UIPanel_CollectionInspectionLayout();
    TEST_ASSERT(UIPanel_MeasurementControlRect(INSPECT_NAME,&rect)&&UIPanel_MeasurementClick(rect.x+2,rect.y+2));
    SDL_Event event={.type=SDL_TEXTINPUT};snprintf(event.text.text,32,"Named cabinet width");Input_Handle(NULL,&event);event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;Input_Handle(NULL,&event);
    TEST_ASSERT(UIPanel_MeasurementControlRect(INSPECT_SAVE,&rect));UIPanel_Get()->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx+=(float)(rect.y-UIPanel_Get()->rightBodyRect.y);UIPanel_CollectionInspectionLayout();
    TEST_ASSERT(UIPanel_MeasurementControlRect(INSPECT_SAVE,&rect)&&UIPanel_MeasurementClick(rect.x+2,rect.y+2));TEST_ASSERT(l->objectStore.measurement_count==1&&Editor_UndoCount(&g->editor)==1&&!strcmp(l->objectStore.measurements[0].name,"Named cabinet width"));
    TEST_ASSERT(Editor_Undo(&g->editor,l)&&l->objectStore.measurement_count==0);TEST_ASSERT(Editor_Redo(&g->editor,l)&&l->objectStore.measurement_count==1);
    UIPanel_BeginMeasurement();TEST_ASSERT(!UIPanel_CollectionInspectionActive());ld_test_shutdown_runtime();return true;
}
static bool incomplete_bounds_and_malformed_records(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;TEST_ASSERT(create("A",(Vec3){0})&&create("B",(Vec3){3,4,0})&&scope());
    Object3D* a=(Object3D*)Layout_InspectionObject(l,"A");a->transform.scale.x=2;
    LayoutCollectionInspection bounds=Layout_InspectCollection(l,"@scene.pilot","");TEST_ASSERT(bounds.status==LAYOUT_MEASUREMENT_UNSUPPORTED&&bounds.unsupported==1&&isnan(bounds.max_meters[0]));
    a->transform.scale.x=1;LayoutSavedMeasurement m=measurement(0);TEST_ASSERT(Layout_SaveMeasurement(l,&m,NULL,NULL,NULL));
    char* before=Layout_SaveToString(l);TEST_ASSERT(before);cJSON* root=cJSON_Parse(before);TEST_ASSERT(root);
    cJSON* row=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"engineering"),"measurements"),0);
    TEST_ASSERT(cJSON_ReplaceItemInObjectCaseSensitive(row,"kind",cJSON_CreateNumber(1.5)));char* invalid=cJSON_PrintUnformatted(root);TEST_ASSERT(invalid);
    TEST_ASSERT(!Layout_LoadFromString(l,invalid));char* after=Layout_SaveToString(l);TEST_ASSERT(after&&!strcmp(before,after));free(after);free(invalid);cJSON_Delete(root);
    root=cJSON_Parse(before);cJSON* version=cJSON_GetObjectItemCaseSensitive(root,"file");TEST_ASSERT(cJSON_ReplaceItemInObjectCaseSensitive(version,"schemaVersion",cJSON_CreateNumber(28)));invalid=cJSON_PrintUnformatted(root);
    TEST_ASSERT(!Layout_LoadFromString(l,invalid));free(before);free(invalid);cJSON_Delete(root);ld_test_shutdown_runtime();return true;
}
static bool captured_frame_and_new_membership(void) {
    ld_test_init_runtime();Layout* l=&Global_Get()->layout;TEST_ASSERT(create("A",(Vec3){0})&&create("B",(Vec3){3,4,0})&&scope());
    LayoutSavedMeasurement m=measurement(0);snprintf(m.frame_entity,64,"A");TEST_ASSERT(Layout_SaveMeasurement(l,&m,NULL,NULL,NULL));
    LayoutSceneCollection c=*Layout_FindSceneCollection(&l->objectStore,"@scene.pilot");snprintf(c.inspection_frame,64,"B");TEST_ASSERT(Layout_EditSceneCollection(l,&c,NULL,NULL,NULL));
    TEST_ASSERT(!strcmp(l->objectStore.measurements[0].frame_entity,"A"));TEST_ASSERT(fabs(Layout_EvaluateSavedMeasurement(l,&m).value-4)<1e-6);
    TEST_ASSERT(create("C",(Vec3){10,0,0})&&Layout_AssignSceneCollection(l,"@scene.pilot","C",true,NULL,NULL));TEST_ASSERT(fabs(Layout_EvaluateSavedMeasurement(l,&m).value-11)<1e-6);
    LayoutSavedMeasurement bad=measurement(3);snprintf(bad.b.entity_id,64,"outside");TEST_ASSERT(Layout_EvaluateSavedMeasurement(l,&bad).status==LAYOUT_MEASUREMENT_UNRESOLVED);
    ld_test_shutdown_runtime();return true;
}
bool collection_inspection_run_tests(void) {
    const TestCase cases[]={{"analytic_frames_visibility_independence",analytic_frames_and_visibility_independence},{"mesh_envelopes_scale_rotation_datums",mesh_envelopes_scale_rotation_and_datums},{"measurements_undo_reload_dynamic_geometry",measurements_undo_reload_and_dynamic_geometry},{"frame_persistence_atomic_refusal",frame_persistence_and_atomic_refusal},{"ui_collection_focus_save_restore",ui_collection_focus_save_and_restore},{"incomplete_bounds_malformed_records",incomplete_bounds_and_malformed_records},{"captured_frame_new_membership",captured_frame_and_new_membership}};
    return run_test_cases("CollectionInspection",cases,sizeof(cases)/sizeof(cases[0]));
}
