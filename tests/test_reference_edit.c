#include "test_layout_internal.h"
#include "Editor/editor_reference_edit.h"
#include "UI/ui_panel_measurement.h"
#include "Input/input_handler.h"
#include "core_scene_compile.h"

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


static EditorReferencePlacement placement(void) {
    return (EditorReferencePlacement){.a=ref("A",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_POS_U),
        .b=ref("B",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_NEG_U),
        .world_axis={2,0,0}, .target_meters=0.02};
}

static bool test_face_placement_undo_save_export(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){3,4,0}));
    Object3D original=s->layout.objectStore.items[0];
    Editor_ClearHistory(&s->editor);
    EditorReferencePlacement p=placement();
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(near_measure(&s->layout,p.a,p.b,EDITOR_MEASURE_PROJECTED_DISTANCE,p.world_axis,0.02));
    TEST_ASSERT(memcmp(&original,&s->layout.objectStore.items[0],sizeof(original))==0);
    TEST_ASSERT(s->layout.objectStore.items[1].transform.position.y==4);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(s->layout.objectStore.items[1].transform.position.x==3);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));
    char* json=Layout_SaveToString(&s->layout); Layout reopened; Layout_Init(&reopened,1);
    TEST_ASSERT(json && Layout_LoadFromString(&reopened,json));
    TEST_ASSERT(near_measure(&reopened,p.a,p.b,EDITOR_MEASURE_PROJECTED_DISTANCE,p.world_axis,0.02));
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(&reopened,"placement_fixture");
    char* runtime=NULL; char diagnostic[256];
    TEST_ASSERT(authored && core_scene_compile_authoring_to_runtime(authored,&runtime,diagnostic,sizeof(diagnostic)).code==CORE_OK);
    cJSON* root=cJSON_Parse(runtime);
    cJSON* o=ld_test_find_object_by_id(cJSON_GetObjectItemCaseSensitive(root,"objects"),"B");
    TEST_ASSERT(o);
    cJSON* position=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(o,"transform"),"position");
    TEST_ASSERT(fabs(cJSON_GetObjectItemCaseSensitive(position,"x")->valuedouble-1.02)<1e-6);
    cJSON_Delete(root); free(runtime); free(authored); free(json); Layout_Free(&reopened);
    ld_test_shutdown_runtime(); return true;
}

static bool test_scaled_negative_and_coincident(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){3,4,2}));
    s->layout.metersPerWorldUnit=0.0254;
    EditorReferencePlacement p=placement(); p.target_meters=-0.05;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(near_measure(&s->layout,p.a,p.b,EDITOR_MEASURE_PROJECTED_DISTANCE,p.world_axis,-0.05));
    p.kind=EDITOR_PLACE_COINCIDENT_POINTS;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(near_measure(&s->layout,p.a,p.b,EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0},0));
    size_t n=Editor_UndoCount(&s->editor); s->layoutDirty=false; s->layoutDirtySinceSave=false;
    EditorNumericEditResult unchanged=Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p);
    TEST_ASSERT(unchanged.status==EDITOR_NUMERIC_UNCHANGED);
    TEST_ASSERT(fabs(unchanged.actual_meters[0]-0.0254)<1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==n && !s->layoutDirty && !s->layoutDirtySinceSave);
    ld_test_shutdown_runtime(); return true;
}

static bool test_atomic_rejections_and_redo(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){1,0,0}));
    EditorReferencePlacement p=placement();
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    Object3D* b=&s->layout.objectStore.items[1];
    b->rectPrism.lockToBounds=true;
    s->layout.scene3d.bounds=(SceneBounds3D){.enabled=true,.clampOnEdit=true,.min={-2,-2,-2},.max={2,2,2}};
    s->layoutDirty=false; s->layoutDirtySinceSave=false;
    size_t n=Editor_UndoCount(&s->editor); char* before=Layout_SaveToString(&s->layout);
    p.target_meters=5;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_CONFLICT);
    p.target_meters=NAN;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_INVALID);
    p.target_meters=0.1; p.world_axis=(Vec3){0};
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_INVALID);
    p=placement(); p.b.entity_id[0]='Z';
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_INVALID);
    p=placement(); p.b=p.a;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_CONFLICT);
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && Editor_UndoCount(&s->editor)==n && Editor_RedoCount(&s->editor)==1);
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    free(before); free(after);
    p=placement(); b->coreMeta.flags.locked=true;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_CONFLICT);
    b->coreMeta.flags.locked=false; b->coreMeta.dimensional_mode=CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    b->coreMeta.locked_plane=CORE_OBJECT_PLANE_XY; p.world_axis=(Vec3){0,0,1}; p.target_meters=0.02;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_CONFLICT);
    TEST_ASSERT(b->transform.position.z==0 && Editor_RedoCount(&s->editor)==1);
    ld_test_shutdown_runtime(); return true;
}

static bool test_oblique_axis_and_precision(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){3,4,2}));
    EditorReferencePlacement p=placement(); p.a=ref("A",EDITOR_REFERENCE_ORIGIN,0); p.b=ref("B",EDITOR_REFERENCE_ORIGIN,0);
    p.world_axis=(Vec3){1,1,0}; p.target_meters=0;
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(near_measure(&s->layout,p.a,p.b,EDITOR_MEASURE_PROJECTED_DISTANCE,p.world_axis,0));
    TEST_ASSERT(fabs(s->layout.objectStore.items[1].transform.position.y-s->layout.objectStore.items[1].transform.position.x-1)<1e-6);
    TEST_ASSERT(s->layout.objectStore.items[1].transform.position.z==2);
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,s->layout.objectStore.items[0].objectId,(Vec3){100000,0,0},NULL));
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,s->layout.objectStore.items[1].objectId,(Vec3){100001,0,0},NULL));
    p.world_axis=(Vec3){1,0,0}; p.target_meters=0.02;
    char* before=Layout_SaveToString(&s->layout); size_t n=Editor_UndoCount(&s->editor);
    TEST_ASSERT(Editor_ApplyReferencePlacement(&s->editor,&s->layout,&p).status==EDITOR_NUMERIC_CONFLICT);
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && Editor_UndoCount(&s->editor)==n);
    free(before); free(after); ld_test_shutdown_runtime(); return true;
}

static void key(SDL_Keycode code) {
    SDL_Event e={.type=SDL_KEYDOWN}; e.key.keysym.sym=code; Input_Handle(NULL,&e);
}

static bool test_modal_placement_input(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t id=prism("A",(Vec3){0}); TEST_ASSERT(id); TEST_ASSERT(prism("B",(Vec3){3,4,0}));
    s->editor.selectedObject3DId=id; Editor_ClearHistory(&s->editor);
    TEST_ASSERT(UIPanel_BeginMeasurement()); UIPanelState* ui=UIPanel_Get();
    key(SDLK_d); TEST_ASSERT(ui->measurement.placing==1);
    EditorGeometricReference a=ui->measurement.refs[0]; key(SDLK_RIGHT); key(SDLK_DELETE); key(SDLK_k);
    TEST_ASSERT(memcmp(&a,&ui->measurement.refs[0],sizeof(a))==0 && !ui->measurement.picking);
    key(SDLK_RETURN); TEST_ASSERT(ui->measurement.placing==1 && ui->measurement.placement_message[0]);
    SDL_Event event={.type=SDL_TEXTINPUT}; snprintf(event.text.text,sizeof(event.text.text),"20 mm"); Input_Handle(NULL,&event);
    key(SDLK_RETURN); TEST_ASSERT(!ui->measurement.placing && ui->measurement.active);
    TEST_ASSERT(near_measure(&s->layout,ui->measurement.refs[0],ui->measurement.refs[1],EDITOR_MEASURE_PROJECTED_DISTANCE,(Vec3){1,0,0},0.02));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    key(SDLK_c); TEST_ASSERT(ui->measurement.placing==2); key(SDLK_ESCAPE);
    TEST_ASSERT(!ui->measurement.placing && Editor_UndoCount(&s->editor)==1);
    key(SDLK_c); key(SDLK_RETURN);
    TEST_ASSERT(near_measure(&s->layout,ui->measurement.refs[0],ui->measurement.refs[1],EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0},0));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==2);
    key(SDLK_d); TEST_ASSERT(UIPanel_MeasurementText("5 m")); key(SDLK_BACKSPACE);
    TEST_ASSERT(strcmp(ui->measurement.placement_text,"5 ")==0); key(SDLK_ESCAPE);
    key(SDLK_ESCAPE); TEST_ASSERT(!ui->measurement.active);
    ld_test_shutdown_runtime(); return true;
}

bool reference_edit_run_tests(void) {
    const TestCase cases[]={
        {"face_placement_undo_save_export",test_face_placement_undo_save_export},
        {"scaled_negative_coincident",test_scaled_negative_and_coincident},
        {"atomic_rejections_redo",test_atomic_rejections_and_redo},
        {"oblique_axis_precision",test_oblique_axis_and_precision},
        {"modal_placement_input",test_modal_placement_input}
    };
    return run_test_cases("ReferenceEdit",cases,sizeof(cases)/sizeof(cases[0]));
}
