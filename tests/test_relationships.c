#include "test_layout_internal.h"
#include "Layout/layout_relationships.h"
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_scene_list.h"
#include "Input/input_handler.h"
#include "Tools/canonical_scene_export.h"
#include "core_scene_compile.h"

static uint32_t part(const char* id, Vec3 origin) {
    RectPrismPrimitiveCreateParams p={.width=1,.height=.5f,.depth=.2f,.useExplicitFrame=true,
        .explicitFrame={.origin=origin,.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t handle=0;
    if (!Layout_CreateRectPrismPrimitive(&Global_Get()->layout,&p,&handle,NULL)) return 0;
    Object3D* o=Layout_ObjectStore_Find(&Global_Get()->layout.objectStore,handle);
    if (core_object_set_identity(&o->coreMeta,id,"rect_prism_primitive").code!=CORE_OK) return 0;
    return handle;
}
static bool assembly(char id[64]) {
    LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(a.info.label,96,"Water cabinet");snprintf(a.info.entity_type,64,"Assembly");
    Layout* l=&Global_Get()->layout;
    if (!Layout_EditAssembly(l,&a,NULL,Layout_GeometryHistory,NULL)) return false;
    snprintf(id,64,"%s",l->objectStore.assemblies[l->objectStore.assembly_count-1].id);return true;
}
static LayoutRelationship edge(const char* source, const char* target, LayoutRelationshipKind kind) {
    LayoutRelationship r={.kind=kind};snprintf(r.source,64,"%s",source);snprintf(r.target,64,"%s",target);return r;
}
static bool add(const char* source, const char* target, LayoutRelationshipKind kind) {
    LayoutRelationship r=edge(source,target,kind);
    return Layout_EditRelationship(&Global_Get()->layout,&r,NULL,Layout_GeometryHistory,NULL);
}
static bool same_json(Layout* layout, const char* before) {
    char* after=Layout_SaveToString(layout);bool same=after && !strcmp(before,after);Layout_FreeString(after);return same;
}
static bool test_directed_queries_history_identity(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char cabinet[64];
    TEST_ASSERT(part("node",(Vec3){0}) && part("rail",(Vec3){2,0,0}) && assembly(cabinet));
    Object3D node=s->layout.objectStore.items[0],rail=s->layout.objectStore.items[1];
    Editor_ClearHistory(&s->editor);
    TEST_ASSERT(add("node","rail",LAYOUT_RELATIONSHIP_ATTACHED_TO));
    TEST_ASSERT(add("node","rail",LAYOUT_RELATIONSHIP_SUPPORTED_BY));
    TEST_ASSERT(add("node",cabinet,LAYOUT_RELATIONSHIP_CONTAINED_BY));
    TEST_ASSERT(!memcmp(&node,&s->layout.objectStore.items[0],sizeof(node)) && !memcmp(&rail,&s->layout.objectStore.items[1],sizeof(rail)));
    TEST_ASSERT(!s->layout.objectStore.items[0].info.parent_id[0] && Editor_UndoCount(&s->editor)==3);
    char ids[1][64];TEST_ASSERT(Layout_QueryRelationships(&s->layout.objectStore,"node",-1,1,ids,1)==3 && !strcmp(ids[0],"relationship_1"));
    TEST_ASSERT(Layout_QueryRelationships(&s->layout.objectStore,"rail",-1,2,NULL,0)==2 && Layout_QueryRelationships(&s->layout.objectStore,"rail",-1,1,NULL,0)==0);
    TEST_ASSERT(Layout_QueryRelationships(&s->layout.objectStore,NULL,LAYOUT_RELATIONSHIP_CONTAINED_BY,0,NULL,0)==1);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && s->layout.objectStore.relationship_count==2);
    TEST_ASSERT(Editor_Redo(&s->editor,&s->layout) && !strcmp(s->layout.objectStore.relationships[2].target,cabinet));
    LayoutEntityInfo info=*Layout_EntityInfo(&s->layout.objectStore,"node");snprintf(info.label,96,"Renamed water node");snprintf(info.parent_id,64,"%s",cabinet);
    TEST_ASSERT(Layout_SetEntityInfo(&s->layout,"node",&info,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_MoveAssembly(&s->layout,cabinet,(double[]){.025,0,0},(Vec3){0,0,30},Layout_GeometryHistory,NULL));
    TEST_ASSERT(!strcmp(s->layout.objectStore.relationships[0].source,"node") && !strcmp(s->layout.objectStore.relationships[2].target,cabinet));
    LayoutRelationship r=*Layout_FindRelationship(&s->layout.objectStore,"relationship_1");r.kind=LAYOUT_RELATIONSHIP_CONTAINED_BY;
    TEST_ASSERT(Layout_EditRelationship(&s->layout,&r,NULL,Layout_GeometryHistory,NULL));
    TEST_ASSERT(!strcmp(s->layout.objectStore.relationships[0].id,"relationship_1"));
    size_t count=Editor_UndoCount(&s->editor);TEST_ASSERT(Layout_EditRelationship(&s->layout,&r,NULL,Layout_GeometryHistory,NULL) && Editor_UndoCount(&s->editor)==count);
    ld_test_shutdown_runtime();return true;
}
static bool reject_history(const Layout* layout, void* context) {(void)layout;(void)context;return false;}
static bool test_invalid_edits_cycles_atomic(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(part("A",(Vec3){0}) && part("B",(Vec3){2,0,0}) && part("C",(Vec3){4,0,0}));
    TEST_ASSERT(add("A","B",LAYOUT_RELATIONSHIP_CONTAINED_BY) && add("B","C",LAYOUT_RELATIONSHIP_CONTAINED_BY));
    char* before=Layout_SaveToString(&s->layout);size_t undo=Editor_UndoCount(&s->editor);
    LayoutRelationship invalid[]={edge("C","A",LAYOUT_RELATIONSHIP_CONTAINED_BY),edge("A","A",LAYOUT_RELATIONSHIP_ATTACHED_TO),edge("A","missing",LAYOUT_RELATIONSHIP_SUPPORTED_BY),edge("A","B",LAYOUT_RELATIONSHIP_CONTAINED_BY),edge("A","C",(LayoutRelationshipKind)99)};
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        TEST_ASSERT(!Layout_EditRelationship(&s->layout,&invalid[i],NULL,Layout_GeometryHistory,NULL));
        TEST_ASSERT(same_json(&s->layout,before) && Editor_UndoCount(&s->editor)==undo);
    }
    LayoutRelationship r=edge("A","C",LAYOUT_RELATIONSHIP_ATTACHED_TO);
    TEST_ASSERT(!Layout_EditRelationship(&s->layout,&r,NULL,reject_history,NULL) && same_json(&s->layout,before));
    TEST_ASSERT(!Layout_EditRelationship(&s->layout,NULL,"missing",Layout_GeometryHistory,NULL) && same_json(&s->layout,before));
    snprintf(r.id,64,"unknown_id");TEST_ASSERT(!Layout_EditRelationship(&s->layout,&r,NULL,Layout_GeometryHistory,NULL) && same_json(&s->layout,before));
    Layout_FreeString(before);
    /* Connectivity can be cyclic; containment cannot. Multiple supporting targets are legal. */
    TEST_ASSERT(add("A","B",LAYOUT_RELATIONSHIP_ATTACHED_TO) && add("B","A",LAYOUT_RELATIONSHIP_ATTACHED_TO));
    TEST_ASSERT(add("A","B",LAYOUT_RELATIONSHIP_SUPPORTED_BY) && add("B","A",LAYOUT_RELATIONSHIP_SUPPORTED_BY) && add("A","C",LAYOUT_RELATIONSHIP_SUPPORTED_BY));
    TEST_ASSERT(Layout_ValidateRelationships(&s->layout,NULL,0) && Layout_HasEngineeringData(&s->layout));
    size_t saved_count=s->layout.objectStore.relationship_count;
    s->layout.objectStore.relationship_count=LAYOUT_MAX_RELATIONSHIPS+1;
    LayoutRelationship pending=edge("A","C",LAYOUT_RELATIONSHIP_ATTACHED_TO);
    TEST_ASSERT(!Layout_EditRelationship(&s->layout,&pending,NULL,Layout_GeometryHistory,NULL));
    s->layout.objectStore.relationship_count=saved_count;
    ld_test_shutdown_runtime();return true;
}
static bool test_deletion_guards_explicit_remove_undo(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char cabinet[64];uint32_t handle=part("A",(Vec3){0});TEST_ASSERT(handle && assembly(cabinet));
    TEST_ASSERT(add("A",cabinet,LAYOUT_RELATIONSHIP_CONTAINED_BY));Editor_ClearHistory(&s->editor);
    char* before=Layout_SaveToString(&s->layout);
    s->editor.selectedObject3DId=handle;
    TEST_ASSERT(!UIPanel_SceneListDeleteSelectedObject() && !Editor_UndoCount(&s->editor) && same_json(&s->layout,before));
    TEST_ASSERT(!Layout_ObjectStore_Delete(&s->layout.objectStore,handle));
    TEST_ASSERT(!Layout_EditAssembly(&s->layout,NULL,cabinet,Layout_GeometryHistory,NULL) && same_json(&s->layout,before));
    TEST_ASSERT(Layout_EditRelationship(&s->layout,NULL,"relationship_1",Layout_GeometryHistory,NULL));
    TEST_ASSERT(Layout_EditAssembly(&s->layout,NULL,cabinet,Layout_GeometryHistory,NULL));
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout) && Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(same_json(&s->layout,before) && Layout_FindRelationship(&s->layout.objectStore,"relationship_1"));
    Layout_FreeString(before);ld_test_shutdown_runtime();return true;
}
static bool test_schema_roundtrip_export_malformed(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char cabinet[64],root_assembly[64];TEST_ASSERT(part("A",(Vec3){0}) && part("B",(Vec3){2,0,0}) && assembly(cabinet));
    TEST_ASSERT(add("A","B",LAYOUT_RELATIONSHIP_SUPPORTED_BY) && add("A",cabinet,LAYOUT_RELATIONSHIP_CONTAINED_BY));
    TEST_ASSERT(assembly(root_assembly) && add(cabinet,root_assembly,LAYOUT_RELATIONSHIP_ATTACHED_TO));
    char* json=Layout_SaveToString(&s->layout);TEST_ASSERT(json && Layout_LoadFromString(&s->layout,json));
    for (int bad=0;bad<10;++bad) {
        cJSON* root=cJSON_Parse(json);cJSON* engineering=cJSON_GetObjectItemCaseSensitive(root,"engineering");cJSON* links=cJSON_GetObjectItemCaseSensitive(engineering,"relationships");cJSON* first=cJSON_GetArrayItem(links,0);
        if (bad==0)cJSON_ReplaceItemInObjectCaseSensitive(first,"source",cJSON_CreateString("missing"));
        if (bad==1)cJSON_ReplaceItemInObjectCaseSensitive(first,"target",cJSON_CreateString("A"));
        if (bad==2)cJSON_ReplaceItemInObjectCaseSensitive(first,"type",cJSON_CreateString("unknown"));
        if (bad==3)cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(links,1),"id",cJSON_CreateString("relationship_1"));
        if (bad==4)cJSON_AddItemToArray(links,cJSON_Duplicate(first,1));
        if (bad==5)cJSON_DeleteItemFromObjectCaseSensitive(engineering,"relationships");
        if (bad==6)cJSON_ReplaceItemInObjectCaseSensitive(engineering,"nextRelationshipId",cJSON_CreateNumber(1.5));
        if (bad==7)cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(15));
        if (bad==8)cJSON_ReplaceItemInObjectCaseSensitive(first,"id",cJSON_CreateString("bad id"));
        if (bad==9) {
            cJSON* cycle=cJSON_CreateObject();cJSON_AddStringToObject(cycle,"id","relationship_4");cJSON_AddStringToObject(cycle,"source",cabinet);cJSON_AddStringToObject(cycle,"target","A");cJSON_AddStringToObject(cycle,"type","contained_by");cJSON_AddItemToArray(links,cycle);
        }
        char* broken=cJSON_PrintUnformatted(root);TEST_ASSERT(!Layout_LoadFromString(&s->layout,broken) && same_json(&s->layout,json));cJSON_free(broken);cJSON_Delete(root);
    }
    snprintf(s->layout.objectStore.view_query.entity_type,64,"HideEverything");
    char* authored=LineDrawingCanonicalScene_ExportLayoutToString(&s->layout,"s3_review"),*compiled=NULL;char diagnostics[256];
    TEST_ASSERT(authored && core_scene_compile_authoring_to_runtime(authored,&compiled,diagnostics,sizeof(diagnostics)).code==CORE_OK);
    cJSON* runtime=cJSON_Parse(compiled);const cJSON* objects=cJSON_GetObjectItemCaseSensitive(runtime,"objects");const cJSON* projected=NULL;
    for (int i=0;i<cJSON_GetArraySize(objects);++i) {
        const cJSON* item=cJSON_GetArrayItem(objects,i);const cJSON* id=cJSON_GetObjectItemCaseSensitive(item,"object_id");
        if (cJSON_IsString(id) && !strcmp(id->valuestring,"A")) projected=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(item,"extensions"),"engineering"),"relationships");
    }
    TEST_ASSERT(cJSON_IsArray(projected) && cJSON_GetArraySize(projected)==2 && !strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(projected,1),"target")->valuestring,cabinet));
    const cJSON* snapshot=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(runtime,"extensions"),"line_drawing"),"layout_snapshot");
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(snapshot,"engineering"),"relationships"))==3);
    cJSON_Delete(runtime);free(compiled);free(authored);Layout_FreeString(json);ld_test_shutdown_runtime();return true;
}
static bool test_schema15_migration_and_capacity(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char cabinet[64];TEST_ASSERT(part("A",(Vec3){0}) && assembly(cabinet));
    LayoutEntityInfo info=*Layout_EntityInfo(&s->layout.objectStore,"A");snprintf(info.label,96,"Retained panel");snprintf(info.parent_id,64,"%s",cabinet);TEST_ASSERT(Layout_SetEntityInfo(&s->layout,"A",&info,Layout_GeometryHistory,NULL));
    char* json=Layout_SaveToString(&s->layout);cJSON* root=cJSON_Parse(json);cJSON* engineering=cJSON_GetObjectItemCaseSensitive(root,"engineering");
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion",cJSON_CreateNumber(15));cJSON_DeleteItemFromObjectCaseSensitive(engineering,"relationships");cJSON_DeleteItemFromObjectCaseSensitive(engineering,"nextRelationshipId");char* legacy=cJSON_PrintUnformatted(root);
    TEST_ASSERT(Layout_LoadFromString(&s->layout,legacy) && s->layout.objectStore.assembly_count==1 && !s->layout.objectStore.relationship_count && !strcmp(s->layout.objectStore.items[0].info.label,"Retained panel"));
    cJSON_Delete(root);cJSON_free(legacy);Layout_FreeString(json);Editor_ClearHistory(&s->editor);
    for (int i=0;i<LAYOUT_MAX_RELATIONSHIPS;++i) {
        char id[64];snprintf(id,64,"target_%d",i);TEST_ASSERT(part(id,(Vec3){2,0,0}));LayoutRelationship r=edge("A",id,LAYOUT_RELATIONSHIP_ATTACHED_TO);
        TEST_ASSERT(Layout_EditRelationship(&s->layout,&r,NULL,NULL,NULL));
    }
    TEST_ASSERT(s->layout.objectStore.relationship_count==LAYOUT_MAX_RELATIONSHIPS);
    json=Layout_SaveToString(&s->layout);LayoutRelationship r=edge("A",cabinet,LAYOUT_RELATIONSHIP_SUPPORTED_BY);
    TEST_ASSERT(!Layout_EditRelationship(&s->layout,&r,NULL,Layout_GeometryHistory,NULL) && same_json(&s->layout,json));
    TEST_ASSERT(Layout_EditRelationship(&s->layout,NULL,"relationship_1",NULL,NULL));TEST_ASSERT(Layout_EditRelationship(&s->layout,&r,NULL,NULL,NULL));
    TEST_ASSERT(!strcmp(s->layout.objectStore.relationships[LAYOUT_MAX_RELATIONSHIPS-1].id,"relationship_129"));
    Layout_FreeString(json);ld_test_shutdown_runtime();return true;
}
static bool click(int action) {
    SDL_Rect rect;TEST_ASSERT(UIPanel_PartsControlRect(action,&rect));TEST_ASSERT(rect.y>=UIPanel_Get()->rightBodyRect.y && rect.y+rect.h<=UIPanel_Get()->rightBodyRect.y+UIPanel_Get()->rightBodyRect.h);
    AppContext context={0};SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=rect.x+rect.w/2;e.button.y=rect.y+rect.h/2;Input_Handle(&context,&e);return true;
}
static bool test_mouse_links_form_history_incoming_filter(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();char cabinet[64];uint32_t handle=part("A",(Vec3){0});TEST_ASSERT(handle && part("B",(Vec3){2,0,0}) && assembly(cabinet));
    Global_SetWindowSize(1400,1400);UIPanel_OnWindowResized(1400,1400);TEST_ASSERT(UIPanel_PartsSelectEntity("A"));Editor_ClearHistory(&s->editor);
    TEST_ASSERT(click(PARTS_LINKS) && !strcmp(UIPanel_Get()->parts.link.source,"A"));
    TEST_ASSERT(click(PARTS_LINK_TYPE) && click(1001));TEST_ASSERT(click(PARTS_LINK_TARGET) && click(1002));
    TEST_ASSERT(click(PARTS_LINK_SAVE) && s->layout.objectStore.relationship_count==1 && Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(s->layout.objectStore.relationships[0].kind==LAYOUT_RELATIONSHIP_SUPPORTED_BY && !strcmp(s->layout.objectStore.relationships[0].target,cabinet));
    TEST_ASSERT(click(7000) && click(PARTS_LINK_TYPE) && click(1000) && click(PARTS_LINK_SAVE));
    TEST_ASSERT(!strcmp(s->layout.objectStore.relationships[0].id,"relationship_1") && s->layout.objectStore.relationships[0].kind==LAYOUT_RELATIONSHIP_ATTACHED_TO);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));UIPanel_LayoutParts();TEST_ASSERT(UIPanel_Get()->parts.link.kind==LAYOUT_RELATIONSHIP_SUPPORTED_BY);
    TEST_ASSERT(click(PARTS_LINK_REMOVE) && click(PARTS_LINK_CANCEL) && s->layout.objectStore.relationship_count==1);
    TEST_ASSERT(click(PARTS_LINK_REMOVE) && click(PARTS_LINK_CONFIRM_REMOVE) && !s->layout.objectStore.relationship_count);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));UIPanel_LayoutParts();TEST_ASSERT(click(7000));
    TEST_ASSERT(click(PARTS_LINK_SOURCE) && click(1002) && !s->editor.selectedObject3DId); /* Inspect incoming support to the assembly. */
    TEST_ASSERT(click(7000) && !strcmp(UIPanel_Get()->parts.link.source,"A") && s->editor.selectedObject3DId==handle);
    snprintf(s->layout.objectStore.view_query.entity_type,64,"Hidden");
    TEST_ASSERT(click(PARTS_LINK_TARGET) && click(1001) && click(PARTS_LINK_SAVE));
    TEST_ASSERT(!strcmp(s->layout.objectStore.relationships[0].target,"B"));
    TEST_ASSERT(click(PARTS_LINK_TYPE));SDL_Event escape={.type=SDL_KEYDOWN};escape.key.keysym.sym=SDLK_ESCAPE;TEST_ASSERT(UIPanel_PartsEvent(&escape) && !UIPanel_Get()->parts.chooser);
    ld_test_shutdown_runtime();return true;
}
bool relationships_run_tests(void) {
    const TestCase cases[]={
        {"directed_queries_history_identity",test_directed_queries_history_identity},
        {"invalid_edits_cycles_atomic",test_invalid_edits_cycles_atomic},
        {"deletion_guards_remove_undo",test_deletion_guards_explicit_remove_undo},
        {"schema_export_malformed",test_schema_roundtrip_export_malformed},
        {"schema15_migration_capacity",test_schema15_migration_and_capacity},
        {"mouse_links_incoming_filter",test_mouse_links_form_history_incoming_filter}
    };
    return run_test_cases("Relationships",cases,sizeof(cases)/sizeof(cases[0]));
}
