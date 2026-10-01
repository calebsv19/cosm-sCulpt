#include "test_layout_internal.h"
#include "Layout/layout_motion.h"
#include "UI/ui_panel_spatial.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_parts.h"
#include "Input/input_handler.h"
#include "core_scene_compile.h"

static Layout* live(void) {return &Global_Get()->layout;}
static uint32_t part(const char* name, Vec3 center, float width, float height, float depth) {
    RectPrismPrimitiveCreateParams p={.width=width,.height=height,.depth=depth,.useExplicitFrame=true,
        .explicitFrame={.origin=center,.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t id=0;if (!Layout_CreateRectPrismPrimitive(live(),&p,&id,NULL)) return 0;
    Object3D* o=Layout_ObjectStore_Find(&live()->objectStore,id);
    if (core_object_set_identity(&o->coreMeta,name,"rect_prism_primitive").code!=CORE_OK) return 0;
    return id;
}
static bool rig(bool hinge) {
    TEST_ASSERT(part("A",(Vec3){0},.2f,.2f,.2f));
    TEST_ASSERT(part("B",(Vec3){1,0,0},hinge?2:.2f,hinge?.05f:.2f,.2f));
    LayoutConstraint c={.axis={0,0,1}};snprintf(c.id,64,"movement");snprintf(c.a.entity_id,64,"A");snprintf(c.b.entity_id,64,"B");
    if (hinge) {
        c.a.kind=c.b.kind=LAYOUT_REFERENCE_AXIS_U;c.b.local_offset_meters[0]=-1;
        TEST_ASSERT(Layout_InitAngularTravel(live(),&c,-30,110));
    } else {c.axis=(Vec3){1,0,0};TEST_ASSERT(Layout_InitLinearTravel(live(),&c,1,3));}
    TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));Editor_ClearHistory(&Global_Get()->editor);return true;
}
static bool same(const char* before) {char* after=Layout_SaveToString(live());bool ok=after && !strcmp(before,after);Layout_FreeString(after);return ok;}
static bool deny(const Layout* l,void* c) {(void)l;(void)c;return false;}
static bool inside(const Object3D* pose,const Object3D* envelope) {
    Vec3 min,max,c[8];TEST_ASSERT(Layout_Object3D_ComputeWorldAABB(envelope,&min,&max));
    TEST_ASSERT(pose->kind==OBJECT3D_KIND_PLANE?Layout_Object3D_ComputePlaneCorners(pose,c):Layout_Object3D_ComputeRectPrismCorners(pose,c));
    for (int i=0;i<(pose->kind==OBJECT3D_KIND_PLANE?4:8);++i) {
        TEST_ASSERT(c[i].x>=min.x-1e-6 && c[i].x<=max.x+1e-6 && c[i].y>=min.y-1e-6 && c[i].y<=max.y+1e-6 && c[i].z>=min.z-1e-6 && c[i].z<=max.z+1e-6);
    }
    return true;
}
static bool test_travel_samples_undo_and_static_clear_motion_hit(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(false));TEST_ASSERT(part("cabinet",(Vec3){2,0,0},.1f,.1f,.1f));
    char* before=Layout_SaveToString(live());Object3D pose;
    TEST_ASSERT(Layout_SampleMotion(live(),"movement",2,&pose) && fabs(pose.transform.position.x-2)<1e-6 && same(before));
    TEST_ASSERT(!Layout_SampleMotion(live(),"movement",4,&pose) && same(before));
    TEST_ASSERT(!Layout_GenerateMotionEnvelope(live(),"movement",33,deny,NULL) && same(before));Layout_FreeString(before);
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Editor_UndoCount(&Global_Get()->editor)==1 && live()->objectStore.motion_envelope_count==1);
    const LayoutMotionEnvelope e=live()->objectStore.motion_envelopes[0];const Object3D box=*Layout_ObjectStore_FindConst(&live()->objectStore,e.object_id);
    TEST_ASSERT(Layout_MotionEnvelopeCurrent(live(),&e) && !strcmp(box.info.entity_type,"MotionEnvelope"));
    TEST_ASSERT(fabs(box.rectPrism.width-2.2)<1e-4 && e.padding_meters>0 && live()->objectStore.constraints[0].target==1);
    double distance;bool hit,approx;TEST_ASSERT(Layout_SpatialDistance(live(),&live()->objectStore.items[1],&live()->objectStore.items[2],&distance,&hit,&approx) && !hit);
    LayoutSpatialResult results[4];TEST_ASSERT(Layout_CheckSpatial(live(),results,4)==1 && results[0].overlap && results[0].approximate && results[0].severity==LAYOUT_SPATIAL_WARNING && !strcmp(results[0].target,"cabinet"));
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,live()) && !live()->objectStore.motion_envelope_count && Layout_ObjectStore_LiveCount(&live()->objectStore)==3);
    TEST_ASSERT(Editor_Redo(&Global_Get()->editor,live()) && live()->objectStore.motion_envelopes[0].object_id==e.object_id);
    TEST_ASSERT(Layout_SetTravelPosition(live(),"movement",2.5,Layout_GeometryHistory,NULL) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    size_t undo=Editor_UndoCount(&Global_Get()->editor);TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL) && Editor_UndoCount(&Global_Get()->editor)==undo);
    ld_test_shutdown_runtime();return true;
}
static bool test_hinge_dense_intermediate_coverage_and_offsets(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(true));
    /* Two endpoints alone do not contain a door sweep; conservative padding must. */
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",2,NULL,NULL));
    Object3D box=*Layout_ObjectStore_FindConst(&live()->objectStore,live()->objectStore.motion_envelopes[0].object_id),pose;
    char* before=Layout_SaveToString(live());
    for (int i=0;i<=280;++i) {TEST_ASSERT(Layout_SampleMotion(live(),"movement",-30+i*.5,&pose));TEST_ASSERT(inside(&pose,&box));}
    TEST_ASSERT(same(before));Layout_FreeString(before);
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    box=*Layout_ObjectStore_FindConst(&live()->objectStore,live()->objectStore.motion_envelopes[0].object_id);
    TEST_ASSERT(live()->objectStore.motion_envelopes[0].padding_meters<.1);
    for (int i=0;i<=280;++i) {TEST_ASSERT(Layout_SampleMotion(live(),"movement",-30+i*.5,&pose));TEST_ASSERT(inside(&pose,&box));}
    TEST_ASSERT(Layout_SetTravelPosition(live(),"movement",90,NULL,NULL) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    ld_test_shutdown_runtime();return true;
}
static bool test_stale_regenerate_identity_and_deletion(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(false));TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL));
    uint32_t id=live()->objectStore.motion_envelopes[0].object_id;LayoutConstraint c=live()->objectStore.constraints[0];c.travel_max=4;
    TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,Layout_GeometryHistory,NULL));TEST_ASSERT(!Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    LayoutSpatialResult result;TEST_ASSERT(Layout_CheckSpatial(live(),&result,1)==1 && !result.measurable && strstr(result.message,"stale"));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL) && live()->objectStore.motion_envelopes[0].object_id==id);
    TEST_ASSERT(fabs(Layout_ObjectStore_FindConst(&live()->objectStore,id)->rectPrism.width-3.2)<1e-4);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,live()) && !Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Editor_Redo(&Global_Get()->editor,live()));char* before=Layout_SaveToString(live());
    TEST_ASSERT(!Layout_ConstraintEdit(live(),NULL,"movement",NULL,NULL) && same(before));Layout_FreeString(before);
    TEST_ASSERT(Layout_SetObject3DPosition(live(),id,(Vec3){9,0,0},NULL) && !Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(!Layout_ObjectStore_Delete(&live()->objectStore,id));
    LayoutVolumeEdit v={.object_id=id};TEST_ASSERT(Layout_EditVolume(live(),&v,true,Layout_GeometryHistory,NULL) && !live()->objectStore.motion_envelope_count);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,live()) && live()->objectStore.motion_envelope_count==1);
    ld_test_shutdown_runtime();return true;
}
static bool test_schema18_roundtrip_negative_migration_and_export(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(true));TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    char* before=Layout_SaveToString(live());TEST_ASSERT(Layout_LoadFromString(live(),before) && same(before) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    for (int bad=0;bad<8;++bad) {
        cJSON* root=cJSON_Parse(before);cJSON* eng=cJSON_GetObjectItemCaseSensitive(root,"engineering");cJSON* arr=cJSON_GetObjectItemCaseSensitive(eng,"motionEnvelopes");cJSON* item=cJSON_GetArrayItem(arr,0);
        if (bad==0)cJSON_DeleteItemFromObjectCaseSensitive(eng,"motionEnvelopes");
        if (bad==1)cJSON_ReplaceItemInObjectCaseSensitive(item,"ruleId",cJSON_CreateString("missing"));
        if (bad==2)cJSON_ReplaceItemInObjectCaseSensitive(item,"samples",cJSON_CreateNumber(1));
        if (bad==3)cJSON_ReplaceItemInObjectCaseSensitive(item,"paddingMeters",cJSON_CreateNumber(-1));
        if (bad==4)cJSON_ReplaceItemInObjectCaseSensitive(item,"boundsDigest",cJSON_CreateString("bad"));
        if (bad==5)cJSON_AddItemToArray(arr,cJSON_Duplicate(item,true));
        if (bad==6)cJSON_ReplaceItemInObjectCaseSensitive(item,"objectId",cJSON_CreateNumber(999));
        if (bad==7)cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(17));
        char* text=cJSON_PrintUnformatted(root);TEST_ASSERT(!Layout_LoadFromString(live(),text) && same(before));free(text);cJSON_Delete(root);
    }
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(live(),"motion_test");cJSON* scene=authored?cJSON_Parse(authored):NULL;TEST_ASSERT(scene);
    char* compiled=NULL;char diagnostic[256];
    TEST_ASSERT(core_scene_compile_authoring_to_runtime(authored,&compiled,diagnostic,sizeof(diagnostic)).code==CORE_OK);
    cJSON* runtime=cJSON_Parse(compiled);TEST_ASSERT(runtime && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(runtime,"objects"))==5);
    cJSON* preserved=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(runtime,"extensions"),"line_drawing"),"layout_snapshot");
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(preserved,"engineering"),"motionEnvelopes"))==1);
    cJSON_Delete(runtime);free(compiled);free(authored);
    cJSON* snapshot=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(scene,"extensions"),"line_drawing"),"layout_snapshot");
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(snapshot,"engineering"),"motionEnvelopes"))==1);
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(scene,"objects"))==5);cJSON_Delete(scene);Layout_FreeString(before);
    ld_test_shutdown_runtime();ld_test_init_runtime();TEST_ASSERT(rig(false));
    char* text=Layout_SaveToString(live());cJSON* old=cJSON_Parse(text);Layout_FreeString(text);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(old,"file"),"schemaVersion",cJSON_CreateNumber(17));cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(old,"engineering"),"motionEnvelopes");
    text=cJSON_PrintUnformatted(old);TEST_ASSERT(Layout_LoadFromString(live(),text) && !live()->objectStore.motion_envelope_count);free(text);cJSON_Delete(old);
    ld_test_shutdown_runtime();return true;
}
static bool test_scaled_panel_sampling_and_locked_failure(void) {
    ld_test_init_runtime();live()->metersPerWorldUnit=.01;live()->scene3d.bounds.enabled=false;
    TEST_ASSERT(part("A",(Vec3){0},10,10,10));
    PlanePrimitiveCreateParams panel={.width=50,.height=100,.useExplicitFrame=true,.explicitFrame={.origin={100,0,0},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t id;TEST_ASSERT(Layout_CreatePlanePrimitive(live(),&panel,&id,NULL));
    TEST_ASSERT(core_object_set_identity(&Layout_ObjectStore_Find(&live()->objectStore,id)->coreMeta,"B","plane_primitive").code==CORE_OK);
    LayoutConstraint c={.axis={1,0,0}};snprintf(c.id,64,"movement");snprintf(c.a.entity_id,64,"A");snprintf(c.b.entity_id,64,"B");
    TEST_ASSERT(Layout_InitLinearTravel(live(),&c,1,2) && Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    Layout_ObjectStore_Find(&live()->objectStore,id)->coreMeta.flags.locked=true;char* before=Layout_SaveToString(live());
    TEST_ASSERT(!Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL) && same(before));
    TEST_ASSERT(!Layout_GenerateMotionEnvelope(live(),"movement",1,NULL,NULL) && same(before));Layout_FreeString(before);
    Layout_ObjectStore_Find(&live()->objectStore,id)->coreMeta.flags.locked=false;
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    const Object3D* envelope=Layout_ObjectStore_FindConst(&live()->objectStore,live()->objectStore.motion_envelopes[0].object_id);
    TEST_ASSERT(fabs(envelope->rectPrism.width*.01-1.5)<1e-4 && envelope->rectPrism.depth>0);
    for (int i=0;i<=32;++i) {Object3D pose;TEST_ASSERT(Layout_SampleMotion(live(),"movement",1+i/32.0,&pose) && inside(&pose,envelope));}
    TEST_ASSERT(Layout_SetObject3DPosition(live(),1,(Vec3){10,0,0},NULL));
    TEST_ASSERT(!Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    ld_test_shutdown_runtime();return true;
}
static bool parts_click(int action) {
    SDL_Rect rect;TEST_ASSERT(UIPanel_PartsControlRect(action,&rect));
    AppContext c={0};SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=rect.x+rect.w/2;e.button.y=rect.y+rect.h/2;Input_Handle(&c,&e);return true;
}
static bool test_mouse_create_regenerate_checks_return_to_movement(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(false));TEST_ASSERT(part("cabinet",(Vec3){2,0,0},.1f,.1f,.1f));
    Global_SetWindowSize(1800,1900);UIPanel_OnWindowResized(1800,1900);TEST_ASSERT(UIPanel_BeginMeasurement());UIPanel_MeasurementSelectRule(0);UIPanel_LayoutMeasurementPane();
    SDL_Rect rect;TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_ENVELOPE,&rect));TEST_ASSERT(UIPanel_MeasurementClick(rect.x+rect.w/2,rect.y+rect.h/2));
    TEST_ASSERT(UIPanel_Get()->activeRightTab==UI_PANEL_RIGHT_TAB_PARTS && UIPanel_Get()->parts.mode==4 && live()->objectStore.motion_envelope_count==1);UIPanel_LayoutParts();
    TEST_ASSERT(!UIPanel_PartsControlRect(PARTS_VOLUME_SAVE,NULL));TEST_ASSERT(parts_click(PARTS_ENVELOPE_CHECK));
    TEST_ASSERT(UIPanel_Get()->parts.mode==5 && UIPanel_Get()->spatial.result_count==1 && UIPanel_Get()->spatial.results[0].approximate);
    UIPanel_Get()->parts.mode=4;UIPanel_LayoutParts();TEST_ASSERT(parts_click(PARTS_ENVELOPE_MOVEMENT) && UIPanel_Get()->activeRightTab==UI_PANEL_RIGHT_TAB_MEASURE && UIPanel_Get()->measurement.constraint_index==0 && Global_Get()->editor.selectedObject3DId==2);
    LayoutConstraint c=live()->objectStore.constraints[0];c.travel_max=4;TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_PARTS);UIPanel_Get()->parts.mode=4;UIPanel_LayoutParts();TEST_ASSERT(parts_click(PARTS_ENVELOPE_REGENERATE) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(parts_click(PARTS_VOLUME_REMOVE) && parts_click(PARTS_SPATIAL_CANCEL) && live()->objectStore.motion_envelope_count==1);
    TEST_ASSERT(parts_click(PARTS_VOLUME_NEW) && UIPanel_PartsControlRect(PARTS_VOLUME_SAVE,NULL));
    Global_Get()->editor.selectedObject3DId=live()->objectStore.motion_envelopes[0].object_id;UIPanel_SpatialEnterVolumes();UIPanel_LayoutParts();
    TEST_ASSERT(parts_click(PARTS_VOLUME_REMOVE) && parts_click(PARTS_VOLUME_CONFIRM_REMOVE) && !live()->objectStore.motion_envelope_count);
    ld_test_shutdown_runtime();return true;
}
bool motion_run_tests(void) {
    const TestCase tests[]={
        {"travel_atomic_sample_history_obstruction",test_travel_samples_undo_and_static_clear_motion_hit},
        {"hinge_dense_intermediate_coverage_offsets",test_hinge_dense_intermediate_coverage_and_offsets},
        {"stale_regenerate_identity_delete",test_stale_regenerate_identity_and_deletion},
        {"schema18_roundtrip_migration_export",test_schema18_roundtrip_negative_migration_and_export},
        {"scaled_panel_locked_failure",test_scaled_panel_sampling_and_locked_failure},
        {"mouse_envelope_workflow",test_mouse_create_regenerate_checks_return_to_movement}
    };return run_test_cases("Motion",tests,sizeof(tests)/sizeof(tests[0]));
}
