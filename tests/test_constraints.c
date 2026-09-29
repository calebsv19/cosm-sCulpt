#include "test_layout_internal.h"
#include "Editor/editor_measurement.h"
#include "Editor/editor_numeric_edit.h"
#include "Layout/layout_constraints.h"
#include "Layout/asset/layout_object_asset_mesh_authoring.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_right_scroll.h"
#include "Input/input_handler.h"
#include "Input/input_mouse_drag.h"
#include "Input/input_mouse_drag_shared.h"
#include "core_scene_compile.h"
#include <unistd.h>

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


static LayoutConstraint rule(const char* id, const char* a, const char* b, double meters) {
    LayoutConstraint c={.a=ref(a,EDITOR_REFERENCE_ORIGIN,0), .b=ref(b,EDITOR_REFERENCE_ORIGIN,0),
        .kind=LAYOUT_CONSTRAINT_DISTANCE, .axis={1,0,0}, .target=meters};
    snprintf(c.id,sizeof(c.id),"%s",id); return c;
}
static bool put(LayoutConstraint c) {
    GlobalState* s=Global_Get();
    return Layout_ConstraintEdit(&s->layout,&c,NULL,Editor_ReserveGeometryHistory,&s->editor);
}
static bool valid(void) { return Layout_ValidateConstraints(&Global_Get()->layout,NULL,0); }
static bool test_driver_target_chain_and_undo(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}); TEST_ASSERT(a);
    TEST_ASSERT(prism("B",(Vec3){5,2,0})); TEST_ASSERT(prism("C",(Vec3){10,3,0}));
    /* Reverse storage order is deliberately not dependency order. */
    TEST_ASSERT(put(rule("bc","B","C",0.5))); TEST_ASSERT(put(rule("ab","A","B",1)));
    TEST_ASSERT(valid() && s->layout.objectStore.items[2].transform.position.x==1.5f);
    Editor_ClearHistory(&s->editor);
    EditorNumericEdit e={.entity_id="A",.kind=EDITOR_NUMERIC_TRANSLATE,.unit=CORE_UNIT_MILLIMETER,.values={25,0,0}};
    TEST_ASSERT(Editor_ApplyNumericEdit(&s->editor,&s->layout,&e).status==EDITOR_NUMERIC_APPLIED);
    TEST_ASSERT(valid() && fabs(s->layout.objectStore.items[2].transform.position.x-1.525)<1e-6);
    TEST_ASSERT(s->layout.objectStore.items[1].transform.position.y==2 && s->layout.objectStore.items[2].transform.position.y==3);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1 && Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(valid() && s->layout.objectStore.items[0].transform.position.x==0);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout) && valid());
    TEST_ASSERT(put(rule("ab","A","B",2)));
    TEST_ASSERT(valid() && fabs(s->layout.objectStore.items[2].transform.position.x-2.525)<1e-6);
    TEST_ASSERT(s->layout.objectStore.constraintCount==2);
    ld_test_shutdown_runtime(); return true;
}
static bool test_atomic_conflicts_cycles_and_redo(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}), b=prism("B",(Vec3){1,0,0}); TEST_ASSERT(a && b);
    TEST_ASSERT(put(rule("ab","A","B",1)));
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,a,(Vec3){1,0,0},NULL));
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    char* before=Layout_SaveToString(&s->layout); size_t undo=Editor_UndoCount(&s->editor), redo=Editor_RedoCount(&s->editor);
    s->layoutDirty=false; s->layoutDirtySinceSave=false;
    TEST_ASSERT(!put(rule("ba","B","A",-1)));
    TEST_ASSERT(!put(rule("duplicate","A","B",2)));
    TEST_ASSERT(!Layout_SetObject3DPosition(&s->layout,b,(Vec3){4,0,0},NULL));
    TEST_ASSERT(!Layout_ObjectStore_Delete(&s->layout.objectStore,a));
    s->editor.selectedObject3DId=b;
    TEST_ASSERT(!UIPanel_SceneListDeleteSelectedObject());
    AppContext ctx={0}; SDL_Event event={.type=SDL_KEYDOWN}; event.key.keysym.sym=SDLK_DELETE; Input_Handle(&ctx,&event);
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && strcmp(before,after)==0);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==undo && Editor_RedoCount(&s->editor)==redo);
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    free(before); free(after);
    /* Perpendicular motion is still a free degree of freedom. */
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,b,(Vec3){1,2,0},NULL) && valid());
    ld_test_shutdown_runtime(); return true;
}
static bool test_dependent_bounds_and_lock_rollback(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}); TEST_ASSERT(a); TEST_ASSERT(prism("B",(Vec3){1,0,0}));
    TEST_ASSERT(put(rule("ab","A","B",1)));
    s->layout.scene3d.bounds=(SceneBounds3D){.enabled=true,.clampOnEdit=true,.min={-2,-2,-2},.max={2,2,2}};
    s->layout.objectStore.items[1].rectPrism.lockToBounds=true;
    Editor_ClearHistory(&s->editor); char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(!Layout_SetObject3DPosition(&s->layout,a,(Vec3){2,0,0},NULL));
    char* after=Layout_SaveToString(&s->layout); TEST_ASSERT(strcmp(before,after)==0 && !Editor_UndoCount(&s->editor));
    free(before); free(after);
    s->layout.objectStore.items[1].coreMeta.flags.locked=true;
    TEST_ASSERT(!Layout_SetObject3DPosition(&s->layout,a,(Vec3){0.1f,0,0},NULL));
    TEST_ASSERT(s->layout.objectStore.items[0].transform.position.x==0);
    s->layout.objectStore.items[1].coreMeta.flags.locked=false;
    s->layout.objectStore.items[1].coreMeta.dimensional_mode=CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    s->layout.objectStore.items[1].coreMeta.locked_plane=CORE_OBJECT_PLANE_XY;
    LayoutConstraint c=rule("ab","A","B",0); c.axis=(Vec3){0,0,1}; TEST_ASSERT(put(c));
    TEST_ASSERT(!Layout_SetObject3DPosition(&s->layout,a,(Vec3){0,0,1},NULL));
    TEST_ASSERT(valid() && s->layout.objectStore.items[0].transform.position.z==0);
    ld_test_shutdown_runtime(); return true;
}
static bool test_resize_rotate_scale_routes(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}), b=prism("B",(Vec3){2,0,0}); TEST_ASSERT(a && b);
    LayoutConstraint c=rule("gap","A","B",0.02);
    c.a=ref("A",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_POS_U);
    c.b=ref("B",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_NEG_U); TEST_ASSERT(put(c));
    TEST_ASSERT(Layout_SetRectPrismDimensions(&s->layout,a,2,0.5f,0.2f,NULL) && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.items[1].transform.position.x-1.52)<1e-6);
    TEST_ASSERT(Layout_ResizeRectPrismPrimitiveFromHandle(&s->layout,a,PLANE_RESIZE_HANDLE_EDGE_POS_U,(Vec3){2,0,0},NULL) && valid());
    Vec3 point; TEST_ASSERT(Layout_RectPrismResizeHandleWorldPoint(&s->layout.objectStore.items[0],RECT_PRISM_RESIZE_HANDLE_CORNER_0,&point));
    point.x-=0.1f; point.y-=0.1f; point.z-=0.1f;
    TEST_ASSERT(Layout_ResizeRectPrismFrom3DHandle(&s->layout,a,RECT_PRISM_RESIZE_HANDLE_CORNER_0,point,NULL) && valid());
    TEST_ASSERT(Layout_ResizeRectPrismDepthFromFaceHandle(&s->layout,a,PLANE_RESIZE_HANDLE_EDGE_POS_U,true,(Vec3){2,0,0.5f},NULL) && valid());
    Object3D base=s->layout.objectStore.items[0];
    TEST_ASSERT(Layout_RotateObject3D(&s->layout,a,(Vec3){0,0,1},30,&base,NULL) && valid());
    base=s->layout.objectStore.items[0];
    TEST_ASSERT(Layout_ScaleObject3D(&s->layout,a,(Vec3){1.2f,1.2f,1.2f},&base,NULL) && valid());
    size_t n=Editor_UndoCount(&s->editor);
    TEST_ASSERT(!Layout_SetRectPrismDimensions(&s->layout,b,2,0.5f,0.2f,NULL));
    TEST_ASSERT(valid() && Editor_UndoCount(&s->editor)==n);
    ld_test_shutdown_runtime(); return true;
}
static bool test_plane_resize_routes(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    PlanePrimitiveCreateParams p={.width=1,.height=1,.useExplicitFrame=true,.explicitFrame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t a=0; TEST_ASSERT(Layout_CreatePlanePrimitive(&s->layout,&p,&a,NULL));
    TEST_ASSERT(core_object_set_identity(&s->layout.objectStore.items[0].coreMeta,"A","plane_primitive").code==CORE_OK);
    TEST_ASSERT(prism("B",(Vec3){2,0,0})); TEST_ASSERT(put(rule("ab","A","B",1)));
    TEST_ASSERT(Layout_SetPlaneDimensions(&s->layout,a,2,2,NULL) && valid());
    TEST_ASSERT(Layout_ResizePlanePrimitiveFromHandle(&s->layout,a,PLANE_RESIZE_HANDLE_EDGE_POS_U,(Vec3){2,0,0},NULL) && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.items[1].transform.position.x-1.5)<1e-6);
    ld_test_shutdown_runtime(); return true;
}
static bool test_save_reopen_invalid_import_and_export(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){2,0,0}));
    s->layout.metersPerWorldUnit=0.0254; TEST_ASSERT(put(rule("ab","A","B",0.02)));
    char* json=Layout_SaveToString(&s->layout); TEST_ASSERT(json);
    Layout loaded; Layout_Init(&loaded,1); TEST_ASSERT(Layout_LoadFromString(&loaded,json));
    TEST_ASSERT(loaded.objectStore.constraintCount==1 && Layout_ValidateConstraints(&loaded,NULL,0));
    TEST_ASSERT(Layout_SetObject3DPosition(&loaded,loaded.objectStore.items[0].objectId,(Vec3){1,0,0},NULL));
    TEST_ASSERT(Layout_ValidateConstraints(&loaded,NULL,0));
    cJSON* root=cJSON_Parse(json); cJSON* rules=cJSON_GetObjectItemCaseSensitive(root,"geometricConstraints");
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(rules,0),"target",cJSON_CreateNumber(5));
    char* invalid=cJSON_PrintUnformatted(root); char* before=Layout_SaveToString(&loaded);
    s->layoutDirty=false; s->layoutDirtySinceSave=false;
    TEST_ASSERT(!Layout_LoadFromString(&loaded,invalid));
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    char* after=Layout_SaveToString(&loaded); TEST_ASSERT(strcmp(before,after)==0);
    free(invalid); free(before); free(after); cJSON_Delete(root);
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(&s->layout,"constrained_fixture");
    char* runtime=NULL; char diagnostic[256];
    TEST_ASSERT(authored && core_scene_compile_authoring_to_runtime(authored,&runtime,diagnostic,sizeof(diagnostic)).code==CORE_OK);
    TEST_ASSERT(strstr(authored,"geometricConstraints") && strstr(authored,"ab"));
    s->layout.objectStore.items[1].transform.position.x+=1;
    s->layout.objectStore.items[1].rectPrism.frame.origin.x+=1;
    TEST_ASSERT(!Layout_SaveToString(&s->layout));
    TEST_ASSERT(!LineDrawingCanonicalScene_ExportLayoutToString(&s->layout,"invalid"));
    free(authored); free(runtime); free(json); Layout_Free(&loaded); ld_test_shutdown_runtime(); return true;
}
static bool test_remove_undo_and_coincident_pivot(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}), b=prism("B",(Vec3){2,3,4}); TEST_ASSERT(a && b);
    LayoutConstraint c=rule("pivot","A","B",0); c.kind=LAYOUT_CONSTRAINT_COINCIDENT;
    c.a=ref("A",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_POS_U);
    c.b=ref("B",EDITOR_REFERENCE_FACE,OBJECT3D_FACE_RECT_PRISM_NEG_U);
    TEST_ASSERT(put(c) && valid());
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,a,(Vec3){1,2,3},NULL) && valid());
    TEST_ASSERT(near_measure(&s->layout,c.a,c.b,EDITOR_MEASURE_POINT_DISTANCE,(Vec3){0},0));
    TEST_ASSERT(Layout_ConstraintEdit(&s->layout,NULL,"pivot",Editor_ReserveGeometryHistory,&s->editor));
    TEST_ASSERT(!s->layout.objectStore.constraintCount);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && valid() && s->layout.objectStore.constraintCount==1);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout) && !s->layout.objectStore.constraintCount);
    TEST_ASSERT(Layout_ObjectStore_Delete(&s->layout.objectStore,b));
    ld_test_shutdown_runtime(); return true;
}
static bool test_planar_mate_rotation_target_and_failure(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}), b=prism("B",(Vec3){2,0,0}); TEST_ASSERT(a && b);
    LayoutConstraint c=rule("mate","A","B",30); c.kind=LAYOUT_CONSTRAINT_PLANAR_MATE;
    c.a=ref("A",EDITOR_REFERENCE_AXIS_U,0); c.b=ref("B",EDITOR_REFERENCE_AXIS_U,0); c.axis=(Vec3){0,0,1};
    TEST_ASSERT(put(c) && valid());
    TEST_ASSERT(near_measure(&s->layout,c.a,c.b,EDITOR_MEASURE_PLANAR_ANGLE,c.axis,30));
    Object3D base=s->layout.objectStore.items[0];
    TEST_ASSERT(Layout_RotateObject3D(&s->layout,a,(Vec3){0,0,1},90,&base,NULL) && valid());
    TEST_ASSERT(near_measure(&s->layout,c.a,c.b,EDITOR_MEASURE_PLANAR_ANGLE,c.axis,30));
    c.target=-30; TEST_ASSERT(put(c) && valid());
    c.target=180; TEST_ASSERT(put(c) && valid());
    char* saved=Layout_SaveToString(&s->layout); Layout reopened; Layout_Init(&reopened,1);
    TEST_ASSERT(saved && Layout_LoadFromString(&reopened,saved) && Layout_ValidateConstraints(&reopened,NULL,0));
    EditorMeasurementResult angle=Editor_Measure(&reopened,&c.a,&c.b,EDITOR_MEASURE_PLANAR_ANGLE,c.axis);
    TEST_ASSERT(angle.status==EDITOR_MEASUREMENT_OK && fabs(remainder(angle.value-180,360))<1e-4);
    free(saved); Layout_Free(&reopened);
    base=s->layout.objectStore.items[0]; size_t n=Editor_UndoCount(&s->editor);
    TEST_ASSERT(!Layout_RotateObject3D(&s->layout,a,(Vec3){1,0,0},30,&base,NULL));
    TEST_ASSERT(valid() && Editor_UndoCount(&s->editor)==n);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && valid());
    ld_test_shutdown_runtime(); return true;
}
static void key(SDL_Keycode code) { (void)UIPanel_MeasurementKey(code); }
static bool test_rule_ui_create_select_update_remove(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}); TEST_ASSERT(a); TEST_ASSERT(prism("B",(Vec3){2,0,0}));
    s->editor.selectedObject3DId=a; Editor_ClearHistory(&s->editor); TEST_ASSERT(UIPanel_BeginMeasurement());
    key(SDLK_r); TEST_ASSERT(UIPanel_MeasurementText("20 mm")); key(SDLK_RETURN);
    TEST_ASSERT(s->layout.objectStore.constraintCount==1 && valid() && !UIPanel_Get()->measurement.placing);
    key(SDLK_q); TEST_ASSERT(UIPanel_Get()->measurement.constraint_index==0);
    key(SDLK_r); TEST_ASSERT(UIPanel_MeasurementText("50 mm")); key(SDLK_RETURN);
    TEST_ASSERT(s->layout.objectStore.constraintCount==1 && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.constraints[0].target-0.05)<1e-9);
    key(SDLK_q); key(SDLK_DELETE); key(SDLK_RETURN);
    TEST_ASSERT(s->layout.objectStore.constraintCount==0);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && valid() && s->layout.objectStore.constraintCount==1);
    key(SDLK_ESCAPE);
    ld_test_shutdown_runtime(); return true;
}
static bool test_drag_group_history(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}), b=prism("B",(Vec3){1,0,0}); TEST_ASSERT(a && b);
    TEST_ASSERT(put(rule("ab","A","B",1))); Editor_ClearHistory(&s->editor);
    Layout_BeginGeometryGesture(&s->layout);
    TEST_ASSERT(!Layout_SetObject3DPosition(&s->layout,b,(Vec3){2,0,0},NULL));
    TEST_ASSERT(!Editor_UndoCount(&s->editor));
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,a,(Vec3){0,0,0},NULL));
    TEST_ASSERT(!Editor_UndoCount(&s->editor));
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,a,(Vec3){1,0,0},NULL));
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,a,(Vec3){2,0,0},NULL));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1 && valid());
    Layout_EndGeometryGesture(&s->layout);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && valid());
    TEST_ASSERT(s->layout.objectStore.items[0].transform.position.x==0 && s->layout.objectStore.items[1].transform.position.x==1);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout) && valid());
    TEST_ASSERT(s->layout.objectStore.items[1].transform.position.x==3);
    ld_test_shutdown_runtime(); return true;
}
static bool refuse_history(const Layout* layout, void* context) { (void)layout; (void)context; return false; }
static bool test_history_reservation_and_malformed_rules(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){2,0,0}));
    LayoutConstraint c=rule("","A","B",1);
    Editor_ClearHistory(&s->editor);
    char* before=Layout_SaveToString(&s->layout); s->layoutDirty=false; s->layoutDirtySinceSave=false;
    TEST_ASSERT(!Layout_ConstraintEdit(&s->layout,&c,NULL,refuse_history,NULL));
    c.target=NAN; TEST_ASSERT(!put(c));
    c=rule("bad","A","B",1); c.axis=(Vec3){0}; TEST_ASSERT(!put(c));
    c=rule("bad","A","missing",1); TEST_ASSERT(!put(c));
    c=rule("bad","A","B",1); memset(c.id,'x',sizeof(c.id)); TEST_ASSERT(!put(c));
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && !Editor_UndoCount(&s->editor));
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    free(before); free(after);
    TEST_ASSERT(put(rule("constraint_1","A","B",1)));
    TEST_ASSERT(prism("C",(Vec3){3,0,0})); TEST_ASSERT(put(rule("","B","C",1)));
    TEST_ASSERT(strcmp(s->layout.objectStore.constraints[1].id,"constraint_2")==0);
    ld_test_shutdown_runtime(); return true;
}
static bool test_mouse_drag_transaction_and_refusal(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}), b=prism("B",(Vec3){1,0,0}); TEST_ASSERT(a && b);
    TEST_ASSERT(put(rule("ab","A","B",1))); Editor_ClearHistory(&s->editor);
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D,false));
    s->freeViewCamera.enabled=true; s->freeViewCamera.yawDeg=35; s->freeViewCamera.pitchDeg=20;
    s->freeViewCamera.target=(Vec3){0};
    SDL_SetModState(KMOD_SHIFT);
    TEST_ASSERT(BeginObjectTranslateDragSession(s,&s->editor,a,GIZMO_AXIS_DIR_POS_X,200,200));
    draggingObjectTranslate=true;
    SDL_MouseMotionEvent motion={.x=300,.y=200}; HandleMouseDrag(&motion);
    motion.x=400; HandleMouseDrag(&motion);
    draggingObjectTranslate=false; ResetObjectTranslateDrag(&s->editor);
    TEST_ASSERT(valid() && Editor_UndoCount(&s->editor)==1 && s->layout.objectStore.items[0].transform.position.x!=0);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(valid() && s->layout.objectStore.items[0].transform.position.x==0);
    TEST_ASSERT(BeginObjectTranslateDragSession(s,&s->editor,b,GIZMO_AXIS_DIR_POS_X,200,200));
    draggingObjectTranslate=true; motion.x=400; HandleMouseDrag(&motion);
    draggingObjectTranslate=false; ResetObjectTranslateDrag(&s->editor); SDL_SetModState(KMOD_NONE);
    TEST_ASSERT(valid() && !Editor_UndoCount(&s->editor) && Editor_RedoCount(&s->editor)==1);
    TEST_ASSERT(s->layout.geometryMessage[0] && s->layout.objectStore.items[1].transform.position.x==1);
    ld_test_shutdown_runtime(); return true;
}
static bool test_new_object_preserves_imported_identity(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("obj3d_3",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){1,0,0}));
    TEST_ASSERT(put(rule("ab","obj3d_3","B",1)));
    uint32_t id=prism("C",(Vec3){2,0,0}); TEST_ASSERT(id==4);
    TEST_ASSERT(valid()); char* saved=Layout_SaveToString(&s->layout); TEST_ASSERT(saved); free(saved);
    ld_test_shutdown_runtime(); return true;
}
static bool test_canonical_import_keeps_rules_and_rejects_broken_snapshot(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){2,0,0}));
    LayoutConstraint offset_rule=rule("ab","A","B",0.5);
    offset_rule.a.local_offset_meters[1]=0.025;
    offset_rule.b.local_offset_meters[0]=-0.1;
    TEST_ASSERT(put(offset_rule));
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(&s->layout,"rule_import"); TEST_ASSERT(authored);
    char path[]="/tmp/ld_rule_import_XXXXXX"; int fd=mkstemp(path); TEST_ASSERT(fd>=0);
    FILE* file=fdopen(fd,"w"); TEST_ASSERT(file && fputs(authored,file)>=0 && fclose(file)==0);
    Layout imported; Layout_Init(&imported,1); char diagnostics[256];
    TEST_ASSERT(LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,path,diagnostics,sizeof(diagnostics)));
    TEST_ASSERT(imported.objectStore.constraintCount==1 && Layout_ValidateConstraints(&imported,NULL,0));
    TEST_ASSERT(imported.objectStore.constraints[0].a.local_offset_meters[1]==0.025);
    TEST_ASSERT(imported.objectStore.constraints[0].b.local_offset_meters[0]==-0.1);
    cJSON* root=cJSON_Parse(authored);
    cJSON* extension=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"extensions"),"line_drawing");
    TEST_ASSERT(cJSON_IsObject(extension));
    cJSON_ReplaceItemInObjectCaseSensitive(extension,"layout_snapshot",cJSON_CreateString("broken"));
    char* broken=cJSON_PrintUnformatted(root); TEST_ASSERT(broken);
    file=fopen(path,"w"); TEST_ASSERT(file && fputs(broken,file)>=0 && fclose(file)==0);
    char* before=Layout_SaveToString(&s->layout); size_t count=Editor_UndoCount(&s->editor);
    s->layoutDirty=false; s->layoutDirtySinceSave=false;
    TEST_ASSERT(!LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&s->layout,path,diagnostics,sizeof(diagnostics)));
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && Editor_UndoCount(&s->editor)==count);
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    free(before); free(after); free(authored); free(broken); cJSON_Delete(root); Layout_Free(&imported); unlink(path);
    ld_test_shutdown_runtime(); return true;
}
static bool test_persistent_precision_refusal(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){100000,0,0})); TEST_ASSERT(prism("B",(Vec3){100001,0,0}));
    Editor_ClearHistory(&s->editor); char* before=Layout_SaveToString(&s->layout);
    s->layoutDirty=false; s->layoutDirtySinceSave=false;
    TEST_ASSERT(!put(rule("precision","A","B",0.02)));
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(strcmp(before,after)==0 && !Editor_UndoCount(&s->editor));
    TEST_ASSERT(!s->layoutDirty && !s->layoutDirtySinceSave);
    free(before); free(after); ld_test_shutdown_runtime(); return true;
}
static bool test_object_asset_boundary_preserves_scene_rules(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0})); TEST_ASSERT(prism("B",(Vec3){1,0,0}));
    s->workspaceMode=LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    TEST_ASSERT(!put(rule("ab","A","B",1)) && !s->layout.objectStore.constraintCount);
    s->workspaceMode=LINE_DRAWING_WORKSPACE_MODE_SCENE;
    TEST_ASSERT(put(rule("ab","A","B",1)));
    char path[]="/tmp/ld_constraint_asset_XXXXXX"; int fd=mkstemp(path); TEST_ASSERT(fd>=0);
    FILE* file=fdopen(fd,"w"); TEST_ASSERT(file && fputs("keep original",file)>=0 && fclose(file)==0);
    char diagnostics[256];
    TEST_ASSERT(!LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(&s->layout,NULL,path,diagnostics,sizeof(diagnostics)));
    char* kept=ld_test_read_text_file(path); TEST_ASSERT(kept && !strcmp(kept,"keep original"));
    TEST_ASSERT(valid() && s->layout.objectStore.constraintCount==1);
    free(kept); unlink(path); ld_test_shutdown_runtime(); return true;
}

static bool test_offset_frame_physical_resize(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t id=prism("A",(Vec3){2,3,4}); TEST_ASSERT(id);
    s->layout.metersPerWorldUnit=0.01;
    LayoutGeometricReference a=ref("A",EDITOR_REFERENCE_AXIS_U,0);
    a.local_offset_meters[0]=0.25; a.local_offset_meters[1]=-0.03;
    LayoutResolvedReference before,after;
    TEST_ASSERT(Layout_ResolveReference(&s->layout,&a,&before)==LAYOUT_MEASUREMENT_OK);
    TEST_ASSERT(fabs(before.point_meters[0]-.27)<1e-8 && fabs(before.point_meters[1])<1e-8);
    bool adjusted=false;
    TEST_ASSERT(Layout_SetRectPrismDimensions(&s->layout,id,2,1,.4,&adjusted));
    TEST_ASSERT(Layout_ResolveReference(&s->layout,&a,&after)==LAYOUT_MEASUREMENT_OK);
    TEST_ASSERT(fabs(after.point_meters[0]-before.point_meters[0])<1e-8);
    Object3D baseline=s->layout.objectStore.items[0];
    TEST_ASSERT(Layout_RotateObject3D(&s->layout,id,(Vec3){0,0,1},90,&baseline,&adjusted));
    TEST_ASSERT(Layout_ResolveReference(&s->layout,&a,&after)==LAYOUT_MEASUREMENT_OK);
    TEST_ASSERT(fabs(after.point_meters[0]-.05)<1e-6 && fabs(after.point_meters[1]-.28)<1e-6);
    TEST_ASSERT(fabs(after.direction[1]-1)<1e-6);
    a.local_offset_meters[2]=NAN;
    TEST_ASSERT(Layout_ResolveReference(&s->layout,&a,&after)==LAYOUT_MEASUREMENT_INVALID);
    ld_test_shutdown_runtime(); return true;
}
static bool test_offset_mate_driver_undo_reopen(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    uint32_t a=prism("A",(Vec3){0}); TEST_ASSERT(a && prism("B",(Vec3){2,0,0}));
    LayoutConstraint c=rule("edge_mate","A","B",30);
    c.kind=LAYOUT_CONSTRAINT_PLANAR_MATE; c.axis=(Vec3){0,0,1};
    c.a.kind=c.b.kind=LAYOUT_REFERENCE_AXIS_U;
    c.a.local_offset_meters[0]=.5; c.b.local_offset_meters[0]=-.5;
    TEST_ASSERT(put(c) && valid());
    LayoutConstraintFeedback feedback=Layout_ConstraintFeedback(&s->layout,&c);
    TEST_ASSERT(feedback.satisfied && fabs(feedback.angle.value-30)<1e-4 && feedback.position.value<1e-6);
    Editor_ClearHistory(&s->editor);
    Object3D baseline=s->layout.objectStore.items[0]; bool adjusted=false;
    TEST_ASSERT(Layout_RotateObject3D(&s->layout,a,(Vec3){0,0,1},90,&baseline,&adjusted) && valid());
    LayoutResolvedReference pivot;
    TEST_ASSERT(Layout_ResolveReference(&s->layout,&c.b,&pivot)==LAYOUT_MEASUREMENT_OK);
    TEST_ASSERT(fabs(pivot.point_meters[0])<1e-6 && fabs(pivot.point_meters[1]-.5)<1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1 && Editor_Undo(&s->editor,&s->layout) && valid());
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout) && valid());
    char* json=Layout_SaveToString(&s->layout); Layout loaded; Layout_Init(&loaded,1);
    TEST_ASSERT(json && Layout_LoadFromString(&loaded,json));
    TEST_ASSERT(loaded.objectStore.constraints[0].a.local_offset_meters[0]==.5);
    TEST_ASSERT(Layout_ConstraintFeedback(&loaded,&loaded.objectStore.constraints[0]).satisfied);
    /* Failed operand update must preserve the exact saved scene and undo. */
    c.b.local_offset_meters[0]=INFINITY; size_t count=Editor_UndoCount(&s->editor);
    TEST_ASSERT(!put(c)); char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(after && !strcmp(json,after) && Editor_UndoCount(&s->editor)==count);
    free(json); free(after); Layout_Free(&loaded); ld_test_shutdown_runtime(); return true;
}
static bool test_offset_schema_legacy_and_atomic(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0}) && prism("B",(Vec3){1,0,0}) && put(rule("ab","A","B",1)));
    char* json=Layout_SaveToString(&s->layout); cJSON* root=cJSON_Parse(json); TEST_ASSERT(root);
    cJSON* c=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"geometricConstraints"),0);
    cJSON* a=cJSON_GetObjectItemCaseSensitive(c,"a");
    cJSON_DeleteItemFromObjectCaseSensitive(a,"offsetU_m");
    char* broken=cJSON_PrintUnformatted(root);
    TEST_ASSERT(!Layout_LoadFromString(&s->layout,broken));
    char* after=Layout_SaveToString(&s->layout); TEST_ASSERT(!strcmp(json,after));
    free(after); free(broken);
    /* Schema 11 without offsets retains its exact previous origin semantics. */
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion"),11);
    const char* fields[]={"offsetU_m","offsetV_m","offsetN_m"};
    for (int i=0;i<3;++i) {
        cJSON_DeleteItemFromObjectCaseSensitive(a,fields[i]);
        cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(c,"b"),fields[i]);
    }
    char* legacy=cJSON_PrintUnformatted(root);
    TEST_ASSERT(Layout_LoadFromString(&s->layout,legacy) && valid());
    TEST_ASSERT(s->layout.objectStore.constraints[0].a.local_offset_meters[0]==0);
    free(legacy); free(json); cJSON_Delete(root); ld_test_shutdown_runtime(); return true;
}
static bool test_offset_ui_staging_update_cancel(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0}) && prism("B",(Vec3){2,0,0}));
    TEST_ASSERT(put(rule("ab","A","B",1)));
    Editor_ClearHistory(&s->editor); UIPanel_BeginMeasurement(); UIPanel_MeasurementKey(SDLK_q);
    UIPanelState* ui=UIPanel_Get();
    UIPanel_MeasurementKey(SDLK_x); UIPanel_MeasurementText("25 mm"); UIPanel_MeasurementKey(SDLK_RETURN);
    TEST_ASSERT(!ui->measurement.placing && fabs(ui->measurement.refs[0].local_offset_meters[0]-.025)<1e-12);
    TEST_ASSERT(s->layout.objectStore.constraints[0].a.local_offset_meters[0]==0 && !Editor_UndoCount(&s->editor));
    UIPanel_MeasurementKey(SDLK_y); UIPanel_MeasurementText("2 cm"); UIPanel_MeasurementKey(SDLK_ESCAPE);
    TEST_ASSERT(ui->measurement.refs[0].local_offset_meters[1]==0);
    UIPanel_MeasurementKey(SDLK_r); UIPanel_MeasurementText("1 m"); UIPanel_MeasurementKey(SDLK_RETURN);
    TEST_ASSERT(!ui->measurement.placing && s->layout.objectStore.constraintCount==1 && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.constraints[0].a.local_offset_meters[0]-.025)<1e-12);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1 && Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(s->layout.objectStore.constraints[0].a.local_offset_meters[0]==0 && valid());
    UIPanel_MeasurementKey(SDLK_ESCAPE); ld_test_shutdown_runtime(); return true;
}
static bool test_constraint_feedback_readonly_conflict(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0}) && prism("B",(Vec3){1,0,0}));
    LayoutConstraint c=rule("ab","A","B",1);
    char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(Layout_ConstraintFeedback(&s->layout,&c).satisfied);
    c.target=2; TEST_ASSERT(!Layout_ConstraintFeedback(&s->layout,&c).satisfied);
    snprintf(c.b.entity_id,sizeof(c.b.entity_id),"missing");
    TEST_ASSERT(Layout_ConstraintFeedback(&s->layout,&c).position.status==LAYOUT_MEASUREMENT_UNRESOLVED);
    char* after=Layout_SaveToString(&s->layout); TEST_ASSERT(!strcmp(before,after));
    free(before); free(after); ld_test_shutdown_runtime(); return true;
}

static bool click_measure(int action) {
    UIPanelState* ui=UIPanel_Get(); SDL_Rect rect;
    UIPanel_LayoutMeasurementPane();
    TEST_ASSERT(UIPanel_MeasurementControlRect(action,&rect));
    if(rect.y<ui->rightBodyRect.y || rect.y+rect.h>ui->rightBodyRect.y+ui->rightBodyRect.h) {
        ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx+=(float)(rect.y-ui->rightBodyRect.y-20);
        UIPanel_LayoutMeasurementPane();
        TEST_ASSERT(UIPanel_MeasurementControlRect(action,&rect));
    }
    TEST_ASSERT(rect.y>=ui->rightBodyRect.y && rect.y+rect.h<=ui->rightBodyRect.y+ui->rightBodyRect.h);
    AppContext context={0}; SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};
    e.button.button=SDL_BUTTON_LEFT;e.button.x=rect.x+rect.w/2;e.button.y=rect.y+rect.h/2;
    Input_Handle(&context,&e);
    e.type=SDL_MOUSEBUTTONUP;Input_Handle(&context,&e);
    return true;
}
static void type_measure(const char* text) {
    AppContext context={0};SDL_Event e={.type=SDL_TEXTINPUT};snprintf(e.text.text,sizeof(e.text.text),"%s",text);Input_Handle(&context,&e);
}
static bool test_measure_pane_mouse_workflow(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    uint32_t id=prism("A",(Vec3){0});TEST_ASSERT(id && prism("B",(Vec3){2,0,0}));
    s->editor.selectedObject3DId=id;Editor_ClearHistory(&s->editor);UIPanel_BeginMeasurement();
    UIPanelState* ui=UIPanel_Get();TEST_ASSERT(!UIPanel_IsCapturingKeyboard());
    TEST_ASSERT(click_measure(MEASURE_OBJECT_B));TEST_ASSERT(ui->measurement.chooser==2);
    TEST_ASSERT(click_measure(MEASURE_CHOICE_BASE+1));TEST_ASSERT(!strcmp(ui->measurement.refs[1].entity_id,"B"));
    TEST_ASSERT(click_measure(MEASURE_ADVANCED));TEST_ASSERT(click_measure(MEASURE_FEATURE_A));TEST_ASSERT(click_measure(MEASURE_CHOICE_BASE+1));
    TEST_ASSERT(ui->measurement.refs[0].kind==LAYOUT_REFERENCE_AXIS_U);
    TEST_ASSERT(click_measure(MEASURE_VALUE));type_measure("20 mm");
    TEST_ASSERT(!Editor_UndoCount(&s->editor) && !s->layout.objectStore.constraintCount);
    TEST_ASSERT(click_measure(MEASURE_ONCE));TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(near_measure(&s->layout,ui->measurement.refs[0],ui->measurement.refs[1],EDITOR_MEASURE_PROJECTED_DISTANCE,(Vec3){1,0,0},.02));
    TEST_ASSERT(click_measure(MEASURE_SAVE));TEST_ASSERT(s->layout.objectStore.constraintCount==1 && valid());
    TEST_ASSERT(click_measure(MEASURE_SAVED));TEST_ASSERT(click_measure(MEASURE_RULES));TEST_ASSERT(click_measure(MEASURE_CHOICE_BASE));
    TEST_ASSERT(ui->measurement.constraint_index==0);
    TEST_ASSERT(click_measure(MEASURE_VALUE));type_measure("50 mm");
    TEST_ASSERT(click_measure(MEASURE_SAVE));TEST_ASSERT(valid() && s->layout.objectStore.constraints[0].target==.05);
    TEST_ASSERT(click_measure(MEASURE_SAVED));TEST_ASSERT(click_measure(MEASURE_RULES));TEST_ASSERT(click_measure(MEASURE_CHOICE_BASE));
    TEST_ASSERT(click_measure(MEASURE_SAVED));TEST_ASSERT(click_measure(MEASURE_REMOVE));TEST_ASSERT(s->layout.objectStore.constraintCount==1);
    TEST_ASSERT(click_measure(MEASURE_CANCEL));TEST_ASSERT(!ui->measurement.placing);
    TEST_ASSERT(click_measure(MEASURE_REMOVE));TEST_ASSERT(click_measure(MEASURE_SAVE));TEST_ASSERT(!s->layout.objectStore.constraintCount);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && valid() && s->layout.objectStore.constraintCount==1);
    TEST_ASSERT(click_measure(MEASURE_OFFSETS));TEST_ASSERT(click_measure(MEASURE_OFFSET_U));type_measure("25 mm");
    TEST_ASSERT(click_measure(MEASURE_APPLY_OFFSET));TEST_ASSERT(ui->measurement.refs[0].local_offset_meters[0]==.025);
    TEST_ASSERT(!ui->measurement.placing);
    TEST_ASSERT(click_measure(MEASURE_RESET_OFFSET));TEST_ASSERT(ui->measurement.refs[0].local_offset_meters[0]==0);
    UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_CREATE);ld_test_shutdown_runtime();return true;
}
static bool test_measure_pane_tabs_scroll_focus(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    UIPanel_BeginMeasurement();
    TEST_ASSERT(click_measure(MEASURE_FILE));
    TEST_ASSERT(UIPanel_Get()->activeLeftTab==UI_PANEL_LEFT_TAB_FILE);
    TEST_ASSERT(click_measure(MEASURE_CREATE));
    TEST_ASSERT(UIPanel_Get()->activeRightTab==UI_PANEL_RIGHT_TAB_CREATE && !UIPanel_Get()->measurement.active);
    TEST_ASSERT(s->layout.objectStore.count==0);
    TEST_ASSERT(prism("A",(Vec3){0}) && prism("B",(Vec3){1,0,0}));
    UIPanel_BeginMeasurement();UIPanelState* ui=UIPanel_Get();
    LayoutGeometricReference a=ui->measurement.refs[0];
    TEST_ASSERT(click_measure(MEASURE_VALUE));type_measure("15 mm");TEST_ASSERT(UIPanel_IsCapturingKeyboard());
    size_t undo=Editor_UndoCount(&s->editor);
    AppContext ctx={0};SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=SDLK_RETURN;Input_Handle(&ctx,&e);
    TEST_ASSERT(!UIPanel_IsCapturingKeyboard() && Editor_UndoCount(&s->editor)==undo);
    TEST_ASSERT(!s->layout.objectStore.constraintCount);
    TEST_ASSERT(click_measure(MEASURE_ADVANCED) && click_measure(MEASURE_OFFSETS));
    TEST_ASSERT(UIPanel_RightScrollHandleWheel(ui->rightBodyRect.x+20,ui->rightBodyRect.y+20,-2));
    TEST_ASSERT(UIPanel_RightScrollOffset(ui)>0);
    TEST_ASSERT(click_measure(MEASURE_PICK_B) && ui->measurement.picking);
    SDL_Rect tab=ui->rightTabs[UI_PANEL_RIGHT_TAB_CREATE].bounds;
    e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=tab.x+tab.w/2;e.button.y=tab.y+tab.h/2;Input_Handle(&ctx,&e);
    TEST_ASSERT(ui->activeRightTab==UI_PANEL_RIGHT_TAB_CREATE && !ui->measurement.active && !ui->measurement.picking);
    tab=ui->rightTabs[UI_PANEL_RIGHT_TAB_MEASURE].bounds;e.button.x=tab.x+tab.w/2;e.button.y=tab.y+tab.h/2;Input_Handle(&ctx,&e);
    TEST_ASSERT(ui->activeRightTab==UI_PANEL_RIGHT_TAB_MEASURE && ui->measurement.active);
    TEST_ASSERT(!memcmp(&a,&ui->measurement.refs[0],sizeof(a)));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==undo);
    UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_CREATE);ld_test_shutdown_runtime();return true;
}
static bool test_travel_chain_direct_edits(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    uint32_t a=prism("floor",(Vec3){0}),b=prism("bed",(Vec3){.2f,.3f,.9f});
    TEST_ASSERT(a && b && prism("sensor",(Vec3){.2f,.3f,1.1f}));
    LayoutConstraint follower=rule("sensor_mount","bed","sensor",.2);follower.axis=(Vec3){0,0,1};TEST_ASSERT(put(follower));
    LayoutConstraint c=rule("bed_lift","floor","bed",0);c.axis=(Vec3){0,0,1};
    TEST_ASSERT(Layout_InitLinearTravel(&s->layout,&c,.9,1.8) && put(c) && valid());
    TEST_ASSERT(c.travel_home==.9 && fabs(c.travel_offset[0]-.2)<1e-6);
    Editor_ClearHistory(&s->editor);
    TEST_ASSERT(Layout_SetTravelPosition(&s->layout,c.id,1.8,Layout_GeometryHistory,NULL) && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.items[2].transform.position.z-2)<1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1 && Editor_Undo(&s->editor,&s->layout) && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.items[1].transform.position.z-.9)<1e-6);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout) && valid());
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,b,(Vec3){.2f,.3f,1.2f},NULL) && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.constraints[1].target-1.2)<1e-6);
    char* before=Layout_SaveToString(&s->layout);size_t undo=Editor_UndoCount(&s->editor);
    TEST_ASSERT(!Layout_SetObject3DPosition(&s->layout,b,(Vec3){.2f,.3f,2},NULL));
    TEST_ASSERT(!Layout_SetObject3DPosition(&s->layout,b,(Vec3){.4f,.3f,1.2f},NULL));
    Object3D baseline=s->layout.objectStore.items[1];
    TEST_ASSERT(!Layout_RotateObject3D(&s->layout,b,(Vec3){0,0,1},30,&baseline,NULL));
    TEST_ASSERT(!Layout_SetTravelPosition(&s->layout,c.id,.8,Layout_GeometryHistory,NULL));
    TEST_ASSERT(!Layout_SetTravelPosition(&s->layout,c.id,NAN,Layout_GeometryHistory,NULL));
    char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(!strcmp(before,after) && Editor_UndoCount(&s->editor)==undo);free(before);free(after);
    TEST_ASSERT(Layout_SetObject3DPosition(&s->layout,a,(Vec3){1,1,.1f},NULL) && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.items[1].transform.position.x-1.2)<1e-6);
    TEST_ASSERT(fabs(s->layout.objectStore.items[2].transform.position.z-1.5)<1e-6);
    ld_test_shutdown_runtime();return true;
}
static bool test_travel_contract_rollback(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0}) && prism("B",(Vec3){0,0,1}) && prism("C",(Vec3){0,0,1.2f}));
    LayoutConstraint c=rule("travel","A","B",0);c.axis=(Vec3){0,0,1};
    TEST_ASSERT(!Layout_InitLinearTravel(&s->layout,&c,2,1));
    TEST_ASSERT(!Layout_InitLinearTravel(&s->layout,&c,NAN,2));
    TEST_ASSERT(Layout_InitLinearTravel(&s->layout,&c,.5,2) && put(c));
    LayoutConstraint follower=rule("child","B","C",.2);follower.axis=c.axis;TEST_ASSERT(put(follower));
    LayoutConstraint bad=c;bad.travel_min=1.1;TEST_ASSERT(!put(bad));
    bad=c;bad.travel_min=-1e308;bad.travel_max=1e308;TEST_ASSERT(!put(bad));
    bad=c;bad.travel_offset[2]=1;TEST_ASSERT(!put(bad));
    bad=c;bad.travel_basis[0][0]=0;TEST_ASSERT(!put(bad));
    s->layout.objectStore.items[2].coreMeta.flags.locked=true;
    Editor_ClearHistory(&s->editor);char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(!Layout_SetTravelPosition(&s->layout,c.id,1.5,Layout_GeometryHistory,NULL));
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(!strcmp(before,after) && !Editor_UndoCount(&s->editor));free(before);free(after);
    s->layout.objectStore.items[2].coreMeta.flags.locked=false;
    s->layout.scene3d.bounds=(SceneBounds3D){.enabled=true,.clampOnEdit=true,.min={-2,-2,-2},.max={2,2,1.6f}};
    s->layout.objectStore.items[2].rectPrism.lockToBounds=true;
    before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(!Layout_SetTravelPosition(&s->layout,c.id,1.7,Layout_GeometryHistory,NULL));
    after=Layout_SaveToString(&s->layout);TEST_ASSERT(!strcmp(before,after) && !Editor_UndoCount(&s->editor));free(before);free(after);
    TEST_ASSERT(Layout_SetTravelPosition(&s->layout,c.id,1.1,Layout_GeometryHistory,NULL) && valid());
    ld_test_shutdown_runtime();return true;
}
static bool test_travel_persistence_units_export(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    TEST_ASSERT(prism("A",(Vec3){0}) && prism("B",(Vec3){20,0,100}));s->layout.metersPerWorldUnit=.01;
    LayoutConstraint c=rule("rail","A","B",0);c.axis=(Vec3){0,0,2};
    TEST_ASSERT(Layout_InitLinearTravel(&s->layout,&c,.9,1.8) && put(c));
    TEST_ASSERT(Layout_SetTravelPosition(&s->layout,c.id,1.8,Layout_GeometryHistory,NULL));
    char* json=Layout_SaveToString(&s->layout);TEST_ASSERT(json);
    Layout loaded;Layout_Init(&loaded,1);TEST_ASSERT(Layout_LoadFromString(&loaded,json));
    TEST_ASSERT(loaded.objectStore.constraints[0].travel_home==1 && Layout_ValidateConstraints(&loaded,NULL,0));
    TEST_ASSERT(Layout_SetTravelPosition(&loaded,c.id,1,NULL,NULL) && Layout_ValidateConstraints(&loaded,NULL,0));
    TEST_ASSERT(fabs(loaded.objectStore.items[1].transform.position.z-100)<1e-6);
    for(int mode=0;mode<4;++mode) {
        cJSON* root=cJSON_Parse(json);cJSON* rule_json=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"geometricConstraints"),0);
        if(mode==0)cJSON_DeleteItemFromObjectCaseSensitive(rule_json,"travelHome_m");
        if(mode==1)cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(rule_json,"target"),1.3);
        if(mode==2)cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion"),12);
        if(mode==3)cJSON_SetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(rule_json,"travelBasis"),0),0);
        char* invalid=cJSON_PrintUnformatted(root);char* before=Layout_SaveToString(&loaded);
        TEST_ASSERT(!Layout_LoadFromString(&loaded,invalid));char* after=Layout_SaveToString(&loaded);
        TEST_ASSERT(!strcmp(before,after));free(before);free(after);free(invalid);cJSON_Delete(root);
    }
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(&s->layout,"bed_lift_fixture");char* runtime=NULL;char diagnostic[256];
    TEST_ASSERT(authored && strstr(authored,"travelHome_m") && core_scene_compile_authoring_to_runtime(authored,&runtime,diagnostic,sizeof(diagnostic)).code==CORE_OK);
    free(authored);free(runtime);free(json);Layout_Free(&loaded);ld_test_shutdown_runtime();return true;
}
static bool test_travel_mouse_controls_and_slider(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    uint32_t a=prism("floor",(Vec3){0});TEST_ASSERT(a && prism("bed",(Vec3){0,0,.9f}));
    s->editor.selectedObject3DId=a;UIPanel_BeginMeasurement();UIPanelState* ui=UIPanel_Get();
    TEST_ASSERT(click_measure(MEASURE_AXIS_Z) && click_measure(MEASURE_TOOL) && click_measure(MEASURE_TRAVEL));
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_MIN));type_measure("900 mm");
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_MAX));type_measure("1.8 m");
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_POSITION));type_measure("120 cm");
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_SAVE) && valid() && s->layout.objectStore.constraintCount==1);
    TEST_ASSERT(s->layout.objectStore.constraints[0].target==1.2);
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_TO_MAX) && s->layout.objectStore.constraints[0].target==1.8);
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_RESET) && s->layout.objectStore.constraints[0].target==.9);
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_TO_MIN));
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_POSITION));type_measure("2 m");
    size_t undo=Editor_UndoCount(&s->editor);
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_SAVE) && s->layout.objectStore.constraints[0].target==.9 && Editor_UndoCount(&s->editor)==undo);
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_RESET));
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_SLIDER));
    double before_drag=s->layout.objectStore.constraints[0].target;
    SDL_Rect rect;TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&rect));
    Editor_ClearHistory(&s->editor);AppContext context={0};SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};
    e.button.button=SDL_BUTTON_LEFT;e.button.x=rect.x+6;e.button.y=rect.y+rect.h/2;Input_Handle(&context,&e);
    TEST_ASSERT(ui->measurement.travel_dragging);
    e.type=SDL_MOUSEMOTION;e.motion.x=rect.x+rect.w/3;Input_Handle(&context,&e);
    e.motion.x=rect.x+rect.w+100;Input_Handle(&context,&e);
    TEST_ASSERT(s->layout.objectStore.constraints[0].target==1.8 && valid());
    e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;Input_Handle(&context,&e);
    TEST_ASSERT(!ui->measurement.travel_dragging && !s->layout.geometryGestureActive && Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && valid());
    TEST_ASSERT(fabs(s->layout.objectStore.constraints[0].target-before_drag)<1e-6);
    TEST_ASSERT(click_measure(MEASURE_SAVED) && click_measure(MEASURE_RULES) && click_measure(MEASURE_CHOICE_BASE));
    TEST_ASSERT(ui->measurement.operation==3);
    TEST_ASSERT(click_measure(MEASURE_SAVED) && click_measure(MEASURE_REMOVE) && click_measure(MEASURE_SAVE));
    TEST_ASSERT(!s->layout.objectStore.constraintCount && Editor_Undo(&s->editor,&s->layout) && valid());
    UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_CREATE);ld_test_shutdown_runtime();return true;
}

static bool test_measure_task_surface_and_units(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    Global_SetWindowSize(1200,900);UIPanel_OnWindowResized(1200,900);
    uint32_t a=prism("rail",(Vec3){0});TEST_ASSERT(a && prism("moving",(Vec3){0,0,.9f}));
    s->editor.selectedObject3DId=a;UIPanel_BeginMeasurement();UIPanelState* ui=UIPanel_Get();
    UIPanel_SetDisplayUnit(CORE_UNIT_FOOT);Editor_ClearHistory(&s->editor);
    TEST_ASSERT(click_measure(MEASURE_AXIS_Z) && click_measure(MEASURE_TOOL) && click_measure(MEASURE_TRAVEL));
    SDL_Rect slider,primary,reset,hidden;
    TEST_ASSERT(!UIPanel_MeasurementControlRect(MEASURE_PLANE_XY,&hidden));
    TEST_ASSERT(!UIPanel_MeasurementControlRect(MEASURE_OFFSETS,&hidden));
    TEST_ASSERT(!UIPanel_MeasurementControlRect(MEASURE_RULES,&hidden));
    TEST_ASSERT(!UIPanel_MeasurementControlRect(MEASURE_DISTANCE,&hidden));
    TEST_ASSERT(strstr(ui->measurement.travel_text[0]," ft"));
    TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&slider));
    TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SAVE,&primary));
    TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_TRAVEL_RESET,&reset));
    TEST_ASSERT(primary.y>=ui->rightBodyRect.y && reset.y+reset.h<=ui->rightBodyRect.y+ui->rightBodyRect.h);
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_SLIDER) && click_measure(MEASURE_TRAVEL_TO_MAX));
    TEST_ASSERT(!ui->measurement.travel_dragging && !s->layout.objectStore.constraintCount && !Editor_UndoCount(&s->editor));
    TEST_ASSERT(click_measure(MEASURE_UNITS) && click_measure(MEASURE_CHOICE_BASE));
    double meters;TEST_ASSERT(Editor_ParseLength(ui->measurement.travel_text[0],UIPanel_GetDisplayUnit(),&meters));
    TEST_ASSERT(UIPanel_GetDisplayUnit()==CORE_UNIT_MILLIMETER && fabs(meters-.9)<1e-6 && strstr(ui->measurement.travel_text[0]," mm"));
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_MIN));type_measure("2000 mm");
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_MAX));type_measure("1000 mm");
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_SAVE));
    TEST_ASSERT(!s->layout.objectStore.constraintCount && !Editor_UndoCount(&s->editor));
    TEST_ASSERT(strstr(ui->measurement.placement_message,"Min must"));
    TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&hidden));
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_MIN));type_measure("900 mm");
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_MAX));type_measure("1800 mm");
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_SAVE) && s->layout.objectStore.constraintCount==1 && valid());
    TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&hidden));
    TEST_ASSERT(abs(hidden.y-slider.y)<=2*(slider.h+5));
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_TO_MAX) && s->layout.objectStore.constraints[0].target==1.8);
    TEST_ASSERT(click_measure(MEASURE_TRAVEL_RESET) && fabs(s->layout.objectStore.constraints[0].target-.9)<1e-6);
    char* saved=Layout_SaveToString(&s->layout);TEST_ASSERT(saved && Layout_LoadFromString(&s->layout,saved));free(saved);
    UIPanel_BeginMeasurement();TEST_ASSERT(click_measure(MEASURE_SAVED) && click_measure(MEASURE_RULES) && click_measure(MEASURE_CHOICE_BASE));
    TEST_ASSERT(ui->measurement.operation==3 && !ui->measurement.rules_open && valid());
    TEST_ASSERT(click_measure(MEASURE_TOOL) && click_measure(MEASURE_ANGLE));
    TEST_ASSERT(ui->measurement.operation==2 && ui->measurement.constraint_index==-1);
    TEST_ASSERT(UIPanel_MeasurementControlRect(MEASURE_PLANE_XY,&hidden) && !UIPanel_MeasurementControlRect(MEASURE_TRAVEL_SLIDER,&hidden));
    TEST_ASSERT(s->layout.objectStore.constraintCount==1 && valid());
    UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_CREATE);ld_test_shutdown_runtime();return true;
}

bool constraints_run_tests(void) {
    const TestCase cases[]={
        {"measure_task_surface_units",test_measure_task_surface_and_units},
        {"travel_chain_direct_edits",test_travel_chain_direct_edits},
        {"travel_contract_rollback",test_travel_contract_rollback},
        {"travel_persistence_units_export",test_travel_persistence_units_export},
        {"travel_mouse_controls_slider",test_travel_mouse_controls_and_slider},
        {"measure_pane_mouse_workflow",test_measure_pane_mouse_workflow},
        {"measure_pane_tabs_scroll_focus",test_measure_pane_tabs_scroll_focus},
        {"offset_physical_frame_resize",test_offset_frame_physical_resize},
        {"offset_mate_driver_undo_reopen",test_offset_mate_driver_undo_reopen},
        {"offset_schema_legacy_atomic",test_offset_schema_legacy_and_atomic},
        {"offset_ui_staging_update_cancel",test_offset_ui_staging_update_cancel},
        {"feedback_readonly_conflict",test_constraint_feedback_readonly_conflict},
        {"object_asset_boundary",test_object_asset_boundary_preserves_scene_rules},
        {"canonical_import_rules_and_atomic_failure",test_canonical_import_keeps_rules_and_rejects_broken_snapshot},
        {"persistent_precision_refusal",test_persistent_precision_refusal},
        {"mouse_drag_transaction_refusal",test_mouse_drag_transaction_and_refusal},
        {"new_object_stable_identity",test_new_object_preserves_imported_identity},
        {"drag_group_history",test_drag_group_history},
        {"history_reservation_malformed_rules",test_history_reservation_and_malformed_rules},
        {"driver_target_chain_undo",test_driver_target_chain_and_undo},
        {"atomic_cycle_dependent_delete_redo",test_atomic_conflicts_cycles_and_redo},
        {"dependent_bounds_locks",test_dependent_bounds_and_lock_rollback},
        {"resize_rotate_scale_routes",test_resize_rotate_scale_routes},
        {"plane_resize_routes",test_plane_resize_routes},
        {"persistence_invalid_import_export",test_save_reopen_invalid_import_and_export},
        {"remove_undo_coincident_pivot",test_remove_undo_and_coincident_pivot},
        {"planar_mate_angle_driver",test_planar_mate_rotation_target_and_failure},
        {"rule_ui_create_update_remove",test_rule_ui_create_select_update_remove}
    };
    return run_test_cases("Constraints",cases,sizeof(cases)/sizeof(cases[0]));
}
