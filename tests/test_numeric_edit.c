#include "test_layout_internal.h"
#include "Editor/editor_numeric_edit.h"
#include "UI/input_ui_panel.h"
#include "UI/ui_panel_internal.h"
#include "core_scene_compile.h"

static uint32_t create_prism(GlobalState* state) {
    RectPrismPrimitiveCreateParams p = {
        .width = 1, .height = 0.5f, .depth = 0.2f, .useExplicitFrame = true,
        .explicitFrame = {.axisU={1,0,0}, .axisV={0,1,0}, .normal={0,0,1}}
    };
    uint32_t id = 0;
    bool adjusted = false;
    if (!Layout_CreateRectPrismPrimitive(&state->layout, &p, &id, &adjusted)) return 0;
    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, id);
    if (core_object_set_identity(&object->coreMeta, "fixture.panel", "rect_prism_primitive").code != CORE_OK) return 0;
    Editor_ClearHistory(&state->editor);
    return id;
}

static EditorNumericEdit command(EditorNumericEditKind kind, CoreUnitKind unit, double x, double y, double z) {
    return (EditorNumericEdit){.entity_id="fixture.panel", .kind=kind, .unit=unit, .values={x,y,z}};
}

static bool test_length_parser(void) {
    double m = 0;
    TEST_ASSERT(Editor_ParseLength("3.5 in", CORE_UNIT_METER, &m));
    TEST_ASSERT(fabs(m - 0.0889) < 1e-12);
    TEST_ASSERT(Editor_ParseLength("20mm", CORE_UNIT_FOOT, &m));
    TEST_ASSERT(fabs(m - 0.020) < 1e-12);
    TEST_ASSERT(Editor_ParseLength("0.5", CORE_UNIT_METER, &m));
    TEST_ASSERT(m == 0.5);
    TEST_ASSERT(Editor_ParseLength(" -2 CM ", CORE_UNIT_METER, &m));
    TEST_ASSERT(fabs(m + 0.02) < 1e-12);
    const char* invalid[] = {"", "nan", "inf m", "1e999", "3.5 zz", "1 m extra", "1/2 m", "2 m 3 cm"};
    for (size_t i=0; i<sizeof(invalid)/sizeof(invalid[0]); ++i)
        TEST_ASSERT(!Editor_ParseLength(invalid[i], CORE_UNIT_METER, &m));
    return true;
}

static bool test_exact_edits_undo_reopen_and_runtime_export(void) {
    ld_test_init_runtime();
    GlobalState* s = Global_Get();
    uint32_t id = create_prism(s);
    TEST_ASSERT(id);
    s->layout.objectStore.items[0].coreMeta.flags.visible = false;
    EditorNumericEdit e = command(EDITOR_NUMERIC_WIDTH, CORE_UNIT_INCH, 3.5,0,0);
    EditorNumericEditResult r = Editor_ApplyNumericEdit(&s->editor, &s->layout, &e);
    TEST_ASSERT(r.status == EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(fabs(r.actual_meters[0]-0.0889) < 1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor) == 1);
    e = command(EDITOR_NUMERIC_TRANSLATE, CORE_UNIT_MILLIMETER, 25,0,0);
    r = Editor_ApplyNumericEdit(&s->editor, &s->layout, &e);
    TEST_ASSERT(r.status == EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(fabs(r.actual_meters[0]-0.025) < 1e-6);
    TEST_ASSERT(Editor_Undo(&s->editor, &s->layout));
    TEST_ASSERT(fabs(s->layout.objectStore.items[0].transform.position.x) < 1e-6);
    TEST_ASSERT(Editor_Redo(&s->editor, &s->layout));
    TEST_ASSERT(strcmp(s->layout.objectStore.items[0].coreMeta.object_id, "fixture.panel") == 0);
    TEST_ASSERT(!s->layout.objectStore.items[0].coreMeta.flags.visible);
    char* saved = Layout_SaveToString(&s->layout);
    Layout reopened;
    Layout_Init(&reopened,1);
    TEST_ASSERT(saved && Layout_LoadFromString(&reopened,saved));
    TEST_ASSERT(fabs(reopened.objectStore.items[0].rectPrism.width-0.0889) < 1e-6);
    TEST_ASSERT(fabs(reopened.objectStore.items[0].transform.position.x-0.025) < 1e-6);
    char* authoring = LineDrawingCanonicalScene_ExportLayoutToString(&reopened,"numeric_fixture");
    char* runtime = NULL;
    char diagnostic[256];
    TEST_ASSERT(authoring && core_scene_compile_authoring_to_runtime(authoring,&runtime,diagnostic,sizeof(diagnostic)).code == CORE_OK);
    cJSON* root = cJSON_Parse(runtime);
    cJSON* object = ld_test_find_object_by_id(cJSON_GetObjectItemCaseSensitive(root,"objects"),"fixture.panel");
    TEST_ASSERT(object);
    cJSON* transform = cJSON_GetObjectItemCaseSensitive(object,"transform");
    cJSON* position = cJSON_GetObjectItemCaseSensitive(transform,"position");
    TEST_ASSERT(cJSON_IsObject(position));
    TEST_ASSERT(fabs(cJSON_GetObjectItemCaseSensitive(position,"x")->valuedouble-0.025)<1e-6);
    cJSON_Delete(root); free(runtime); free(authoring); free(saved); Layout_Free(&reopened);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_bounds_conflict_preserves_document_history_and_dirty_state(void) {
    ld_test_init_runtime();
    GlobalState* s=Global_Get();
    TEST_ASSERT(create_prism(s));
    s->layout.scene3d.bounds=(SceneBounds3D){.enabled=true,.clampOnEdit=true,.min={-1,-1,-1},.max={1,1,1}};
    s->layout.objectStore.items[0].rectPrism.lockToBounds=true;
    s->layoutDirty=false; s->layoutDirtySinceSave=false;
    char* before=Layout_SaveToString(&s->layout);
    EditorNumericEdit e=command(EDITOR_NUMERIC_WIDTH,CORE_UNIT_METER,5,0,0);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_CONFLICT);
    e=command(EDITOR_NUMERIC_POSITION,CORE_UNIT_METER,2,0,0);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_CONFLICT);
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0 && Editor_RedoCount(&s->editor)==0);
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    free(before);free(after);ld_test_shutdown_runtime();return true;
}

static bool test_rejections_and_noop_preserve_redo(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(create_prism(s));
    EditorNumericEdit e=command(EDITOR_NUMERIC_WIDTH,CORE_UNIT_METER,0.5,0,0);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    s->layoutDirty=false;s->layoutDirtySinceSave=false;
    e=command(EDITOR_NUMERIC_WIDTH,CORE_UNIT_METER,1,0,0);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_UNCHANGED);
    e.values[0]=NAN;
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_INVALID);
    e.values[0]=-1;
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_INVALID);
    e.entity_id="missing";e.values[0]=0.5;
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_NOT_FOUND);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0 && Editor_RedoCount(&s->editor)==1);
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    ld_test_shutdown_runtime();return true;
}

static bool test_scaled_document_and_plane_lock(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(create_prism(s));
    s->layout.metersPerWorldUnit=0.0254;
    EditorNumericEdit e=command(EDITOR_NUMERIC_WIDTH,CORE_UNIT_MILLIMETER,88.9,0,0);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(fabs(s->layout.objectStore.items[0].rectPrism.width-3.5)<1e-6);
    Object3D* object=&s->layout.objectStore.items[0];
    object->coreMeta.dimensional_mode=CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    object->coreMeta.locked_plane=CORE_OBJECT_PLANE_XY;
    e=command(EDITOR_NUMERIC_TRANSLATE,CORE_UNIT_MILLIMETER,0,0,25);
    const size_t history=Editor_UndoCount(&s->editor);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_CONFLICT);
    TEST_ASSERT(object->transform.position.z==0);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==history);
    object->transform.scale.x=2;
    e=command(EDITOR_NUMERIC_WIDTH,CORE_UNIT_METER,1,0,0);
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_UNSUPPORTED);
    ld_test_shutdown_runtime();return true;
}

static bool test_dimension_dialog_suffix_and_visible_conflict(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();uint32_t id=create_prism(s);TEST_ASSERT(id);
    s->editor.selectedObject3DId=id;
    UIPanelState* ui=UIPanel_Get();ui->displayUnit=CORE_UNIT_FOOT;
    TEST_ASSERT(UIPanel_BeginPrismDimensionDialog(UI_PRISM_DIMENSION_TARGET_WIDTH));
    ui->prismDimensionDialog.buffer[0]='\0';
    ui->prismDimensionDialog.length=ui->prismDimensionDialog.cursor=0;
    TEST_ASSERT(UIPanel_HandleTextInput("1/2 m"));
    TEST_ASSERT(strcmp(ui->prismDimensionDialog.buffer,"1/2 m")==0);
    TEST_ASSERT(!UIPanel_ApplyPrismDimensionDialog(ui));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0);
    ui->prismDimensionDialog.buffer[0]='\0';
    ui->prismDimensionDialog.length=ui->prismDimensionDialog.cursor=0;
    TEST_ASSERT(UIPanel_HandleTextInput("3.5 in"));
    TEST_ASSERT(UIPanel_ApplyPrismDimensionDialog(ui));
    TEST_ASSERT(fabs(s->layout.objectStore.items[0].rectPrism.width-0.0889)<1e-6);
    s->layout.scene3d.bounds=(SceneBounds3D){.enabled=true,.clampOnEdit=true,.min={-1,-1,-1},.max={1,1,1}};
    s->layout.objectStore.items[0].rectPrism.lockToBounds=true;
    const size_t count=Editor_UndoCount(&s->editor);
    TEST_ASSERT(UIPanel_BeginPrismDimensionDialog(UI_PRISM_DIMENSION_TARGET_WIDTH));
    snprintf(ui->prismDimensionDialog.buffer,sizeof(ui->prismDimensionDialog.buffer),"5 m");
    TEST_ASSERT(!UIPanel_ApplyPrismDimensionDialog(ui));
    TEST_ASSERT(ui->prismDimensionDialog.active && ui->prismDimensionDialog.validationMessage[0]);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==count);
    ld_test_shutdown_runtime();return true;
}

bool numeric_edit_run_tests(void) {
    const TestCase cases[]={
        {"length_parser",test_length_parser},
        {"exact_edit_undo_reopen_export",test_exact_edits_undo_reopen_and_runtime_export},
        {"bounds_failure_atomicity",test_bounds_conflict_preserves_document_history_and_dirty_state},
        {"rejections_noop_preserve_redo",test_rejections_and_noop_preserve_redo},
        {"scaled_document_plane_lock",test_scaled_document_and_plane_lock},
        {"dimension_dialog_suffix_conflict",test_dimension_dialog_suffix_and_visible_conflict}
    };
    return run_test_cases("NumericEdit",cases,sizeof(cases)/sizeof(cases[0]));
}
