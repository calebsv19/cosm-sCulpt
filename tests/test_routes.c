#include "test_layout_internal.h"
#include "Layout/layout_routes.h"
#include "UI/ui_panel_routes.h"
#include "UI/ui_panel_right_scroll.h"
#include "Input/input_handler.h"
#include "core_scene_compile.h"

static Layout* live(void) { return &Global_Get()->layout; }
static uint32_t part(const char* name, Vec3 position) {
    RectPrismPrimitiveCreateParams params = {.width=.1f,.height=.1f,.depth=.1f,.useExplicitFrame=true,
        .explicitFrame={.origin=position,.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t id=0;
    if (!Layout_CreateRectPrismPrimitive(live(),&params,&id,NULL)) return 0;
    Object3D* object=Layout_ObjectStore_Find(&live()->objectStore,id);
    if (core_object_set_identity(&object->coreMeta,name,"rect_prism_primitive").code!=CORE_OK) return 0;
    return id;
}
static LayoutPhysicalRoute route(void) {
    LayoutPhysicalRoute r={.point_count=3,.points_meters={{0,0,0},{3,0,0},{3,4,0}}};
    snprintf(r.info.label,sizeof(r.info.label),"Test CAN cable");
    snprintf(r.info.entity_type,sizeof(r.info.entity_type),"Cable");
    snprintf(r.source.entity_id,sizeof(r.source.entity_id),"A");
    snprintf(r.destination.entity_id,sizeof(r.destination.entity_id),"B");
    return r;
}
static bool setup(void) {
    ld_test_init_runtime();
    TEST_ASSERT(part("A",(Vec3){0}) && part("B",(Vec3){3,4,0}));
    Editor_ClearHistory(&Global_Get()->editor);
    return true;
}
static bool same(const char* before) {
    char* after=Layout_SaveToString(live());bool ok=after && !strcmp(before,after);Layout_FreeString(after);return ok;
}
static bool deny(const Layout* layout,void* context) {(void)layout;(void)context;return false;}
static bool test_physical_length_history_stale_refresh_identity(void) {
    TEST_ASSERT(setup());LayoutPhysicalRoute r=route();
    TEST_ASSERT(fabs(Layout_RouteLength(&r)-7)<1e-9 && Layout_RouteEndpointsCurrent(live(),&r));
    TEST_ASSERT(Layout_EditRoute(live(),&r,NULL,Layout_GeometryHistory,NULL));
    TEST_ASSERT(live()->objectStore.route_count==1 && Editor_UndoCount(&Global_Get()->editor)==1);
    TEST_ASSERT(!Layout_ObjectStore_Delete(&live()->objectStore,2));
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,live()) && !live()->objectStore.route_count);
    TEST_ASSERT(Editor_Redo(&Global_Get()->editor,live()) && !strcmp(live()->objectStore.routes[0].id,"route_1"));
    LayoutEntityInfo info=live()->objectStore.items[1].info;snprintf(info.label,sizeof(info.label),"Renamed target");
    TEST_ASSERT(Layout_SetEntityInfo(live(),"B",&info,NULL,NULL));
    TEST_ASSERT(Layout_SetObject3DPosition(live(),2,(Vec3){3,5,0},NULL));
    TEST_ASSERT(!Layout_RouteEndpointsCurrent(live(),&live()->objectStore.routes[0]));
    TEST_ASSERT(live()->objectStore.routes[0].points_meters[2][1]==4);
    TEST_ASSERT(Layout_RefreshRouteEndpoints(live(),"route_1",Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_RouteEndpointsCurrent(live(),&live()->objectStore.routes[0]) && fabs(Layout_RouteLength(&live()->objectStore.routes[0])-8)<1e-9);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,live()) && live()->objectStore.routes[0].points_meters[2][1]==4);
    TEST_ASSERT(Editor_Redo(&Global_Get()->editor,live()));
    ld_test_shutdown_runtime();return true;
}
static bool test_invalid_edits_and_denied_history_are_atomic(void) {
    TEST_ASSERT(setup());LayoutPhysicalRoute r=route();char* before=Layout_SaveToString(live());
    TEST_ASSERT(!Layout_EditRoute(live(),&r,NULL,deny,NULL) && same(before));
    for (int i=0;i<7;++i) {
        r=route();
        if(i==0)r.points_meters[1][0]=NAN;
        if(i==1)r.point_count=65;
        if(i==2)snprintf(r.destination.entity_id,64,"missing");
        if(i==3)snprintf(r.destination.entity_id,64,"A");
        if(i==4)snprintf(r.info.entity_type,64,"Controller");
        if(i==5)snprintf(r.id,64,"unknown_route");
        if(i==6)r.source.local_offset_meters[0]=INFINITY;
        TEST_ASSERT(!Layout_EditRoute(live(),&r,NULL,Layout_GeometryHistory,NULL) && same(before));
    }
    TEST_ASSERT(!Editor_UndoCount(&Global_Get()->editor));Layout_FreeString(before);
    r=route();TEST_ASSERT(Layout_EditRoute(live(),&r,NULL,NULL,NULL));
    LayoutAssembly assembly={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(assembly.info.label,sizeof(assembly.info.label),"Assembly");
    snprintf(assembly.info.entity_type,sizeof(assembly.info.entity_type),"Assembly");
    TEST_ASSERT(Layout_EditAssembly(live(),&assembly,NULL,NULL,NULL));
    LayoutEntityInfo info=live()->objectStore.items[1].info;snprintf(info.parent_id,64,"%s",live()->objectStore.assemblies[0].id);
    TEST_ASSERT(Layout_SetEntityInfo(live(),"B",&info,NULL,NULL) && Layout_RouteEndpointsCurrent(live(),&live()->objectStore.routes[0]));
    TEST_ASSERT(Layout_EditRoute(live(),NULL,"route_1",NULL,NULL) && Layout_ObjectStore_Delete(&live()->objectStore,2));
    ld_test_shutdown_runtime();return true;
}
static bool test_native_migration_export_and_malformed_documents(void) {
    Layout poisoned;memset(&poisoned,0xa5,sizeof(poisoned));Layout_Init(&poisoned,.05f);
    TEST_ASSERT(!poisoned.objectStore.route_count && poisoned.objectStore.next_route_id==1);Layout_Free(&poisoned);
    TEST_ASSERT(setup());LayoutPhysicalRoute r=route();TEST_ASSERT(Layout_EditRoute(live(),&r,NULL,NULL,NULL));
    char* before=Layout_SaveToString(live());TEST_ASSERT(before && Layout_LoadFromString(live(),before) && same(before));
    for(int bad=0;bad<5;++bad) {
        cJSON* root=cJSON_Parse(before);cJSON* eng=cJSON_GetObjectItemCaseSensitive(root,"engineering");
        cJSON* routes=cJSON_GetObjectItemCaseSensitive(eng,"routes");cJSON* saved=cJSON_GetArrayItem(routes,0);
        if(bad==0)cJSON_DeleteItemFromObjectCaseSensitive(eng,"routes");
        if(bad==1)cJSON_AddItemToArray(routes,cJSON_Duplicate(saved,true));
        if(bad==2)cJSON_ReplaceItemInObjectCaseSensitive(saved,"id",cJSON_CreateString("A"));
        if(bad==3)cJSON_ReplaceItemInObjectCaseSensitive(eng,"nextRouteId",cJSON_CreateNumber(1.5));
        if(bad==4)cJSON_ReplaceItemInObjectCaseSensitive(saved,"points_m",cJSON_CreateArray());
        char* invalid=cJSON_PrintUnformatted(root);TEST_ASSERT(!Layout_LoadFromString(live(),invalid) && same(before));
        free(invalid);cJSON_Delete(root);
    }
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(live(),"route_test");char* compiled=NULL;char diagnostics[256];
    TEST_ASSERT(authored && core_scene_compile_authoring_to_runtime(authored,&compiled,diagnostics,sizeof(diagnostics)).code==CORE_OK);
    cJSON* runtime=cJSON_Parse(compiled);cJSON* ext=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(runtime,"extensions"),"line_drawing");
    cJSON* report=cJSON_GetObjectItemCaseSensitive(ext,"physical_routes");
    TEST_ASSERT(cJSON_GetObjectItemCaseSensitive(report,"count")->valueint==1);
    TEST_ASSERT(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(report,"routes"),0),"length_m")->valuedouble==7);
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(runtime,"objects"))==5);
    free(authored);free(compiled);cJSON_Delete(runtime);
    cJSON* root=cJSON_Parse(before);cJSON* eng=cJSON_GetObjectItemCaseSensitive(root,"engineering");
    cJSON_DeleteItemFromObjectCaseSensitive(eng,"routes");cJSON_DeleteItemFromObjectCaseSensitive(eng,"nextRouteId");
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(19));
    char* legacy=cJSON_PrintUnformatted(root);TEST_ASSERT(Layout_LoadFromString(live(),legacy) && !live()->objectStore.route_count);
    free(legacy);cJSON_Delete(root);Layout_FreeString(before);
    live()->metersPerWorldUnit=.001;TEST_ASSERT(Layout_SetObject3DPosition(live(),2,(Vec3){3000,4000,0},NULL));
    r=route();TEST_ASSERT(Layout_RouteEndpointsCurrent(live(),&r) && fabs(Layout_RouteLength(&r)-7)<1e-9);
    ld_test_shutdown_runtime();return true;
}
static bool click(int command) {
    SDL_Rect rect;TEST_ASSERT(UIPanel_RoutesControlRect(command,&rect));
    TEST_ASSERT(rect.y>=UIPanel_Get()->rightBodyRect.y && rect.y+rect.h<=UIPanel_Get()->rightBodyRect.y+UIPanel_Get()->rightBodyRect.h);
    SDL_Event event={.type=SDL_MOUSEBUTTONDOWN};event.button.button=SDL_BUTTON_LEFT;
    event.button.x=rect.x+rect.w/2;event.button.y=rect.y+rect.h/2;AppContext context={0};Input_Handle(&context,&event);return true;
}
static bool test_visible_draft_controls_undo_cancel_and_wrapped_tabs(void) {
    TEST_ASSERT(setup());Global_SetWindowSize(1800,2200);UIPanel_OnWindowResized(1800,2200);
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_ROUTES);
    TEST_ASSERT(click(ROUTES_NEW));Global_Get()->editor.selectedObject3DId=1;
    TEST_ASSERT(click(ROUTES_USE_SOURCE));Global_Get()->editor.selectedObject3DId=2;
    TEST_ASSERT(click(ROUTES_USE_DESTINATION) && click(ROUTES_INSERT));
    TEST_ASSERT(UIPanel_Get()->routes.draft.point_count==3 && !live()->objectStore.route_count);
    TEST_ASSERT(click(ROUTES_X));SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,sizeof(text.text.text),"25 mm");
    TEST_ASSERT(UIPanel_RoutesEvent(&text) && click(ROUTES_SAVE));
    TEST_ASSERT(live()->objectStore.route_count==1 && fabs(live()->objectStore.routes[0].points_meters[1][0]-.025)<1e-9);
    TEST_ASSERT(Editor_UndoCount(&Global_Get()->editor)==1);
    TEST_ASSERT(Editor_Undo(&Global_Get()->editor,live()));UIPanel_LayoutRoutes();TEST_ASSERT(!UIPanel_Get()->routes.observed_valid);
    TEST_ASSERT(Editor_Redo(&Global_Get()->editor,live()));UIPanel_LayoutRoutes();TEST_ASSERT(UIPanel_Get()->routes.observed_valid);
    TEST_ASSERT(click(ROUTES_NEXT) && click(ROUTES_PREVIOUS) && click(ROUTES_X));
    snprintf(text.text.text,sizeof(text.text.text),"50 mm");TEST_ASSERT(UIPanel_RoutesEvent(&text) && click(ROUTES_CANCEL));
    TEST_ASSERT(fabs(UIPanel_Get()->routes.draft.points_meters[1][0]-.025)<1e-9);
    TEST_ASSERT(click(ROUTES_PICK) && UIPanel_Get()->routes.picking && click(ROUTES_PICK) && !UIPanel_Get()->routes.picking);
    SDL_Rect left={0,0,300,1000},right={1400,0,350,1000};UIPanel_UpdateTabLayout(UIPanel_Get(),&left,&right,NULL);
    const UIPanelState* ui=UIPanel_Get();
    for(int i=0;i<UI_PANEL_RIGHT_TAB_COUNT;++i) {
        TEST_ASSERT(ui->rightTabs[i].bounds.w>=(int)strlen(ui->rightTabs[i].label)*8+16);
        TEST_ASSERT(ui->rightTabs[i].bounds.y+ui->rightTabs[i].bounds.h<ui->rightBodyRect.y);
        TEST_ASSERT(ui->rightTabs[i].bounds.x+ui->rightTabs[i].bounds.w<=right.x+right.w);
    }
    TEST_ASSERT(ui->rightTabs[UI_PANEL_RIGHT_TAB_ROUTES].bounds.y>ui->rightTabs[0].bounds.y);
    ld_test_shutdown_runtime();return true;
}
bool routes_run_tests(void) {
    const TestCase tests[]={
        {"length_history_stale_refresh_identity",test_physical_length_history_stale_refresh_identity},
        {"invalid_edits_atomic",test_invalid_edits_and_denied_history_are_atomic},
        {"native_migration_export",test_native_migration_export_and_malformed_documents},
        {"visible_draft_controls",test_visible_draft_controls_undo_cancel_and_wrapped_tabs},
    };
    return run_test_cases("Routes",tests,sizeof(tests)/sizeof(tests[0]));
}
