#include "test_layout_internal.h"
#include "Layout/layout_furniture.h"
#include "UI/ui_panel_furniture.h"
#include "Input/input_handler.h"
#include "Input/input_mouse.h"
#include "Editor/editor_numeric_edit.h"

static bool setup(GlobalState* s, int sign, double scale) {
    UIPanel_FurnitureReset();
    s->layout.metersPerWorldUnit = scale;
    LayoutAssembly a = {.frame = {.axisU = {1,0,0}, .axisV = {0,1,0}, .normal = {0,0,1}}};
    snprintf(a.info.entity_type, sizeof(a.info.entity_type), "Assembly");
    snprintf(a.info.label, sizeof(a.info.label), "Test cabinet");
    if (!Layout_EditAssembly(&s->layout, &a, NULL, NULL, NULL)) return false;
    a = s->layout.objectStore.assemblies[0];
    LayoutFurnitureUnit u = {.size_m = {.5, 1, .9}, .center_m = {.25,.5,.45},
        .thickness_m = {.018,.018,.018,.04}, .overhang_m = {.02,.03,.02,.02},
        .shelf_fraction = .5, .back_sign = sign, .part_count = 7};
    snprintf(u.assembly_id, sizeof(u.assembly_id), "%s", a.id);
    for (int i = 0; i < 7; ++i) {
        RectPrismPrimitiveCreateParams p = {.width=1,.height=1,.depth=.018f,
            .useExplicitFrame=true,.explicitFrame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
        uint32_t id;
        if (!Layout_CreateRectPrismPrimitive(&s->layout,&p,&id,NULL)) return false;
        Object3D* o = Layout_ObjectStore_Find(&s->layout.objectStore,id);
        snprintf(o->info.parent_id,64,"%s",a.id);
        snprintf(o->info.entity_type,64,"%s", i==6?"PhysicalObject":"Panel");
        snprintf(u.parts[i].entity_id,64,"%s",o->coreMeta.object_id);
        u.parts[i].role = i < 5 ? (LayoutFurnitureRole)i : i==5 ? LAYOUT_FURNITURE_WORKTOP : LAYOUT_FURNITURE_FIXTURE;
        if (i==6) {
            memcpy(u.parts[i].span_m,(double[]){.36,.30,.20},sizeof(u.parts[i].span_m));
            memcpy(u.parts[i].offset_m,(double[]){.25,.4,-.08},sizeof(u.parts[i].offset_m));
        }
    }
    s->layout.objectStore.furniture[0]=u; s->layout.objectStore.furniture_count=1;
    for (size_t i=0;i<u.part_count;++i) {
        Object3D expected;
        if (!Layout_FurniturePartGeometry(&s->layout,&u,i,&expected)) return false;
        *Layout_ObjectStore_Find(&s->layout.objectStore,expected.objectId)=expected;
    }
    Editor_ClearHistory(&s->editor);
    return Layout_ValidateFurniture(&s->layout,NULL,0);
}
static bool deny(const Layout* l, void* c) { (void)l; (void)c; return false; }
static bool unchanged(const Layout* l, const char* before) {
    char* after=Layout_SaveToString(l); bool ok=after && !strcmp(before,after); Layout_FreeString(after); return ok;
}
static bool test_resize_fixed_thickness_identity_anchor_history(void) {
    for (int sign=-1;sign<=1;sign+=2) {
        ld_test_init_runtime(); GlobalState* s=Global_Get(); TEST_ASSERT(setup(s,sign,.1));
        LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];
        double wall=u.center_m[0]+sign*u.size_m[0]/2, floor=u.center_m[2]-u.size_m[2]/2;
        Object3D sink=s->layout.objectStore.items[6];
        u.size_m[0]=.60;u.size_m[1]=1.2;u.size_m[2]=1;u.thickness_m[0]=.024;
        TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor));
        const LayoutFurnitureUnit* now=&s->layout.objectStore.furniture[0];
        TEST_ASSERT(fabs(now->center_m[0]+sign*now->size_m[0]/2-wall)<1e-9);
        TEST_ASSERT(fabs(now->center_m[2]-now->size_m[2]/2-floor)<1e-9);
        TEST_ASSERT(fabs(now->center_m[1]+now->size_m[1]/2-1)<1e-9);
        TEST_ASSERT(fabs(s->layout.objectStore.items[3].rectPrism.depth*.1-.024)<1e-7);
        TEST_ASSERT(sink.rectPrism.width==s->layout.objectStore.items[6].rectPrism.width);
        for (int i=0;i<7;++i) TEST_ASSERT(!strcmp(now->parts[i].entity_id,u.parts[i].entity_id));
        TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
        TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,now,1,Editor_ReserveGeometryHistory,&s->editor));
        TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
        TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
        TEST_ASSERT(fabs(s->layout.objectStore.furniture[0].size_m[1]-1)<1e-9);
        TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));
        Layout reopened;Layout_Init(&reopened,.05f);char* text=Layout_SaveToString(&s->layout);
        TEST_ASSERT(text && Layout_LoadFromString(&reopened,text));
        TEST_ASSERT(Layout_ValidateFurniture(&reopened,NULL,0));
        TEST_ASSERT(!memcmp(&reopened.objectStore.furniture[0],now,sizeof(*now)));
        Layout_FreeString(text);Layout_Free(&reopened);ld_test_shutdown_runtime();
    }
    return true;
}
static bool test_failures_are_atomic_and_parts_cannot_drift(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(setup(s,1,1));
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];u.size_m[1]=1.1;
    TEST_ASSERT(!Layout_EditFurnitureUnit(&s->layout,&u,1,deny,NULL) && unchanged(&s->layout,before));
    u.size_m[1]=.2;TEST_ASSERT(!Layout_EditFurnitureUnit(&s->layout,&u,1,NULL,NULL) && unchanged(&s->layout,before));
    bool adjusted=false;Object3D* part=&s->layout.objectStore.items[0];
    TEST_ASSERT(!Layout_SetRectPrismDimensions(&s->layout,part->objectId,.030f,1,.9f,&adjusted));
    TEST_ASSERT(unchanged(&s->layout,before));
    TEST_ASSERT(!Layout_CanDeleteObject(&s->layout.objectStore,part->objectId));
    s->layout.objectStore.items[1].coreMeta.flags.locked=true;Layout_FreeString(before);before=Layout_SaveToString(&s->layout);
    u.size_m[1]=1.1;TEST_ASSERT(!Layout_EditFurnitureUnit(&s->layout,&u,1,NULL,NULL) && unchanged(&s->layout,before));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0);
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_rotated_unit_and_strict_recipe_loading(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(setup(s,-1,1));
    TEST_ASSERT(Layout_MoveAssembly(&s->layout,"assembly_1",(double[]){1,2,3},(Vec3){0,0,30},NULL,NULL));
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];u.size_m[1]=1.1;
    TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,-1,NULL,NULL));
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    cJSON* root=cJSON_Parse(before);TEST_ASSERT(root);
    cJSON_SetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(root,"file"),"schemaVersion"),21);
    char* downgraded=cJSON_Print(root);
    TEST_ASSERT(!Layout_LoadFromString(&s->layout,downgraded) && unchanged(&s->layout,before));
    free(downgraded);
    cJSON_SetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(root,"file"),"schemaVersion"),22);
    cJSON* unit=cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetObjectItem(root,"engineering"),"furnitureUnits"),0);
    cJSON_SetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(unit,"sizeMeters"),1),9);
    char* drift=cJSON_Print(root);TEST_ASSERT(!Layout_LoadFromString(&s->layout,drift) && unchanged(&s->layout,before));
    free(drift);cJSON_Delete(root);Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool click(int action) {
    SDL_Rect r; if (!UIPanel_FurnitureControlRect(action,&r)) return false;
    SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;
    e.button.x=r.x+r.w*3/4;e.button.y=r.y+r.h/2;Input_Handle(NULL,&e);return true;
}
static bool test_visible_form_apply_cancel_resize_and_history(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(setup(s,1,1));
    s->editor.selectedObject3DId=s->layout.objectStore.items[0].objectId;
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_OBJECT);UIPanel_OnWindowResized(1400,1000);
    TEST_ASSERT(click(UI_FURNITURE_OPEN) && UIPanel_FurnitureEditing());
    SDL_Rect old,wide;TEST_ASSERT(UIPanel_FurnitureControlRect(UI_FURNITURE_LENGTH,&old));
    TEST_ASSERT(click(UI_FURNITURE_LENGTH));
    SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,sizeof(text.text.text),"1.1 m");Input_Handle(NULL,&text);
    TEST_ASSERT(click(UI_FURNITURE_APPLY));
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[0].size_m[1]-1.1)<1e-9);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(click(UI_FURNITURE_PANELS));
    TEST_ASSERT(UIPanel_FurnitureControlRect(UI_FURNITURE_CASE,&wide));
    TEST_ASSERT(!UIPanel_FurnitureControlRect(UI_FURNITURE_DOOR,&wide));
    TEST_ASSERT(UIPanel_FurnitureControlRect(UI_FURNITURE_WORKTOP,&wide));
    TEST_ASSERT(click(UI_FURNITURE_CASE));snprintf(text.text.text,sizeof(text.text.text),"25 mm");Input_Handle(NULL,&text);
    TEST_ASSERT(click(UI_FURNITURE_CANCEL));
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[0].thickness_m[0]-.018)<1e-9);
    TEST_ASSERT(click(UI_FURNITURE_OPEN));
    /* Widen the actual pane through its splitter, not just the window. */
    CorePaneRect pane = {0};
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&s->paneHost,
        LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &pane));
    SDL_Event drag = {.type=SDL_MOUSEBUTTONDOWN};drag.button.button=SDL_BUTTON_LEFT;
    drag.button.x=(int)pane.x;drag.button.y=(int)(pane.y+pane.height*.5f);
    Input_MouseHandle(NULL,&drag);
    TEST_ASSERT(LineDrawingPaneHost_IsSplitterDragActive(&s->paneHost));
    drag=(SDL_Event){.type=SDL_MOUSEMOTION};drag.motion.state=SDL_BUTTON_LMASK;
    drag.motion.x=(int)pane.x-120;drag.motion.y=(int)(pane.y+pane.height*.5f);
    Input_MouseHandle(NULL,&drag);
    drag=(SDL_Event){.type=SDL_MOUSEBUTTONUP};drag.button.button=SDL_BUTTON_LEFT;
    drag.button.x=(int)pane.x-120;drag.button.y=(int)(pane.y+pane.height*.5f);
    Input_MouseHandle(NULL,&drag);
    TEST_ASSERT(UIPanel_FurnitureControlRect(UI_FURNITURE_LENGTH,&wide));
    TEST_ASSERT(wide.w>old.w);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));UIPanel_OnWindowResized(1800,1000);
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[0].size_m[1]-1)<1e-9);
    ld_test_shutdown_runtime();return true;
}
bool furniture_run_tests(void) {
    const TestCase cases[] = {
        {"resize_anchor_thickness_ids_history", test_resize_fixed_thickness_identity_anchor_history},
        {"atomic_failure_managed_parts", test_failures_are_atomic_and_parts_cannot_drift},
        {"rotation_strict_loading", test_rotated_unit_and_strict_recipe_loading},
        {"native_form_apply_cancel_resize", test_visible_form_apply_cancel_resize_and_history}};
    return run_test_cases("Furniture", cases, sizeof(cases) / sizeof(cases[0]));
}
