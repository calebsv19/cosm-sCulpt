#include "test_layout_internal.h"
#include "Editor/editor_measurement.h"
#include "Editor/editor_numeric_edit.h"
#include "UI/ui_panel_measurement.h"
#include "Input/input_handler.h"
#include "Core/space_mode_adapter.h"
#include "UI/topbar/line_drawing_editor_topbar.h"

static uint32_t prism(const char* name, Vec3 origin) {
    RectPrismPrimitiveCreateParams p = {.width=1, .height=0.5f, .depth=0.2f,
        .useExplicitFrame=true, .explicitFrame={.origin=origin, .axisU={1,0,0}, .axisV={0,1,0}, .normal={0,0,1}}};
    uint32_t id=0;
    if (!Layout_CreateRectPrismPrimitive(&Global_Get()->layout,&p,&id,NULL)) return 0;
    Object3D* o=Layout_ObjectStore_Find(&Global_Get()->layout.objectStore,id);
    if (core_object_set_identity(&o->coreMeta,name,"rect_prism_primitive").code != CORE_OK) return 0;
    return id;
}

static EditorGeometricReference ref(const char* name, EditorReferenceKind kind, Object3DFaceKind face) {
    EditorGeometricReference r={.kind=kind,.face=face};
    snprintf(r.entity_id,sizeof(r.entity_id),"%s",name);
    return r;
}

static bool near_measure(const Layout* l, EditorGeometricReference a, EditorGeometricReference b,
    EditorMeasurementKind kind, Vec3 axis, double expected) {
    EditorMeasurementResult r=Editor_Measure(l,&a,&b,kind,axis);
    TEST_ASSERT(r.status==EDITOR_MEASUREMENT_OK);
    TEST_ASSERT(fabs(r.value-expected)<1e-5);
    return true;
}

static bool test_distances_scales_and_readonly(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0,0,0})); TEST_ASSERT(prism("B",(Vec3){3,4,0}));
    EditorGeometricReference a=ref("A",EDITOR_REFERENCE_ORIGIN,0), b=ref("B",EDITOR_REFERENCE_ORIGIN,0);
    Editor_ClearHistory(&s->editor); char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0},5));
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_PROJECTED_DISTANCE,(Vec3){2,0,0},3));
    TEST_ASSERT(near_measure(&s->layout,b,a,EDITOR_MEASURE_PROJECTED_DISTANCE,(Vec3){1,0,0},-3));
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && Editor_UndoCount(&s->editor)==0);
    free(before); free(after);
    s->layout.metersPerWorldUnit=0.0254;
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0},0.127));
    TEST_ASSERT(near_measure(&s->layout,a,a,EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0},0));
    ld_test_shutdown_runtime(); return true;
}

static bool test_faces_and_gap(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0,0,0})); uint32_t bid=prism("B",(Vec3){1.02f,4,0}); TEST_ASSERT(bid);
    EditorGeometricReference a=ref("A",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_POS_U);
    EditorGeometricReference b=ref("B",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_NEG_U);
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_PARALLEL_PLANE_GAP,(Vec3){0},0.02));
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_DIRECTION_ANGLE,(Vec3){0},180));
    /* Offsetting in Y does not alter infinite-plane gap. This is not clearance. */
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0},sqrt(16.0004)));
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,bid); Object3D base=*o;
    TEST_ASSERT(Layout_RotateObject3D(&s->layout,bid,(Vec3){0,0,1},30,&base,NULL));
    TEST_ASSERT(Editor_Measure(&s->layout,&a,&b,EDITOR_MEASURE_PARALLEL_PLANE_GAP,(Vec3){0}).status==EDITOR_MEASUREMENT_UNSUPPORTED);
    ld_test_shutdown_runtime(); return true;
}

static bool test_angles_use_authored_frame_once(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); uint32_t id=prism("B",(Vec3){2,0,0}); TEST_ASSERT(id);
    Object3D baseline=*Layout_ObjectStore_Find(&s->layout.objectStore,id);
    EditorGeometricReference a=ref("A",EDITOR_REFERENCE_AXIS_U,0), b=ref("B",EDITOR_REFERENCE_AXIS_U,0);
    const float angles[]={90,30,-30,180};
    for (size_t i=0;i<sizeof(angles)/sizeof(angles[0]);++i) {
        TEST_ASSERT(Layout_RotateObject3D(&s->layout,id,(Vec3){0,0,1},angles[i],&baseline,NULL));
        TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_DIRECTION_ANGLE,(Vec3){0},fabs(angles[i])));
        EditorMeasurementResult r=Editor_Measure(&s->layout,&a,&b,EDITOR_MEASURE_PLANAR_ANGLE,(Vec3){0,0,1});
        TEST_ASSERT(r.status==EDITOR_MEASUREMENT_OK);
        TEST_ASSERT(angles[i]==180 ? fabs(fabs(r.value)-180)<1e-4 : fabs(r.value-angles[i])<1e-5);
    }
    TEST_ASSERT(Layout_RotateObject3D(&s->layout,id,(Vec3){0,1,0},30,&baseline,NULL));
    TEST_ASSERT(Editor_Measure(&s->layout,&a,&b,EDITOR_MEASURE_PLANAR_ANGLE,(Vec3){0,0,1}).status==EDITOR_MEASUREMENT_UNSUPPORTED);
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_DIRECTION_ANGLE,(Vec3){0},30));
    ld_test_shutdown_runtime(); return true;
}

static bool test_identity_reopen_undo_delete(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); uint32_t id=prism("B",(Vec3){1,0,0}); TEST_ASSERT(id);
    EditorGeometricReference a=ref("A",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_POS_U);
    EditorGeometricReference b=ref("B",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_NEG_U);
    EditorNumericEdit e={.entity_id="B",.kind=EDITOR_NUMERIC_TRANSLATE,.unit=CORE_UNIT_MILLIMETER,.values={20,0,0}};
    Editor_ClearHistory(&s->editor);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_PARALLEL_PLANE_GAP,(Vec3){0},0.02));
    char* json=Layout_SaveToString(&s->layout); Layout reopened; Layout_Init(&reopened,1);
    TEST_ASSERT(json && Layout_LoadFromString(&reopened,json));
    /* Stable reference survives storage ordering and numeric handle changes. */
    Object3D tmp=reopened.objectStore.items[0]; reopened.objectStore.items[0]=reopened.objectStore.items[1]; reopened.objectStore.items[1]=tmp;
    reopened.objectStore.items[0].objectId=1234;
    TEST_ASSERT(near_measure(&reopened,a,b,EDITOR_MEASURE_PARALLEL_PLANE_GAP,(Vec3){0},0.02));
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_PARALLEL_PLANE_GAP,(Vec3){0},0));
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_PARALLEL_PLANE_GAP,(Vec3){0},0.02));
    Editor_HistoryCapture(&s->editor,&s->layout);
    TEST_ASSERT(Layout_ObjectStore_Delete(&s->layout.objectStore,id));
    TEST_ASSERT(Editor_Measure(&s->layout,&a,&b,EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0}).status==EDITOR_MEASUREMENT_UNRESOLVED);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(near_measure(&s->layout,a,b,EDITOR_MEASURE_PARALLEL_PLANE_GAP,(Vec3){0},0.02));
    free(json); Layout_Free(&reopened); ld_test_shutdown_runtime(); return true;
}

static bool test_invalid_and_unsupported(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t id=prism("A",(Vec3){0}); TEST_ASSERT(id);
    EditorGeometricReference a=ref("A",EDITOR_REFERENCE_AXIS_U,0); EditorResolvedReference out;
    TEST_ASSERT(Editor_Measure(&s->layout,&a,&a,EDITOR_MEASURE_PROJECTED_DISTANCE,(Vec3){0}).status==EDITOR_MEASUREMENT_DEGENERATE);
    TEST_ASSERT(Editor_Measure(&s->layout,&a,&a,EDITOR_MEASURE_PLANAR_ANGLE,(Vec3){NAN,0,0}).status==EDITOR_MEASUREMENT_DEGENERATE);
    EditorGeometricReference bad=ref("missing",EDITOR_REFERENCE_ORIGIN,0);
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&bad,&out)==EDITOR_MEASUREMENT_UNRESOLVED);
    bad=a; memset(bad.entity_id,'x',sizeof(bad.entity_id));
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&bad,&out)==EDITOR_MEASUREMENT_INVALID);
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,id); o->transform.scale.x=2;
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&a,&out)==EDITOR_MEASUREMENT_UNSUPPORTED);
    o->transform.scale.x=1; o->rectPrism.frame.axisU=(Vec3){0};
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&a,&out)==EDITOR_MEASUREMENT_DEGENERATE);
    o->rectPrism.frame.axisU=(Vec3){1,0,0}; o->rectPrism.frame.origin.x=NAN;
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&a,&out)==EDITOR_MEASUREMENT_INVALID);
    o->rectPrism.frame.origin.x=0; s->layout.metersPerWorldUnit=-1;
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&a,&out)==EDITOR_MEASUREMENT_INVALID);
    s->layout.metersPerWorldUnit=1;
    TEST_ASSERT(prism("A",(Vec3){1,0,0}));
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&a,&out)==EDITOR_MEASUREMENT_UNRESOLVED);
    ld_test_shutdown_runtime(); return true;
}

static bool test_ui_selection_and_modal_input(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t id=prism("A",(Vec3){0}); TEST_ASSERT(id); TEST_ASSERT(prism("B",(Vec3){1,0,0}));
    s->editor.selectedObject3DId=id; Editor_ClearHistory(&s->editor);
    char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(UIPanel_BeginMeasurement()); UIPanelState* ui=UIPanel_Get();
    TEST_ASSERT(!UIPanel_IsCapturingKeyboard());
    TEST_ASSERT(strcmp(ui->measurement.refs[0].entity_id,"A")==0 && strcmp(ui->measurement.refs[1].entity_id,"B")==0);
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_RIGHT));
    TEST_ASSERT(ui->measurement.refs[0].kind==EDITOR_REFERENCE_AXIS_U);
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_TAB));
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_LEFT));
    TEST_ASSERT(ui->measurement.refs[1].face==OBJECT3D_FACE_RECT_PRISM_POS_U);
    UIPanel_MeasurementStartValue(3);
    TEST_ASSERT(UIPanel_IsCapturingKeyboard());
    SDL_Event event={.type=SDL_KEYDOWN}; event.key.keysym.sym=SDLK_DELETE;
    Input_Handle(NULL,&event);
    UIPanel_MeasurementStopInput();
    TEST_ASSERT(!UIPanel_IsCapturingKeyboard());
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && Editor_UndoCount(&s->editor)==0);
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_ESCAPE) && !ui->measurement.active);
    free(before);free(after);ld_test_shutdown_runtime();return true;
}

static bool test_plane_reference_and_invalid_face(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    PlanePrimitiveCreateParams p={.width=2,.height=1,.useExplicitFrame=true,
        .explicitFrame={.origin={0,0,2},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t id=0; TEST_ASSERT(Layout_CreatePlanePrimitive(&s->layout,&p,&id,NULL));
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,id);
    TEST_ASSERT(core_object_set_identity(&o->coreMeta,"panel","plane_primitive").code==CORE_OK);
    EditorGeometricReference a=ref("panel",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_PLANE_SURFACE);
    EditorResolvedReference resolved;
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&a,&resolved)==EDITOR_MEASUREMENT_OK);
    TEST_ASSERT(resolved.point_meters[2]==2 && resolved.direction[2]==1 && resolved.is_plane);
    a.face=OBJECT3D_FACE_RECT_PRISM_POS_U;
    TEST_ASSERT(Editor_ResolveReference(&s->layout,&a,&resolved)==EDITOR_MEASUREMENT_UNSUPPORTED);
    s->editor.selectedObject3DId=id; TEST_ASSERT(UIPanel_BeginMeasurement());
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_LEFT));
    TEST_ASSERT(UIPanel_Get()->measurement.refs[0].face==OBJECT3D_FACE_PLANE_SURFACE);
    UIPanel_ResetTransientUiState(); TEST_ASSERT(!UIPanel_Get()->measurement.active);
    ld_test_shutdown_runtime();return true;
}

static bool test_viewport_pick_identity_filtering_and_no_mutation(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t aid=prism("A",(Vec3){-2,0,0}), bid=prism("B",(Vec3){2,0,0});
    TEST_ASSERT(aid && bid);
    s->editor.selectedObject3DId=aid;
    s->freeViewCamera.enabled=false;
    s->activePlane=(ViewPlane){.axis=VIEW_PLANE_XY,.offset=0};
    CorePaneRect vp; TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&s->paneHost,&vp));
    s->grid.gridSize=1; s->grid.scale=40;
    s->grid.offsetX=-(vp.x+vp.width/2)/40;
    s->grid.offsetY=-(vp.y+vp.height*0.7f)/40;
    TEST_ASSERT(UIPanel_BeginMeasurement()); TEST_ASSERT(UIPanel_MeasurementKey(SDLK_k));
    UIPanelState* ui=UIPanel_Get(); TEST_ASSERT(ui->measurement.picking);
    SpaceViewContext view=SpaceAdapter_BuildViewContext(s);
    Vec2 pixel=WorldToScreen(SpaceAdapter_ProjectToView((Vec3){2,0,0},&view),&s->grid);
    Editor_ClearHistory(&s->editor); char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(UIPanel_MeasurementPickAt((int)pixel.x,(int)pixel.y));
    TEST_ASSERT(strcmp(ui->measurement.refs[0].entity_id,"B")==0);
    EditorGeometricReference saved=ui->measurement.refs[0];
    TEST_ASSERT(!UIPanel_MeasurementPickAt((int)pixel.x+20,(int)pixel.y+20));
    TEST_ASSERT(memcmp(&saved,&ui->measurement.refs[0],sizeof(saved))==0);
    TEST_ASSERT(!UIPanel_MeasurementPickAt(0,0));
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && Editor_UndoCount(&s->editor)==0);
    Object3D* b=Layout_ObjectStore_Find(&s->layout.objectStore,bid);
    b->coreMeta.flags.visible=false;
    TEST_ASSERT(!UIPanel_MeasurementPickAt((int)pixel.x,(int)pixel.y));
    b->coreMeta.flags.visible=true;b->coreMeta.flags.selectable=false;
    TEST_ASSERT(!UIPanel_MeasurementPickAt((int)pixel.x,(int)pixel.y));
    b->coreMeta.flags.selectable=true;
    ui->measurement.refs[0]=ref("A",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_POS_U);
    pixel=WorldToScreen(SpaceAdapter_ProjectToView((Vec3){2.5f,0,0},&view),&s->grid);
    TEST_ASSERT(UIPanel_MeasurementPickAt((int)pixel.x,(int)pixel.y));
    TEST_ASSERT(ui->measurement.refs[0].kind==EDITOR_REFERENCE_FACE && ui->measurement.refs[0].face==OBJECT3D_FACE_RECT_PRISM_POS_U);
    TEST_ASSERT(strcmp(ui->measurement.refs[0].entity_id,"B")==0);
    ConstructionPlane3D construction=s->layout.scene3d.constructionPlane;
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_2));
    TEST_ASSERT(s->activePlane.axis==VIEW_PLANE_YZ && !s->freeViewCamera.enabled);
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_3)); TEST_ASSERT(s->activePlane.axis==VIEW_PLANE_XZ);
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_1)); TEST_ASSERT(s->activePlane.axis==VIEW_PLANE_XY);
    TEST_ASSERT(memcmp(&construction,&s->layout.scene3d.constructionPlane,sizeof(construction))==0);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0);
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_ESCAPE));
    TEST_ASSERT(ui->measurement.active && !ui->measurement.picking);
    TEST_ASSERT(UIPanel_MeasurementKey(SDLK_ESCAPE)); TEST_ASSERT(!ui->measurement.active);
    free(before);free(after);ld_test_shutdown_runtime();return true;
}

static bool test_toolbar_view_change_retains_measure_context(void) {
    ld_test_init_runtime();
    GlobalState* s=Global_Get();
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D,true));
    TEST_ASSERT(prism("A",(Vec3){0,0,0}) && prism("B",(Vec3){2,0,0}));
    TEST_ASSERT(UIPanel_BeginMeasurement());
    UIPanelState* ui=UIPanel_Get();
    ui->measurement.operation=3;
    ui->measurement.rules_open=true;
    ui->measurement.picking=true;
    EditorGeometricReference saved=ui->measurement.refs[0];
    Editor_ClearHistory(&s->editor);
    char* before=Layout_SaveToString(&s->layout);
    CorePaneRect pane;
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&s->paneHost,LINE_DRAWING_PANE_ROLE_TOP_BAR,&pane));
    int height=((int)pane.height-26)/2;
    if(height<20)height=20;
    if(height>28)height=28;
    int row=(int)pane.y+10+height+6;
    if(row+height>(int)(pane.y+pane.height)-4)row=(int)(pane.y+pane.height)-height-4;
    int file_width=(int)pane.width/3;
    if(file_width>230)file_width=230;
    if(file_width<150)file_width=150;
    int x=(int)pane.x+10+file_width+6+10;
    TEST_ASSERT(LineDrawingEditorTopbar_HandleClick(x,row+height/2));
    TEST_ASSERT(ui->measurement.active && ui->activeRightTab==UI_PANEL_RIGHT_TAB_MEASURE);
    TEST_ASSERT(ui->measurement.operation==3 && ui->measurement.rules_open && !ui->measurement.picking);
    TEST_ASSERT(memcmp(&saved,&ui->measurement.refs[0],sizeof(saved))==0);
    TEST_ASSERT(LineDrawingEditorTopbar_HandleClick(x+118,row+height/2));
    TEST_ASSERT(ui->measurement.active && ui->measurement.operation==3);
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && Editor_UndoCount(&s->editor)==0);
    free(before);free(after);ld_test_shutdown_runtime();return true;
}

bool measurement_run_tests(void) {
    const TestCase cases[]={
        {"distances_scales_readonly",test_distances_scales_and_readonly},
        {"face_points_and_infinite_plane_gap",test_faces_and_gap},
        {"directed_angles_frame_once",test_angles_use_authored_frame_once},
        {"reference_reopen_undo_delete",test_identity_reopen_undo_delete},
        {"invalid_degenerate_unsupported",test_invalid_and_unsupported},
        {"selectable_refs_modal_input",test_ui_selection_and_modal_input},
        {"plane_face_and_session_reset",test_plane_reference_and_invalid_face},
        {"viewport_reference_pick",test_viewport_pick_identity_filtering_and_no_mutation},
        {"toolbar_view_preserves_measure_context",test_toolbar_view_change_retains_measure_context}
    };
    return run_test_cases("Measurement",cases,sizeof(cases)/sizeof(cases[0]));
}
