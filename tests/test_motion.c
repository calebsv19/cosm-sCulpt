#include "Input/input_editor_actions.h"
#include "Input/input_handler.h"
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

static bool assembly(const char* label, const char* parent, char id[64]) {
    LayoutAssembly a={.frame={.origin={1,0,0},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(a.info.label,96,"%s",label);snprintf(a.info.entity_type,64,"Assembly");snprintf(a.info.parent_id,64,"%s",parent);
    TEST_ASSERT(Layout_EditAssembly(live(),&a,NULL,NULL,NULL));snprintf(id,64,"%s",live()->objectStore.assemblies[live()->objectStore.assembly_count-1].id);return true;
}
static bool parent_part(const char* id, const char* group) {
    const LayoutEntityInfo* old=Layout_EntityInfo(&live()->objectStore,id);TEST_ASSERT(old);
    LayoutEntityInfo info=*old;snprintf(info.parent_id,64,"%s",group);TEST_ASSERT(Layout_SetEntityInfo(live(),id,&info,NULL,NULL));return true;
}
static bool group_rig(bool hinge) {
    TEST_ASSERT(rig(hinge));char group[64],nested[64];TEST_ASSERT(assembly("Moving group","",group) && assembly("Nested",group,nested));
    TEST_ASSERT(part("peer",hinge?(Vec3){1,1,0}:(Vec3){1,.5f,0},.15f,.15f,.15f));
    TEST_ASSERT(parent_part("B",group) && parent_part("peer",nested));
    LayoutConstraint c=live()->objectStore.constraints[0];snprintf(c.motion_assembly,64,"%s",group);
    TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));Editor_ClearHistory(&Global_Get()->editor);return true;
}
static bool test_assembly_travel_envelope_undo_freshness(void) {
    ld_test_init_runtime();TEST_ASSERT(group_rig(false));TEST_ASSERT(part("cabinet",(Vec3){2,.5f,0},.1f,.1f,.1f));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL));
    const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];uint32_t id=e->object_id;
    TEST_ASSERT(e->member_count==2 && !strcmp(e->assembly_id,"assembly_1"));
    LayoutSpatialResult results[8];TEST_ASSERT(Layout_CheckSpatial(live(),results,8)==1 && !strcmp(results[0].target,"cabinet"));
    Object3D poses[4];size_t count;char* before=Layout_SaveToString(live());
    TEST_ASSERT(Layout_SampleMotionSet(live(),"movement",2.5,poses,4,&count) && count==2 && same(before));Layout_FreeString(before);
    TEST_ASSERT(fabs(poses[1].transform.position.x-2.5)<1e-5 && fabs(poses[1].transform.position.y-.5)<1e-5);
    TEST_ASSERT(Layout_SetTravelPosition(live(),"movement",2.5,Layout_GeometryHistory,NULL));
    TEST_ASSERT(fabs(live()->objectStore.items[2].transform.position.x-2.5)<1e-5 && fabs(live()->objectStore.assemblies[1].frame.origin.x-2.5)<1e-5);
    TEST_ASSERT(Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,live()) && fabs(live()->objectStore.items[2].transform.position.x-1)<1e-5);
    TEST_ASSERT(Editor_Redo(&Global_Get()->editor,live()) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Layout_SetObject3DPosition(live(),2,(Vec3){2,0,0},NULL) && fabs(live()->objectStore.items[2].transform.position.x-2)<1e-5);
    TEST_ASSERT(Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Layout_SetObject3DPosition(live(),3,(Vec3){2,.7f,0},NULL) && !Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL) && live()->objectStore.motion_envelopes[0].object_id==id);
    TEST_ASSERT(parent_part("peer","") && !Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL) && live()->objectStore.motion_envelopes[0].member_count==1);
    ld_test_shutdown_runtime();return true;
}
static bool test_scaled_assembly_motion(void) {
    ld_test_init_runtime();live()->metersPerWorldUnit=.01;live()->scene3d.bounds.enabled=false;
    TEST_ASSERT(part("A",(Vec3){0},20,20,20) && part("B",(Vec3){100,0,0},40,40,20) && part("peer",(Vec3){100,100,0},20,20,20));
    char group[64];TEST_ASSERT(assembly("Scaled","",group) && parent_part("B",group) && parent_part("peer",group));
    LayoutConstraint c={.axis={1,0,0}};snprintf(c.id,64,"movement");snprintf(c.a.entity_id,64,"A");snprintf(c.b.entity_id,64,"B");snprintf(c.motion_assembly,64,"%s",group);
    TEST_ASSERT(Layout_InitLinearTravel(live(),&c,1,3) && Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    TEST_ASSERT(Layout_SetTravelPosition(live(),"movement",2.5,NULL,NULL) && fabs(live()->objectStore.items[2].transform.position.x-250)<1e-4);
    const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];TEST_ASSERT(Layout_MotionEnvelopeCurrent(live(),e));
    const Object3D* box=Layout_ObjectStore_FindConst(&live()->objectStore,e->object_id);TEST_ASSERT(fabs(box->rectPrism.width*.01-2.4)<1e-4);
    Object3D poses[4];size_t count;TEST_ASSERT(Layout_SampleMotionSet(live(),"movement",3,poses,4,&count));for(size_t i=0;i<count;++i)TEST_ASSERT(inside(&poses[i],box));
    ld_test_shutdown_runtime();return true;
}
static bool test_envelope_scope_upgrade_and_downgrade(void) {
    ld_test_init_runtime();TEST_ASSERT(group_rig(false));TEST_ASSERT(part("cabinet",(Vec3){2,.5f,0},.1f,.1f,.1f));
    LayoutConstraint c=live()->objectStore.constraints[0];c.motion_assembly[0]=0;
    TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL) && Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    uint32_t id=live()->objectStore.motion_envelopes[0].object_id;TEST_ASSERT(Layout_CheckSpatial(live(),NULL,0)==0);
    snprintf(c.motion_assembly,64,"assembly_1");TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(!Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    char* before=Layout_SaveToString(live());TEST_ASSERT(!Layout_GenerateMotionEnvelope(live(),"movement",33,deny,NULL) && same(before));Layout_FreeString(before);
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL));
    TEST_ASSERT(live()->objectStore.motion_envelopes[0].object_id==id && live()->objectStore.motion_envelopes[0].member_count==2 && Layout_CheckSpatial(live(),NULL,0)==1);
    c.motion_assembly[0]=0;TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,Layout_GeometryHistory,NULL));
    TEST_ASSERT(live()->objectStore.motion_envelopes[0].object_id==id && !live()->objectStore.motion_envelopes[0].member_count && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    ld_test_shutdown_runtime();return true;
}
static bool test_assembly_hinge_coverage_and_slider_rigidity(void) {
    ld_test_init_runtime();TEST_ASSERT(group_rig(true));TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",2,NULL,NULL));
    Object3D box=*Layout_ObjectStore_FindConst(&live()->objectStore,live()->objectStore.motion_envelopes[0].object_id),poses[4];size_t count;
    char* before=Layout_SaveToString(live());
    for (int i=0;i<=140;++i) {TEST_ASSERT(Layout_SampleMotionSet(live(),"movement",-30+i,poses,4,&count));for(size_t j=0;j<count;++j)TEST_ASSERT(inside(&poses[j],&box));}
    TEST_ASSERT(same(before));Layout_FreeString(before);
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    box=*Layout_ObjectStore_FindConst(&live()->objectStore,live()->objectStore.motion_envelopes[0].object_id);
    for (int i=0;i<=140;++i) {TEST_ASSERT(Layout_SampleMotionSet(live(),"movement",-30+i,poses,4,&count));for(size_t j=0;j<count;++j)TEST_ASSERT(inside(&poses[j],&box));}
    TEST_ASSERT(Layout_SetTravelPosition(live(),"movement",90,Layout_GeometryHistory,NULL));
    const Object3D* peer=&live()->objectStore.items[2];TEST_ASSERT(fabs(peer->transform.position.x+1)<1e-5 && fabs(peer->transform.position.y-1)<1e-5);
    TEST_ASSERT(fabs(live()->objectStore.assemblies[1].frame.axisU.x)<1e-5 && fabs(live()->objectStore.assemblies[1].frame.axisU.y-1)<1e-5);
    TEST_ASSERT(Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    for (int i=0;i<20;++i) TEST_ASSERT(Layout_SetTravelPosition(live(),"movement",i%2?0:90,NULL,NULL));
    TEST_ASSERT(Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));
    char* json=Layout_SaveToString(live());TEST_ASSERT(Layout_LoadFromString(live(),json) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));Layout_FreeString(json);
    ld_test_shutdown_runtime();return true;
}
static bool test_assembly_scope_rejections_atomic(void) {
    ld_test_init_runtime();TEST_ASSERT(group_rig(false));
    live()->objectStore.items[2].coreMeta.flags.locked=true;char* before=Layout_SaveToString(live());
    TEST_ASSERT(!Layout_SetTravelPosition(live(),"movement",2,Layout_GeometryHistory,NULL) && same(before));
    TEST_ASSERT(!Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL) && same(before));Layout_FreeString(before);
    live()->objectStore.items[2].coreMeta.flags.locked=false;
    before=Layout_SaveToString(live());TEST_ASSERT(!Layout_SetTravelPosition(live(),"movement",2,deny,NULL) && same(before));
    LayoutConstraint second={.kind=LAYOUT_CONSTRAINT_DISTANCE,.axis={1,0,0},.target=1};
    snprintf(second.id,64,"conflict");snprintf(second.a.entity_id,64,"A");snprintf(second.b.entity_id,64,"peer");
    TEST_ASSERT(!Layout_ConstraintEdit(live(),&second,NULL,NULL,NULL) && same(before));
    LayoutEntityInfo info=*Layout_EntityInfo(&live()->objectStore,"A");snprintf(info.parent_id,64,"assembly_1");
    TEST_ASSERT(!Layout_SetEntityInfo(live(),"A",&info,NULL,NULL) && same(before));
    info=*Layout_EntityInfo(&live()->objectStore,"peer");info.reference=true;
    TEST_ASSERT(!Layout_SetEntityInfo(live(),"peer",&info,NULL,NULL) && same(before));Layout_FreeString(before);
    live()->scene3d.bounds.enabled=true;live()->scene3d.bounds.clampOnEdit=true;live()->scene3d.bounds.min=(Vec3){-5,-5,-5};live()->scene3d.bounds.max=(Vec3){1.5f,5,5};
    live()->objectStore.items[2].rectPrism.lockToBounds=true;before=Layout_SaveToString(live());
    TEST_ASSERT(!Layout_SetTravelPosition(live(),"movement",2,NULL,NULL) && same(before));Layout_FreeString(before);
    ld_test_shutdown_runtime();return true;
}
static bool test_schema19_scope_negative_and_schema18_compatibility(void) {
    ld_test_init_runtime();TEST_ASSERT(group_rig(true) && Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    char* before=Layout_SaveToString(live());TEST_ASSERT(Layout_LoadFromString(live(),before) && same(before));
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(live(),"assembly_motion");char* compiled=NULL;char diagnostic[256];
    TEST_ASSERT(authored && core_scene_compile_authoring_to_runtime(authored,&compiled,diagnostic,sizeof(diagnostic)).code==CORE_OK);
    cJSON* runtime=cJSON_Parse(compiled);TEST_ASSERT(runtime);
    cJSON* snapshot=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(runtime,"extensions"),"line_drawing"),"layout_snapshot");
    cJSON* exported=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(snapshot,"engineering"),"motionEnvelopes"),0);
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(exported,"members"))==2);
    TEST_ASSERT(!strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(snapshot,"geometricConstraints"),0),"motionAssembly")->valuestring,"assembly_1"));
    cJSON_Delete(runtime);free(compiled);free(authored);
    for (int bad=0;bad<7;++bad) {
        cJSON* root=cJSON_Parse(before);cJSON* item=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"engineering"),"motionEnvelopes"),0);
        cJSON* members=cJSON_GetObjectItemCaseSensitive(item,"members");cJSON* member=cJSON_GetArrayItem(members,0);
        cJSON* rule=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"geometricConstraints"),0);
        if(bad==0)cJSON_DeleteItemFromObjectCaseSensitive(rule,"motionAssembly");
        if(bad==1)cJSON_ReplaceItemInObjectCaseSensitive(rule,"motionAssembly",cJSON_CreateString("missing"));
        if(bad==2)cJSON_DeleteItemFromObjectCaseSensitive(item,"members");
        if(bad==3)cJSON_AddItemToArray(members,cJSON_Duplicate(member,true));
        if(bad==4)cJSON_ReplaceItemInArray(cJSON_GetObjectItemCaseSensitive(member,"localBasis"),0,cJSON_CreateNumber(-1));
        if(bad==5)cJSON_ReplaceItemInArray(cJSON_GetObjectItemCaseSensitive(member,"sizeMeters"),0,cJSON_CreateNumber(-1));
        if(bad==6)cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(18));
        char* text=cJSON_PrintUnformatted(root);TEST_ASSERT(!Layout_LoadFromString(live(),text) && same(before));free(text);cJSON_Delete(root);
    }
    Layout_FreeString(before);ld_test_shutdown_runtime();ld_test_init_runtime();TEST_ASSERT(rig(false) && Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    before=Layout_SaveToString(live());cJSON* root=cJSON_Parse(before);Layout_FreeString(before);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(18));
    cJSON* item=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"engineering"),"motionEnvelopes"),0);
    cJSON_DeleteItemFromObjectCaseSensitive(item,"members");cJSON_DeleteItemFromObjectCaseSensitive(item,"assemblyId");
    cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"geometricConstraints"),0),"motionAssembly");
    char* old=cJSON_PrintUnformatted(root);TEST_ASSERT(Layout_LoadFromString(live(),old) && Layout_MotionEnvelopeCurrent(live(),&live()->objectStore.motion_envelopes[0]));free(old);cJSON_Delete(root);
    ld_test_shutdown_runtime();return true;
}
static bool test_inspection_hit_false_positive_and_read_only(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(true));TEST_ASSERT(part("hit",(Vec3){1,1,0},.15f,.15f,.3f));TEST_ASSERT(part("miss",(Vec3){1.7f,1.7f,0},.1f,.1f,.3f));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];
    char* before=Layout_SaveToString(live());LayoutMotionInspection result;
    TEST_ASSERT(Layout_InspectMotionObstruction(live(),e,"hit",0,false,&result) && result.hit && result.tested_samples==33 && !strcmp(result.member_id,"B"));
    TEST_ASSERT(Layout_InspectMotionObstruction(live(),e,"miss",0,false,&result) && !result.hit && result.gap_meters>0 && result.tested_samples==33);
    TEST_ASSERT(Layout_InspectMotionObstruction(live(),e,"miss",.5,true,&result) && result.hit);
    TEST_ASSERT(!Layout_InspectMotionObstruction(live(),e,"B",0,false,&result) && same(before));Layout_FreeString(before);
    LayoutConstraint c=live()->objectStore.constraints[0];c.travel_max=100;TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(!Layout_InspectMotionObstruction(live(),e,"hit",0,false,&result));
    ld_test_shutdown_runtime();return true;
}
static bool measure_click(int action) {
    SDL_Rect r;TEST_ASSERT(UIPanel_MeasurementControlRect(action,&r));TEST_ASSERT(UIPanel_MeasurementClick(r.x+r.w/2,r.y+r.h/2));return true;
}
static bool test_mouse_scope_and_readonly_preview(void) {
    ld_test_init_runtime();TEST_ASSERT(group_rig(false));LayoutConstraint c=live()->objectStore.constraints[0];c.motion_assembly[0]=0;TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(part("cabinet",(Vec3){2,.5f,0},.1f,.1f,.1f));Global_SetWindowSize(1800,1900);UIPanel_OnWindowResized(1800,1900);
    TEST_ASSERT(UIPanel_BeginMeasurement());UIPanel_MeasurementSelectRule(0);UIPanel_LayoutMeasurementPane();
    TEST_ASSERT(measure_click(MEASURE_MOTION_SCOPE) && UIPanel_Get()->measurement.chooser==9);
    TEST_ASSERT(measure_click(MEASURE_CHOICE_BASE+1) && !strcmp(live()->objectStore.constraints[0].motion_assembly,"assembly_1"));
    TEST_ASSERT(measure_click(MEASURE_TRAVEL_TO_MAX) && fabs(live()->objectStore.items[2].transform.position.x-3)<1e-5);
    TEST_ASSERT(measure_click(MEASURE_ENVELOPE));UIPanel_LayoutParts();TEST_ASSERT(parts_click(PARTS_ENVELOPE_CHECK));UIPanel_LayoutParts();TEST_ASSERT(parts_click(9000));UIPanel_LayoutParts();
    char* before=Layout_SaveToString(live());size_t undo=Editor_UndoCount(&Global_Get()->editor);
    TEST_ASSERT(parts_click(PARTS_MOTION_INSPECT) && UIPanel_Get()->spatial.preview_active && UIPanel_Get()->spatial.inspection.hit);
    UIPanel_LayoutParts();TEST_ASSERT(parts_click(PARTS_MOTION_NEXT) && same(before) && Editor_UndoCount(&Global_Get()->editor)==undo);
    TEST_ASSERT(parts_click(PARTS_MOTION_CLOSE) && !UIPanel_Get()->spatial.preview_active && same(before));
    TEST_ASSERT(parts_click(PARTS_MOTION_INSPECT) && UIPanel_Get()->spatial.preview_active);
    TEST_ASSERT(Layout_SetObject3DPosition(live(),4,(Vec3){2,.7f,0},NULL));UIPanel_LayoutParts();TEST_ASSERT(!UIPanel_Get()->spatial.preview_active);Layout_FreeString(before);
    ld_test_shutdown_runtime();return true;
}

static bool covered(const Object3D* pose, const LayoutMotionCoverage* coverage) {
    Vec3 corners[8];double scale=Layout_WorldScale(live());
    TEST_ASSERT(pose->kind==OBJECT3D_KIND_PLANE?Layout_Object3D_ComputePlaneCorners(pose,corners):Layout_Object3D_ComputeRectPrismCorners(pose,corners));
    for (int j=0;j<(pose->kind==OBJECT3D_KIND_PLANE?4:8);++j) {
        double point[]={corners[j].x*scale,corners[j].y*scale,corners[j].z*scale};bool found=false;
        for (size_t i=0;i<coverage->count && !found;++i) {
            found=true;
            for (int k=0;k<3;++k) if (point[k]<coverage->pieces[i].min_meters[k] || point[k]>coverage->pieces[i].max_meters[k]) found=false;
        }
        TEST_ASSERT(found);
    }
    return true;
}
static bool test_interval_union_removes_empty_assembly_gap_and_reports_pass(void) {
    ld_test_init_runtime();TEST_ASSERT(group_rig(false));TEST_ASSERT(part("gap",(Vec3){2,.25f,0},.05f,.05f,.1f));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];
    const Object3D* box=Layout_ObjectStore_FindConst(&live()->objectStore,e->object_id);const Object3D* target=&live()->objectStore.items[3];
    double gap;bool hit,approx;TEST_ASSERT(Layout_SpatialDistance(live(),box,target,&gap,&hit,&approx) && hit);
    char* before=Layout_SaveToString(live());LayoutMotionCoverage coverage={0};
    TEST_ASSERT(Layout_BuildMotionCoverage(live(),e,&coverage) && coverage.count==64);
    TEST_ASSERT(Layout_MotionCoverageDistance(live(),&coverage,target,&gap,&hit) && !hit && gap>.12 && gap<.13);
    Layout_FreeMotionCoverage(&coverage);TEST_ASSERT(!coverage.pieces && !coverage.count);
    TEST_ASSERT(Layout_CheckSpatial(live(),NULL,0)==0 && same(before));Layout_FreeString(before);
    LayoutSpatialRule rule={.kind=LAYOUT_SPATIAL_NO_INTERSECTION};snprintf(rule.source,64,"%s",box->coreMeta.object_id);snprintf(rule.target,64,"gap");
    TEST_ASSERT(Layout_EditSpatialRule(live(),&rule,NULL,NULL,NULL));before=Layout_SaveToString(live());
    LayoutSpatialResult result;TEST_ASSERT(Layout_CheckSpatial(live(),&result,1)==1 && result.severity==LAYOUT_SPATIAL_PASS && result.motion_intervals && result.approximate && !result.overlap);
    cJSON* report=Layout_SpatialReportJson(live());TEST_ASSERT(report);
    const cJSON* item=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(report,"results"),0);
    TEST_ASSERT(!strcmp(cJSON_GetObjectItemCaseSensitive(item,"geometryMethod")->valuestring,"motion_member_intervals"));cJSON_Delete(report);
    LayoutMotionRangeResult range;TEST_ASSERT(Layout_CheckMotionRange(live(),e,"gap",0,false,257,&range) && range.status==LAYOUT_MOTION_RANGE_CLEAR && range.gap_meters>0 && same(before));
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_piecewise_hinge_reduction_and_dense_group_coverage(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(true));TEST_ASSERT(part("miss",(Vec3){1.7f,1.7f,0},.1f,.1f,.3f));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];
    double gap;bool hit,approx;TEST_ASSERT(Layout_SpatialDistance(live(),Layout_ObjectStore_FindConst(&live()->objectStore,e->object_id),&live()->objectStore.items[2],&gap,&hit,&approx) && hit);
    TEST_ASSERT(Layout_CheckSpatial(live(),NULL,0)==0);LayoutMotionRangeResult range;
    TEST_ASSERT(Layout_CheckMotionRange(live(),e,"miss",0,false,257,&range) && range.status==LAYOUT_MOTION_RANGE_CLEAR && range.gap_meters>0);
    ld_test_shutdown_runtime();ld_test_init_runtime();TEST_ASSERT(group_rig(true));
    for (int samples=2;samples<=33;samples+=31) {
        TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",(uint32_t)samples,NULL,NULL));LayoutMotionCoverage coverage={0};
        TEST_ASSERT(Layout_BuildMotionCoverage(live(),&live()->objectStore.motion_envelopes[0],&coverage));
        char* before=Layout_SaveToString(live());Object3D poses[4];size_t count;
        for (int i=0;i<=140;++i) {
            TEST_ASSERT(Layout_SampleMotionSet(live(),"movement",-30+i,poses,4,&count));
            for (size_t j=0;j<count;++j) TEST_ASSERT(covered(&poses[j],&coverage));
        }
        TEST_ASSERT(same(before));Layout_FreeString(before);Layout_FreeMotionCoverage(&coverage);
    }
    LayoutConstraint c=live()->objectStore.constraints[0];c.axis=(Vec3){0,4,0};
    TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL) && Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    LayoutMotionCoverage coverage={0};TEST_ASSERT(Layout_BuildMotionCoverage(live(),&live()->objectStore.motion_envelopes[0],&coverage));
    Object3D poses[4];size_t count;
    for (int i=0;i<=140;++i) {TEST_ASSERT(Layout_SampleMotionSet(live(),"movement",-30+i,poses,4,&count));for(size_t j=0;j<count;++j)TEST_ASSERT(covered(&poses[j],&coverage));}
    Layout_FreeMotionCoverage(&coverage);ld_test_shutdown_runtime();return true;
}
static bool thin_travel(void) {
    TEST_ASSERT(rig(false));TEST_ASSERT(Layout_SetRectPrismDimensions(live(),2,.005f,.02f,.02f,NULL));
    TEST_ASSERT(part("thin_obstacle",(Vec3){1.03125f,0,0},.005f,.02f,.02f));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));return true;
}
static bool test_between_samples_contact_budget_and_clearance(void) {
    ld_test_init_runtime();TEST_ASSERT(thin_travel());const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];
    char* before=Layout_SaveToString(live());size_t undo=Editor_UndoCount(&Global_Get()->editor);LayoutMotionInspection sampled;LayoutMotionRangeResult range;
    TEST_ASSERT(Layout_InspectMotionObstruction(live(),e,"thin_obstacle",0,false,&sampled) && !sampled.hit);
    TEST_ASSERT(Layout_CheckMotionRange(live(),e,"thin_obstacle",0,false,1,&range) && range.status==LAYOUT_MOTION_RANGE_UNRESOLVED && range.tested_poses==1 && range.gap_meters==0);
    TEST_ASSERT(Layout_CheckMotionRange(live(),e,"thin_obstacle",0,false,257,&range) && range.status==LAYOUT_MOTION_RANGE_FAILURE && fabs(range.position-1.03125)<.003 && !strcmp(range.member_id,"B"));
    TEST_ASSERT(range.tested_poses<=257 && same(before) && Editor_UndoCount(&Global_Get()->editor)==undo);
    TEST_ASSERT(Layout_SetObject3DPosition(live(),3,(Vec3){1.03125f,.1f,0},NULL));Layout_FreeString(before);before=Layout_SaveToString(live());
    TEST_ASSERT(Layout_CheckMotionRange(live(),e,"thin_obstacle",.05,true,257,&range) && range.status==LAYOUT_MOTION_RANGE_CLEAR);
    TEST_ASSERT(Layout_CheckMotionRange(live(),e,"thin_obstacle",.1,true,257,&range) && range.status==LAYOUT_MOTION_RANGE_FAILURE && range.gap_meters<.1 && same(before));
    Layout_FreeString(before);
    TEST_ASSERT(Layout_SetObject3DPosition(live(),3,(Vec3){1.03125f,.020005f,0},NULL));
    TEST_ASSERT(Layout_InspectMotionObstruction(live(),e,"thin_obstacle",0,false,&sampled) && !sampled.hit);
    TEST_ASSERT(Layout_CheckMotionRange(live(),e,"thin_obstacle",0,false,257,&range) && range.status==LAYOUT_MOTION_RANGE_UNRESOLVED && range.tested_poses<=257);
    ld_test_shutdown_runtime();return true;
}
static bool test_hinge_refinement_finds_contact_after_bounds_ambiguity(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(true));LayoutConstraint c=live()->objectStore.constraints[0];c.travel_min=0;
    TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));TEST_ASSERT(part("cabinet",(Vec3){1,1,0},.25f,.25f,.5f));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    char* before=Layout_SaveToString(live());LayoutMotionRangeResult result;
    TEST_ASSERT(Layout_CheckMotionRange(live(),&live()->objectStore.motion_envelopes[0],"cabinet",0,false,257,&result));
    TEST_ASSERT(result.status==LAYOUT_MOTION_RANGE_FAILURE && result.position>30 && result.position<60 && result.tested_poses<=257 && same(before));
    Layout_FreeString(before);
    TEST_ASSERT(Layout_SetTravelPosition(live(),"movement",25,NULL,NULL));c=live()->objectStore.constraints[0];
    TEST_ASSERT(Layout_InitAngularTravel(live(),&c,25,30) && Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));LayoutSpatialResult warning;
    TEST_ASSERT(Layout_CheckSpatial(live(),&warning,1)==1 && warning.motion_intervals && warning.severity==LAYOUT_SPATIAL_WARNING);
    TEST_ASSERT(Layout_CheckMotionRange(live(),&live()->objectStore.motion_envelopes[0],"cabinet",0,false,257,&result) && result.status==LAYOUT_MOTION_RANGE_CLEAR);
    ld_test_shutdown_runtime();return true;
}
static bool test_endpoint_contact_and_unavailable_motion(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(false));TEST_ASSERT(part("endpoint",(Vec3){3,0,0},.02f,.02f,.02f));
    TEST_ASSERT(Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];LayoutMotionRangeResult range;
    TEST_ASSERT(Layout_CheckMotionRange(live(),e,"endpoint",0,false,257,&range) && range.status==LAYOUT_MOTION_RANGE_FAILURE && range.position==3 && range.tested_poses==2);
    live()->objectStore.items[1].coreMeta.flags.locked=true;char* before=Layout_SaveToString(live());
    memset(&range,0x5a,sizeof(range));LayoutMotionRangeResult untouched=range;LayoutMotionCoverage coverage={0};
    TEST_ASSERT(!Layout_CheckMotionRange(live(),e,"endpoint",0,false,257,&range) && !memcmp(&range,&untouched,sizeof(range)));
    TEST_ASSERT(!Layout_BuildMotionCoverage(live(),e,&coverage) && !coverage.pieces);
    LayoutSpatialResult result;TEST_ASSERT(Layout_CheckSpatial(live(),&result,1)==1 && !result.measurable && result.severity==LAYOUT_SPATIAL_WARNING && same(before));Layout_FreeString(before);
    live()->objectStore.items[1].coreMeta.flags.locked=false;LayoutConstraint c=live()->objectStore.constraints[0];c.travel_max=4;TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(!Layout_CheckMotionRange(live(),e,"endpoint",0,false,257,&range));ld_test_shutdown_runtime();return true;
}
static bool test_static_target_boundary_for_downstream_follower(void) {
    ld_test_init_runtime();TEST_ASSERT(rig(false));TEST_ASSERT(part("follower",(Vec3){1,1,0},.1f,.1f,.1f));
    LayoutConstraint c={.kind=LAYOUT_CONSTRAINT_DISTANCE,.axis={0,1,0},.target=1};snprintf(c.id,64,"follower_rule");snprintf(c.a.entity_id,64,"B");snprintf(c.b.entity_id,64,"follower");
    TEST_ASSERT(Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL) && Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    const LayoutMotionEnvelope* e=&live()->objectStore.motion_envelopes[0];LayoutMotionRangeResult range;LayoutMotionInspection sampled;
    TEST_ASSERT(!Layout_MotionTargetStatic(live(),e->rule_id,&live()->objectStore.items[2]));char* before=Layout_SaveToString(live());
    TEST_ASSERT(!Layout_CheckMotionRange(live(),e,"follower",0,false,257,&range) && !Layout_InspectMotionObstruction(live(),e,"follower",0,false,&sampled));
    LayoutSpatialResult result;TEST_ASSERT(Layout_CheckSpatial(live(),&result,1)==1 && !result.measurable && result.severity==LAYOUT_SPATIAL_WARNING && same(before));
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_scaled_panel_and_oblique_interval_coverage(void) {
    ld_test_init_runtime();live()->metersPerWorldUnit=.01;live()->scene3d.bounds.enabled=false;
    TEST_ASSERT(part("A",(Vec3){0},10,10,10));PlanePrimitiveCreateParams panel={.width=20,.height=20,.useExplicitFrame=true,.explicitFrame={.origin={100,100,0},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t id;TEST_ASSERT(Layout_CreatePlanePrimitive(live(),&panel,&id,NULL));TEST_ASSERT(core_object_set_identity(&Layout_ObjectStore_Find(&live()->objectStore,id)->coreMeta,"B","plane_primitive").code==CORE_OK);
    LayoutConstraint c={.axis={1,1,0}};snprintf(c.id,64,"movement");snprintf(c.a.entity_id,64,"A");snprintf(c.b.entity_id,64,"B");
    TEST_ASSERT(Layout_InitLinearTravel(live(),&c,sqrt(2),sqrt(2)+2) && Layout_ConstraintEdit(live(),&c,NULL,NULL,NULL));
    TEST_ASSERT(part("far",(Vec3){150,150,30},10,10,10) && Layout_GenerateMotionEnvelope(live(),"movement",33,NULL,NULL));
    LayoutMotionCoverage coverage={0};TEST_ASSERT(Layout_BuildMotionCoverage(live(),&live()->objectStore.motion_envelopes[0],&coverage));
    Object3D pose;for (int i=0;i<=64;++i){TEST_ASSERT(Layout_SampleMotion(live(),"movement",sqrt(2)+i/32.,&pose) && covered(&pose,&coverage));}
    LayoutMotionRangeResult range;TEST_ASSERT(Layout_CheckMotionRange(live(),&live()->objectStore.motion_envelopes[0],"far",.1,true,257,&range) && range.status==LAYOUT_MOTION_RANGE_CLEAR);
    Layout_FreeMotionCoverage(&coverage);ld_test_shutdown_runtime();return true;
}
static bool test_mouse_refined_pose_without_scene_edit(void) {
    ld_test_init_runtime();TEST_ASSERT(thin_travel());Global_SetWindowSize(1800,1440);UIPanel_OnWindowResized(1800,1440);
    UIPanel_Get()->parts.mode=5;UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_PARTS);UIPanel_SpatialRunChecks();UIPanel_LayoutParts();
    TEST_ASSERT(parts_click(9000));UIPanel_LayoutParts();TEST_ASSERT(parts_click(PARTS_MOTION_INSPECT) && !UIPanel_Get()->spatial.inspection.hit);
    UIPanel_LayoutParts();SDL_Rect control;TEST_ASSERT(UIPanel_PartsControlRect(PARTS_MOTION_RANGE,&control) && control.y+control.h<1440);
    char* before=Layout_SaveToString(live());size_t undo=Editor_UndoCount(&Global_Get()->editor);
    TEST_ASSERT(parts_click(PARTS_MOTION_RANGE) && UIPanel_Get()->spatial.range_checked && UIPanel_Get()->spatial.range_result.status==LAYOUT_MOTION_RANGE_FAILURE);
    TEST_ASSERT(fabs(UIPanel_Get()->spatial.preview_position-1.03125)<.003 && same(before) && Editor_UndoCount(&Global_Get()->editor)==undo);
    UIPanel_LayoutParts();TEST_ASSERT(parts_click(PARTS_MOTION_NEXT) && fabs(UIPanel_Get()->spatial.preview_position-1.0625)<1e-8);
    TEST_ASSERT(parts_click(PARTS_MOTION_PREVIOUS) && fabs(UIPanel_Get()->spatial.preview_position-1)<1e-8);
    TEST_ASSERT(parts_click(PARTS_MOTION_CLOSE) && !UIPanel_Get()->spatial.preview_active && same(before));Layout_FreeString(before);
    ld_test_shutdown_runtime();return true;
}

static bool test_selected_saved_motion_and_history(void) {
    ld_test_init_runtime(); TEST_ASSERT(group_rig(false));
    GlobalState* state=Global_Get();Global_SetWindowSize(1200,1000);UIPanel_OnWindowResized(1200,1000);
    UIPanelState* ui=UIPanel_Get();
    char* before=Layout_SaveToString(live());
    state->editor.selectedObject3DId=live()->objectStore.items[1].objectId;
    UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_OBJECT);UIPanel_OnWindowResized(1200,1000);
    int buttons_count=0;const UIButton* buttons=UIPanel_GetButtons(ui,&buttons_count);bool found=false;
    for(int i=0;i<buttons_count;++i) if(buttons[i].id==UI_BTN_SAVED_MOTION && buttons[i].bounds.w>0) {
        SDL_Event event={.type=SDL_MOUSEBUTTONDOWN};event.button.button=SDL_BUTTON_LEFT;
        event.button.x=buttons[i].bounds.x+buttons[i].bounds.w/2;event.button.y=buttons[i].bounds.y+buttons[i].bounds.h/2;
        AppContext context={0};Input_Handle(&context,&event);found=true;break;
    }
    TEST_ASSERT(found && ui->measurement.active && ui->measurement.motion_preview && ui->measurement.constraint_index==0);
    TEST_ASSERT(ui->measurement.operation==3 && !strcmp(ui->measurement.observed_rule.id,"movement"));
    SDL_Rect slider,field;
    TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&slider));
    TEST_ASSERT(!UIPanel_MeasurementControlRect(MEASURE_TRAVEL_MIN,&field));
    TEST_ASSERT(same(before) && !Editor_UndoCount(&state->editor));
    TEST_ASSERT(measure_click(MEASURE_TRAVEL_TO_MAX) && live()->objectStore.constraintCount==1);
    TEST_ASSERT(fabs(live()->objectStore.items[2].transform.position.x-3)<1e-5);
    TEST_ASSERT(InputEditorAction_Undo() && ui->measurement.active && ui->measurement.motion_preview);
    TEST_ASSERT(!strcmp(ui->measurement.observed_rule.id,"movement"));
    double current;TEST_ASSERT(UIPanel_TravelParse(ui->measurement.travel_text[2],false,&current) && fabs(current-1)<1e-5);
    TEST_ASSERT(InputEditorAction_Redo() && UIPanel_TravelParse(ui->measurement.travel_text[2],false,&current) && fabs(current-3)<1e-5);
    TEST_ASSERT(InputEditorAction_Undo());
    state->editor.selectedObject3DId=live()->objectStore.items[2].objectId;UIPanel_LayoutMeasurementPane();
    TEST_ASSERT(!strcmp(ui->measurement.motion_entity,"peer") && ui->measurement.constraint_index==0);
    TEST_ASSERT(UIPanel_BeginEntityMotion("assembly_2") && ui->measurement.constraint_index==0);
    TEST_ASSERT(!UIPanel_BeginEntityMotion("A") && !UIPanel_BeginEntityMotion("missing"));
    state->editor.selectedObject3DId=live()->objectStore.items[0].objectId;TEST_ASSERT(UIPanel_BeginMeasurement() && !ui->measurement.motion_preview);
    state->editor.selectedObject3DId=live()->objectStore.items[2].objectId;UIPanel_LayoutMeasurementPane();
    TEST_ASSERT(ui->measurement.motion_preview && !strcmp(ui->measurement.motion_entity,"peer"));
    TEST_ASSERT(same(before));free(before);
    TEST_ASSERT(measure_click(MEASURE_EDIT_MOTION) && UIPanel_MeasurementControlRect(MEASURE_TRAVEL_MIN,&field));
    TEST_ASSERT(measure_click(MEASURE_EDIT_MOTION) && !UIPanel_MeasurementControlRect(MEASURE_TRAVEL_MIN,&field));
    ld_test_shutdown_runtime();return true;
}
static bool test_selected_saved_hinge(void) {
    ld_test_init_runtime(); TEST_ASSERT(group_rig(true));
    Global_SetWindowSize(1200,1000);UIPanel_OnWindowResized(1200,1000);
    Global_Get()->editor.selectedObject3DId=live()->objectStore.items[2].objectId;
    TEST_ASSERT(UIPanel_BeginMeasurement() && UIPanel_Get()->measurement.operation==4);
    TEST_ASSERT(UIPanel_Get()->measurement.motion_preview && live()->objectStore.constraintCount==1);
    TEST_ASSERT(measure_click(MEASURE_TRAVEL_TO_MAX) && fabs(live()->objectStore.constraints[0].target-110)<1e-5);
    TEST_ASSERT(InputEditorAction_Undo() && fabs(live()->objectStore.constraints[0].target)<1e-5);
    TEST_ASSERT(measure_click(MEASURE_TRAVEL_TO_MAX) && measure_click(MEASURE_TRAVEL_RESET));
    TEST_ASSERT(fabs(live()->objectStore.constraints[0].target)<1e-5 && live()->objectStore.constraintCount==1);
    ld_test_shutdown_runtime();return true;
}
bool motion_run_tests(void) {
    const TestCase tests[]={
        {"selected_saved_motion_history",test_selected_saved_motion_and_history},
        {"selected_saved_hinge",test_selected_saved_hinge},
        {"travel_atomic_sample_history_obstruction",test_travel_samples_undo_and_static_clear_motion_hit},
        {"hinge_dense_intermediate_coverage_offsets",test_hinge_dense_intermediate_coverage_and_offsets},
        {"stale_regenerate_identity_delete",test_stale_regenerate_identity_and_deletion},
        {"schema18_roundtrip_migration_export",test_schema18_roundtrip_negative_migration_and_export},
        {"scaled_panel_locked_failure",test_scaled_panel_sampling_and_locked_failure},
        {"mouse_envelope_workflow",test_mouse_create_regenerate_checks_return_to_movement},
        {"assembly_travel_undo_freshness",test_assembly_travel_envelope_undo_freshness},
        {"scaled_assembly_motion",test_scaled_assembly_motion},
        {"envelope_scope_upgrade_downgrade",test_envelope_scope_upgrade_and_downgrade},
        {"assembly_hinge_rigid_coverage",test_assembly_hinge_coverage_and_slider_rigidity},
        {"assembly_scope_atomic_rejections",test_assembly_scope_rejections_atomic},
        {"schema19_scope_schema18_compat",test_schema19_scope_negative_and_schema18_compatibility},
        {"inspection_hit_false_positive_readonly",test_inspection_hit_false_positive_and_read_only},
        {"mouse_scope_readonly_preview",test_mouse_scope_and_readonly_preview},
        {"interval_assembly_gap_report_pass",test_interval_union_removes_empty_assembly_gap_and_reports_pass},
        {"piecewise_hinge_dense_group_coverage",test_piecewise_hinge_reduction_and_dense_group_coverage},
        {"between_samples_budget_clearance",test_between_samples_contact_budget_and_clearance},
        {"hinge_contact_after_bounds_ambiguity",test_hinge_refinement_finds_contact_after_bounds_ambiguity},
        {"endpoint_unavailable_motion",test_endpoint_contact_and_unavailable_motion},
        {"static_target_follower_boundary",test_static_target_boundary_for_downstream_follower},
        {"scaled_panel_oblique_coverage",test_scaled_panel_and_oblique_interval_coverage},
        {"mouse_refined_pose_readonly",test_mouse_refined_pose_without_scene_edit}
    };return run_test_cases("Motion",tests,sizeof(tests)/sizeof(tests[0]));
}
