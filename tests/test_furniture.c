#include <unistd.h>
#include "test_layout_internal.h"
#include "Layout/layout_furniture.h"
#include "UI/ui_panel_furniture.h"
#include "Input/input_handler.h"
#include "Layout/layout_section.h"
#include "Layout/layout_spatial.h"
#include "Tools/scene_export.h"
#include "Tools/scene_export_dependencies.h"
#include "core_mesh_asset.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
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

static bool add_unit(GlobalState* s, double center, bool rail) {
    LayoutAssembly a = {.frame = {.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(a.info.entity_type,64,"Assembly"); snprintf(a.info.label,64,"Following cabinet");
    if (!Layout_EditAssembly(&s->layout,&a,NULL,NULL,NULL)) return false;
    a=s->layout.objectStore.assemblies[s->layout.objectStore.assembly_count-1];
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];
    snprintf(u.assembly_id,64,"%s",a.id); u.center_m[1]=center; u.part_count=rail?6:5;
    memset(u.parts,0,sizeof(u.parts));
    for (size_t i=0;i<u.part_count;++i) {
        RectPrismPrimitiveCreateParams params={.width=.5f,.height=1,.depth=.018f,.useExplicitFrame=true,
            .explicitFrame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}}; uint32_t id;
        if (!Layout_CreateRectPrismPrimitive(&s->layout,&params,&id,NULL)) return false;
        Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,id); snprintf(o->info.parent_id,64,"%s",a.id);
        snprintf(u.parts[i].entity_id,64,"%s",o->coreMeta.object_id);
        u.parts[i].role=i<5?(LayoutFurnitureRole)i:LAYOUT_FURNITURE_RUN_RAIL;
        if (i==5) {u.parts[i].span_m[0]=.04;u.parts[i].span_m[2]=.03;u.parts[i].offset_m[0]=.26;}
    }
    s->layout.objectStore.furniture[s->layout.objectStore.furniture_count++]=u;
    for (size_t i=0;i<u.part_count;++i) {
        Object3D expected;if (!Layout_FurniturePartGeometry(&s->layout,&u,i,&expected)) return false;
        *Layout_ObjectStore_Find(&s->layout.objectStore,expected.objectId)=expected;
    }
    return Layout_ValidateFurniture(&s->layout,NULL,0);
}
static bool chain(GlobalState* s) {
    if (!setup(s,1,.1) || !add_unit(s,-.5,false) || !add_unit(s,.5,true)) return false;
    LayoutFurnitureContact c={.driver_end=-1,.follower_end=1,.enabled=true};
    snprintf(c.driver,64,"assembly_1");snprintf(c.follower,64,"assembly_2");
    if (!Layout_EditFurnitureContact(&s->layout,&c,NULL,NULL,NULL)) return false;
    c=(LayoutFurnitureContact){.driver_end=1,.follower_end=-1,.enabled=true,.behavior=LAYOUT_FURNITURE_FOLLOW_FIT_RUN};
    snprintf(c.driver,64,"assembly_2");snprintf(c.follower,64,"assembly_3");
    return Layout_EditFurnitureContact(&s->layout,&c,NULL,NULL,NULL);
}
static bool test_contact_chain_history_and_strict_reopen(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(chain(s));Editor_ClearHistory(&s->editor);
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];u.size_m[1]=1.2;
    TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor));
    TEST_ASSERT(fabs(s->layout.objectStore.assemblies[1].frame.origin.y*.1+.2)<1e-6);
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[2].size_m[1]-1.2)<1e-6);
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[2].center_m[1]-.4)<1e-6);
    Object3D* rail=Layout_ObjectStore_Find(&s->layout.objectStore,s->layout.objectStore.items[17].objectId);
    TEST_ASSERT(fabs(rail->rectPrism.height*.1-1.2)<1e-6);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && unchanged(&s->layout,before));
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));
    char* text=Layout_SaveToString(&s->layout);TEST_ASSERT(text);
    Layout reopened;Layout_Init(&reopened,.05f);TEST_ASSERT(Layout_LoadFromString(&reopened,text));
    TEST_ASSERT(Layout_ValidateFurnitureContacts(&reopened,true,NULL,0));Layout_Free(&reopened);
    cJSON* root=cJSON_Parse(text);TEST_ASSERT(root);
    cJSON* e=cJSON_GetObjectItem(root,"engineering");
    cJSON* contact=cJSON_GetArrayItem(cJSON_GetObjectItem(e,"furnitureContacts"),0);
    cJSON_SetNumberValue(cJSON_GetObjectItem(contact,"gapMeters"),.05);
    char* drift=cJSON_Print(root);TEST_ASSERT(!Layout_LoadFromString(&s->layout,drift) && unchanged(&s->layout,text));
    free(drift);cJSON_SetNumberValue(cJSON_GetObjectItem(contact,"gapMeters"),0);
    cJSON_SetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(root,"file"),"schemaVersion"),22);
    drift=cJSON_Print(root);TEST_ASSERT(!Layout_LoadFromString(&s->layout,drift) && unchanged(&s->layout,text));
    free(drift);cJSON_Delete(root);Layout_FreeString(text);Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_contact_conflicts_locks_rotation_and_disable(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(chain(s));Editor_ClearHistory(&s->editor);
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    LayoutFurnitureContact c={.driver_end=1,.follower_end=-1,.enabled=true};
    snprintf(c.driver,64,"assembly_2");snprintf(c.follower,64,"assembly_1");
    TEST_ASSERT(!Layout_EditFurnitureContact(&s->layout,&c,NULL,NULL,NULL) && unchanged(&s->layout,before));
    snprintf(c.driver,64,"assembly_3");snprintf(c.follower,64,"assembly_2");c.driver_end=-1;c.follower_end=1;
    TEST_ASSERT(!Layout_EditFurnitureContact(&s->layout,&c,NULL,NULL,NULL) && unchanged(&s->layout,before));
    LayoutFurnitureUnit follower=s->layout.objectStore.furniture[1];follower.size_m[1]=1.2;
    TEST_ASSERT(!Layout_EditFurnitureUnit(&s->layout,&follower,-1,NULL,NULL) && unchanged(&s->layout,before));
    TEST_ASSERT(!Layout_MoveAssembly(&s->layout,"assembly_2",(double[]){0,-.1,0},(Vec3){0},NULL,NULL) && unchanged(&s->layout,before));
    TEST_ASSERT(!Layout_MoveAssembly(&s->layout,"assembly_1",(double[]){0},(Vec3){0,0,30},NULL,NULL) && unchanged(&s->layout,before));
    s->layout.objectStore.items[7].coreMeta.flags.locked=true;Layout_FreeString(before);before=Layout_SaveToString(&s->layout);
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];u.size_m[1]=1.2;
    TEST_ASSERT(!Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor) && unchanged(&s->layout,before));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0);s->layout.objectStore.items[7].coreMeta.flags.locked=false;
    s->layout.objectStore.items[17].coreMeta.flags.locked=true;
    Layout_FreeString(before);before=Layout_SaveToString(&s->layout);
    /* A failure in the final rail rolls back the already-evaluated kitchen and pantry. */
    TEST_ASSERT(!Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor) && unchanged(&s->layout,before));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0);s->layout.objectStore.items[17].coreMeta.flags.locked=false;
    c=s->layout.objectStore.furniture_contacts[0];c.enabled=false;
    TEST_ASSERT(Layout_EditFurnitureContact(&s->layout,&c,NULL,NULL,NULL));
    TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,1,NULL,NULL));
    TEST_ASSERT(fabs(s->layout.objectStore.assemblies[1].frame.origin.y)<1e-8);
    c.enabled=true;TEST_ASSERT(Layout_EditFurnitureContact(&s->layout,&c,NULL,NULL,NULL));
    TEST_ASSERT(fabs(s->layout.objectStore.assemblies[1].frame.origin.y*.1+.2)<1e-6);
    c=s->layout.objectStore.furniture_contacts[0];c.gap_m=.01;
    TEST_ASSERT(!Layout_EditFurnitureContact(&s->layout,&c,NULL,deny,NULL));
    TEST_ASSERT(Layout_EditFurnitureContact(&s->layout,&c,NULL,Editor_ReserveGeometryHistory,&s->editor));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(fabs(s->layout.objectStore.assemblies[1].frame.origin.y*.1+.21)<1e-6);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(Layout_EditFurnitureContact(&s->layout,NULL,c.id,NULL,NULL));
    TEST_ASSERT(s->layout.objectStore.furniture_contact_count==1);
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_native_connection_controls(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(chain(s));Editor_ClearHistory(&s->editor);
    s->editor.selectedObject3DId=s->layout.objectStore.items[0].objectId;
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_OBJECT);UIPanel_OnWindowResized(1400,1200);
    TEST_ASSERT(click(UI_FURNITURE_OPEN));TEST_ASSERT(click(UI_FURNITURE_CONNECTIONS));
    TEST_ASSERT(click(UI_FURNITURE_ENABLED));TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));
    TEST_ASSERT(!s->layout.objectStore.furniture_contacts[0].enabled && Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    /* Undo refreshes the saved Enabled state without reopening/resetting the form. */
    TEST_ASSERT(click(UI_FURNITURE_ENABLED));TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));
    TEST_ASSERT(!s->layout.objectStore.furniture_contacts[0].enabled);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(click(UI_FURNITURE_GAP));SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,sizeof(text.text.text),"15 mm");Input_Handle(NULL,&text);
    TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));
    TEST_ASSERT(fabs(s->layout.objectStore.furniture_contacts[0].gap_m-.015)<1e-9);
    TEST_ASSERT(Layout_ValidateFurnitureContacts(&s->layout,true,NULL,0));
    TEST_ASSERT(click(UI_FURNITURE_GAP));snprintf(text.text.text,sizeof(text.text.text),"-1 mm");Input_Handle(NULL,&text);
    size_t count=Editor_UndoCount(&s->editor);TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));TEST_ASSERT(Editor_UndoCount(&s->editor)==count);
    TEST_ASSERT(click(UI_FURNITURE_CANCEL));
    s->editor.selectedObject3DId=s->layout.objectStore.items[7].objectId;
    TEST_ASSERT(click(UI_FURNITURE_OPEN));TEST_ASSERT(click(UI_FURNITURE_CONNECTIONS));
    TEST_ASSERT(click(UI_FURNITURE_LINK));TEST_ASSERT(click(UI_FURNITURE_GAP));
    snprintf(text.text.text,sizeof(text.text.text),"0.0123456789 m");Input_Handle(NULL,&text);
    TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));
    TEST_ASSERT(fabs(s->layout.objectStore.furniture_contacts[1].gap_m-.0123456789)<1e-12);
    count=Editor_UndoCount(&s->editor);TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==count); /* Unedited gap retains full saved precision. */
    TEST_ASSERT(click(UI_FURNITURE_ENABLED));TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));
    TEST_ASSERT(!s->layout.objectStore.furniture_contacts[1].enabled && s->layout.objectStore.furniture_contacts[0].enabled);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(click(UI_FURNITURE_ENABLED));TEST_ASSERT(click(UI_FURNITURE_SAVE_LINK));
    TEST_ASSERT(!s->layout.objectStore.furniture_contacts[1].enabled); /* Second link survives Undo refresh. */
    ld_test_shutdown_runtime();return true;
}

static bool test_rotated_contact_group(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(chain(s));
    LayoutAssembly group={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(group.info.entity_type,64,"Assembly");snprintf(group.info.label,64,"Connected group");
    TEST_ASSERT(Layout_EditAssembly(&s->layout,&group,NULL,NULL,NULL));
    for (size_t i=0;i<3;++i) {
        LayoutEntityInfo info=s->layout.objectStore.assemblies[i].info;snprintf(info.parent_id,64,"assembly_4");
        TEST_ASSERT(Layout_SetEntityInfo(&s->layout,s->layout.objectStore.assemblies[i].id,&info,NULL,NULL));
    }
    TEST_ASSERT(Layout_MoveAssembly(&s->layout,"assembly_4",(double[]){1,2,0},(Vec3){0,0,30},NULL,NULL));
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];u.size_m[1]=1.1;
    TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,1,NULL,NULL));
    TEST_ASSERT(Layout_ValidateFurnitureContacts(&s->layout,true,NULL,0));
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[2].size_m[1]-1.1)<2e-6);
    ld_test_shutdown_runtime();return true;
}
static bool test_full_van_connected_run_acceptance(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    TEST_ASSERT(Layout_LoadFromFile(&s->layout,"config/examples/van_construction_f2.layout.json"));
    TEST_ASSERT(s->layout.objectStore.furniture_count==3 && s->layout.objectStore.furniture_contact_count==2);
    Object3D* bed=NULL;Object3D* rail=NULL;
    for (size_t i=0;i<s->layout.objectStore.count;++i) {
        Object3D* o=&s->layout.objectStore.items[i];
        if (!strcmp(o->coreMeta.object_id,"bed_deck")) bed=o;
        if (!strcmp(o->coreMeta.object_id,"upper_cabinet_passenger_mount_rail")) rail=o;
    }
    TEST_ASSERT(rail);Object3D rail_before=*rail;
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);Editor_ClearHistory(&s->editor);
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];double length=u.size_m[1];u.size_m[1]+=.05;
    TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    const LayoutAssembly* pantry=Layout_FindAssembly(&s->layout.objectStore,"storage_passenger_unit");TEST_ASSERT(pantry);
    TEST_ASSERT(fabs(pantry->frame.origin.y+.05)<1e-6);
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[2].size_m[1]-length-.05)<2e-6);
    TEST_ASSERT(fabs(rail->rectPrism.height-rail_before.rectPrism.height-.05)<2e-6);
    TEST_ASSERT(s->layout.objectStore.route_count==22 && s->layout.objectStore.saved_view_count==13);
    TEST_ASSERT(Layout_ValidateFurnitureContacts(&s->layout,true,NULL,0));
    if (bed) TEST_ASSERT(bed->rectPrism.height<=.9144f);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && unchanged(&s->layout,before));
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_sink_removed_material_sections_checks_export(void) {
    ld_test_init_runtime(); GlobalState* s=Global_Get();
    TEST_ASSERT(Layout_LoadFromFile(&s->layout,"config/examples/van_construction_f4.layout.json"));
    const Object3D* board=NULL;
    for (size_t i=0;i<s->layout.objectStore.count;++i)
        if (!strcmp(s->layout.objectStore.items[i].coreMeta.object_id,"kitchen_worktop")) board=&s->layout.objectStore.items[i];
    TEST_ASSERT(board && board->rectPrism.opening.enabled);
    LayoutSurfaceTriangle triangles[LAYOUT_SURFACE_MAX_TRIANGLES];
    LayoutSectionView v={.mode=LAYOUT_SECTION_EXACT,.axis=2,.position_meters=board->transform.position.z};
    size_t n=Layout_BuildNativeSurface(board,&v,1,triangles); double area=0;
    for (size_t i=0;i<n;++i) area += .5*Vec3_Length(Vec3_Cross(Vec3_Sub(triangles[i].b,triangles[i].a),Vec3_Sub(triangles[i].c,triangles[i].a)));
    const LayoutPanelOpening* f=&board->rectPrism.opening;
    TEST_ASSERT(fabs(area-(board->rectPrism.width*board->rectPrism.height-f->width*f->height))<1e-6);
    Object3D probe=*board; probe.rectPrism.opening=(LayoutPanelOpening){0};
    probe.rectPrism.width=.01f;probe.rectPrism.height=.01f;probe.rectPrism.depth=.10f;
    probe.transform.position=Vec3_Add(board->transform.position,(Vec3){f->u,f->v,0});probe.rectPrism.frame.origin=probe.transform.position;
    double distance; bool overlap,approx;
    TEST_ASSERT(Layout_SpatialDistance(&s->layout,board,&probe,&distance,&overlap,&approx) && !overlap && !approx && distance>.14);
    double a[]={probe.transform.position.x,probe.transform.position.y,.5},b[]={a[0],a[1],1.2},lo,hi;
    TEST_ASSERT(Layout_SpatialSegmentInterval(&s->layout,board,a,b,0,&lo,&hi,&approx) && lo>hi);
    probe.transform.position.x=board->transform.position.x-board->rectPrism.width/2+.02f;probe.rectPrism.frame.origin=probe.transform.position;
    TEST_ASSERT(Layout_SpatialDistance(&s->layout,board,&probe,&distance,&overlap,&approx) && overlap);
    /* Material raster ownership/picking sees the void rather than the old box. */
    Layout only; Layout_Init(&only,1);
    uint32_t only_id=Layout_ObjectStore_Create(&only.objectStore,board->kind,&board->transform,board->coreMeta.object_type,board->coreMeta.dimensional_mode,board->coreMeta.locked_plane);
    TEST_ASSERT(only_id);
    *Layout_ObjectStore_Find(&only.objectStore,only_id)=*board;
    enum {W=128,H=128}; uint8_t rgba[W*H*4]={0}; float depth[W*H]; int32_t owner[W*H];
    for (int i=0;i<W*H;++i) { depth[i]=INFINITY; owner[i]=-1; }
    SpaceViewContext view={.plane={.axis=VIEW_PLANE_XY}};
    Grid grid={.gridSize=1,.scale=100,.offsetX=a[0]-.64f,.offsetY=a[1]-.64f};
    LayoutMeshSolidPreviewFrameStats stats={0};
    TEST_ASSERT(Layout_RasterNativeSurfaces(&only,NULL,&view,&grid,(SDL_Rect){0,0,W,H},1,W,H,true,rgba,depth,owner,&stats));
    TEST_ASSERT(owner[64*W+64]==-1 && rgba[(64*W+64)*4+3]==0);
    Layout_Free(&only);
    /* One board ID exports as one mesh dependency with the same evaluated surface. */
    char root[]="/tmp/ld_f34_export_XXXXXX";TEST_ASSERT(mkdtemp(root));
    LineDrawingSceneExportPaths paths; char diagnostics[256]={0};
    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&s->layout,"f34.layout.json",root,&paths,diagnostics,sizeof(diagnostics)));
    TEST_ASSERT(paths.dependency_count==3);
    Layout imported;Layout_Init(&imported,1);
    TEST_ASSERT(LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,paths.authoring_path,diagnostics,sizeof(diagnostics)));
    TEST_ASSERT(imported.objectStore.furniture_count==9 && imported.objectStore.furniture[0].sink_opening);
    TEST_ASSERT(Layout_ValidateFurniture(&imported,NULL,0));Layout_Free(&imported);
    LineDrawingSceneExportDependencies dependencies;
    TEST_ASSERT(LineDrawingSceneExportDependencies_Collect(&s->layout,paths.authoring_path,&dependencies,diagnostics,sizeof(diagnostics)));
    TEST_ASSERT(dependencies.count==3);
    for (size_t i=0;i<dependencies.count;++i) {
        cJSON* mesh=cJSON_Parse(dependencies.entries[i].payload_data);TEST_ASSERT(mesh);
        const cJSON* payload=cJSON_GetObjectItem(mesh,"mesh");
        TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItem(payload,"triangles"))>12);
        const cJSON* vertices=cJSON_GetObjectItem(payload,"vertices"),*faces=cJSON_GetObjectItem(payload,"triangles");
        double signed_volume=0;
        for (int j=0;j<cJSON_GetArraySize(faces);++j) {
            const cJSON* face=cJSON_GetArrayItem(faces,j);Vec3 p[3];const char* keys[]={"a","b","c"};
            for (int k=0;k<3;++k) {
                const cJSON* vertex=cJSON_GetArrayItem(vertices,cJSON_GetObjectItem(face,keys[k])->valueint);
                p[k]=(Vec3){(float)cJSON_GetObjectItem(vertex,"x")->valuedouble,(float)cJSON_GetObjectItem(vertex,"y")->valuedouble,(float)cJSON_GetObjectItem(vertex,"z")->valuedouble};
            }
            signed_volume+=Vec3_Dot(p[0],Vec3_Cross(p[1],p[2]))/6.0;
        }
        const Object3D* object=NULL;
        for (size_t j=0;j<s->layout.objectStore.count;++j) if (!strcmp(dependencies.entries[i].identity,s->layout.objectStore.items[j].coreMeta.object_id)) object=&s->layout.objectStore.items[j];
        TEST_ASSERT(object);const RectPrismPrimitive3D* prism=&object->rectPrism;
        double expected=prism->width*prism->height*prism->depth-prism->opening.width*prism->opening.height*(prism->depth-prism->opening.floor);
        TEST_ASSERT(fabs(signed_volume-expected)<1e-6);
        char mesh_path[256];snprintf(mesh_path,sizeof(mesh_path),"%s/mesh%zu.runtime.json",root,i);
        FILE* file=fopen(mesh_path,"wb");TEST_ASSERT(file);
        TEST_ASSERT(fwrite(dependencies.entries[i].payload_data,1,dependencies.entries[i].content_bytes,file)==dependencies.entries[i].content_bytes);
        TEST_ASSERT(fclose(file)==0);
        CoreMeshAssetRuntimeDocument document;core_mesh_asset_runtime_document_init(&document);
        TEST_ASSERT(core_mesh_asset_runtime_document_load_file(mesh_path,&document).code==CORE_OK);
        core_mesh_asset_runtime_document_free(&document);
        cJSON_Delete(mesh);
    }
    LineDrawingSceneExportDependencies_Destroy(&dependencies);
    ld_test_shutdown_runtime();return true;
}
static bool test_sink_form_atomic_undo_reopen(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    TEST_ASSERT(Layout_LoadFromFile(&s->layout,"config/examples/van_construction_f4.layout.json"));
    Editor_ClearHistory(&s->editor);
    s->editor.selectedObject3DId=s->layout.objectStore.items[0].objectId;
    for (size_t i=0;i<s->layout.objectStore.count;++i) if (!strcmp(s->layout.objectStore.items[i].coreMeta.object_id,"sink")) s->editor.selectedObject3DId=s->layout.objectStore.items[i].objectId;
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_OBJECT);UIPanel_OnWindowResized(1600,1200);
    TEST_ASSERT(click(UI_FURNITURE_OPEN) && click(UI_FURNITURE_SINK));
    TEST_ASSERT(click(UI_FURNITURE_SINK_WIDTH));
    SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,sizeof(text.text.text),"340 mm");Input_Handle(NULL,&text);
    TEST_ASSERT(click(UI_FURNITURE_APPLY));
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[0].parts[6].span_m[0]-.34)<1e-9);
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1 && Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(fabs(s->layout.objectStore.furniture[0].parts[6].span_m[0]-.36)<1e-6);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout));
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    LayoutFurnitureUnit u=s->layout.objectStore.furniture[0];u.parts[6].span_m[0]=.6;
    TEST_ASSERT(!Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor) && unchanged(&s->layout,before));
    u=s->layout.objectStore.furniture[0];u.thickness_m[3]=.025;
    TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor));
    TEST_ASSERT(Layout_ValidateFurniture(&s->layout,NULL,0));
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && unchanged(&s->layout,before));
    Layout reopened;Layout_Init(&reopened,1);TEST_ASSERT(Layout_LoadFromString(&reopened,before));
    TEST_ASSERT(reopened.objectStore.furniture[0].sink_opening && Layout_ValidateFurniture(&reopened,NULL,0));
    cJSON* json=cJSON_Parse(before);cJSON_SetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(json,"file"),"schemaVersion"),23);
    char* downgrade=cJSON_Print(json);TEST_ASSERT(!Layout_LoadFromString(&s->layout,downgrade) && unchanged(&s->layout,before));
    cJSON_Delete(json);free(downgrade);Layout_FreeString(before);Layout_Free(&reopened);ld_test_shutdown_runtime();return true;
}
static bool test_whole_van_managed_units_ui_and_resize(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();
    TEST_ASSERT(Layout_LoadFromFile(&s->layout,"config/examples/van_construction_f4.layout.json"));
    TEST_ASSERT(s->layout.objectStore.furniture_count==9 && s->layout.objectStore.furniture_contact_count==5);
    Editor_ClearHistory(&s->editor);
    char* before=Layout_SaveToString(&s->layout);TEST_ASSERT(before);
    for (size_t i=0;i<9;++i) {
        LayoutFurnitureUnit u=s->layout.objectStore.furniture[i];
        s->editor.selectedObject3DId=0;
        for (size_t j=0;j<s->layout.objectStore.count;++j) if (!strcmp(s->layout.objectStore.items[j].coreMeta.object_id,u.parts[0].entity_id)) s->editor.selectedObject3DId=s->layout.objectStore.items[j].objectId;
        UIPanel_FurnitureReset();UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_OBJECT);UIPanel_OnWindowResized(1600,1200);
        TEST_ASSERT(click(UI_FURNITURE_OPEN) && click(UI_FURNITURE_PANELS));
        SDL_Rect rect;TEST_ASSERT(UIPanel_FurnitureControlRect(UI_FURNITURE_CASE,&rect));
        if (i>=7) TEST_ASSERT(UIPanel_FurnitureControlRect(UI_FURNITURE_OVERHANG_FRONT,&rect));
        u.size_m[0]+=.01;u.size_m[2]+=.01;
        TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&u,1,Editor_ReserveGeometryHistory,&s->editor));
        TEST_ASSERT(Layout_ValidateFurniture(&s->layout,NULL,0));
        TEST_ASSERT(s->layout.objectStore.route_count==22 && s->layout.objectStore.saved_view_count==13);
        TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && unchanged(&s->layout,before));
    }
    LayoutFurnitureUnit bench=s->layout.objectStore.furniture[3];bench.size_m[1]+=.05;
    TEST_ASSERT(Layout_EditFurnitureUnit(&s->layout,&bench,1,Editor_ReserveGeometryHistory,&s->editor));
    TEST_ASSERT(Layout_ValidateFurnitureContacts(&s->layout,true,NULL,0));
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && unchanged(&s->layout,before));
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
bool furniture_run_tests(void) {
    const TestCase cases[] = {
        {"sink_geometry_sections_checks_export", test_sink_removed_material_sections_checks_export},
        {"sink_form_history_strict_reopen", test_sink_form_atomic_undo_reopen},
        {"whole_van_unit_controls_resize", test_whole_van_managed_units_ui_and_resize},
        {"rotated_connected_group", test_rotated_contact_group},
        {"full_van_connected_run", test_full_van_connected_run_acceptance},
        {"contact_chain_history_reopen", test_contact_chain_history_and_strict_reopen},
        {"contact_conflicts_locks_disable", test_contact_conflicts_locks_rotation_and_disable},
        {"native_connection_controls", test_native_connection_controls},
        {"resize_anchor_thickness_ids_history", test_resize_fixed_thickness_identity_anchor_history},
        {"atomic_failure_managed_parts", test_failures_are_atomic_and_parts_cannot_drift},
        {"rotation_strict_loading", test_rotated_unit_and_strict_recipe_loading},
        {"native_form_apply_cancel_resize", test_visible_form_apply_cancel_resize_and_history}};
    return run_test_cases("Furniture", cases, sizeof(cases) / sizeof(cases[0]));
}
