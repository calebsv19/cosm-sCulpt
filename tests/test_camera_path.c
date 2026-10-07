#include "test_layout_internal.h"
#include "Core/camera_path.h"
#include "Layout/scene/layout_camera_poses.h"
#include "Layout/scene/layout_scene_path_edit.h"
#include "UI/ui_panel_camera.h"
#include "UI/ui_panel_camera_path.h"
#include "UI/ui_panel_right_scroll.h"
#include "UI/ui_panel_view_layout.h"
#include "Tools/scene_authoring_import.h"
#include "core_scene_compile.h"

static bool legacy_anchor_pose_roll_endpoint_and_linear_identity(void) {
    LineDrawingScenePath p={.control_point_count=4,.duration_seconds=4};
    snprintf(p.curve_type,sizeof(p.curve_type),"linear");
    for(int i=0;i<4;++i)p.control_points[i]=(Vec3){0,(float)i,1};
    LineDrawingSceneCamera c;Layout_SceneCamera_SetDefaults(&c,"c","c","p");c.roll_degrees=25;
    TEST_ASSERT(Layout_ScenePathEdit_AnchorCount(&p)==4);
    LineDrawingSceneCameraPose pose;
    float fov;
    TEST_ASSERT(CameraPoses_Anchor(&p,&c,3,&pose,&fov));
    TEST_ASSERT(ld_test_vec3_nearly_equal(pose.position,p.control_points[3]));
    TEST_ASSERT(Vec3_Dot(pose.forward,(Vec3){0,1,0})>.9999);
    TEST_ASSERT(fabsf(pose.up.x-sinf(DegToRad(25)))<.0001);
    TEST_ASSERT(!p.key_count);
    return true;
}
static bool orientation_shortest_arc_and_stationary_turn(void) {
    LineDrawingScenePath p={.control_point_count=2,.duration_seconds=4};
    snprintf(p.curve_type,sizeof(p.curve_type),"linear");
    p.control_points[0]=p.control_points[1]=(Vec3){1,2,3};
    LineDrawingSceneCamera c;Layout_SceneCamera_SetDefaults(&c,"c","c","p");
    TEST_ASSERT(CameraPoses_Enable(&p,&c));
    for(int i=0;i<2;++i) {
        float yaw=DegToRad(i ? -179 : 179);
        CameraPoses_SetRotation(&p.keys[i],(Vec3){cosf(yaw),sinf(yaw),0},(Vec3){0,0,1});
        p.keys[i].seconds=2;p.keys[i].fov=i ? 90 : 50;
    }
    LineDrawingSceneCameraPose pose;float fov;
    TEST_ASSERT(CameraPoses_Time(&p,&c,1,&pose,&fov));
    TEST_ASSERT(Vec3_Dot(pose.forward,(Vec3){-1,0,0})>.9999 && fabsf(fov-70)<.0001);
    TEST_ASSERT(ld_test_vec3_nearly_equal(pose.position,(Vec3){1,2,3}));
    p.playback_mode=LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP;
    TEST_ASSERT(CameraPoses_Time(&p,&c,2.5,&pose,&fov));
    TEST_ASSERT(fabsf(fov-60)<.0001);
    cJSON *json=CameraPoses_ToJson(&p);TEST_ASSERT(json);
    LineDrawingScenePath copy=p;copy.key_count=0;
    TEST_ASSERT(CameraPoses_FromJson(&copy,json));
    TEST_ASSERT(!memcmp(copy.keys,p.keys,sizeof(p.keys)));
    cJSON_ReplaceItemInObject(cJSON_GetArrayItem(cJSON_GetObjectItem(json,"keys"),1),"id",cJSON_CreateString(p.keys[0].id));
    LineDrawingScenePath before=copy;
    TEST_ASSERT(!CameraPoses_FromJson(&copy,json) && !memcmp(&copy,&before,sizeof(copy)));
    cJSON_Delete(json);return true;
}
static bool cubic_sequence_keeps_handles_keys_and_capacity(void) {
    LineDrawingSceneAuthoringState a;Layout_SceneAuthoringState_Init(&a);
    LineDrawingScenePath *p=&a.paths[0];
    LineDrawingSceneCamera *c=Layout_SceneAuthoringState_FindCameraForPath(&a,p);
    TEST_ASSERT(CameraPoses_Enable(p,c));
    LineDrawingCameraKey key=p->keys[0];key.fov=80;
    TEST_ASSERT(CameraPoses_Add(p,0,(Vec3){0,0,1},&key));
    TEST_ASSERT(p->key_count==3 && p->control_point_count==7);
    char id[64];snprintf(id,sizeof(id),"%s",p->keys[1].id);
    Vec3 offset=Vec3_Sub(p->control_points[2],p->control_points[3]);
    TEST_ASSERT(CameraPoses_Move(p,1,2));
    TEST_ASSERT(!strcmp(id,p->keys[2].id));
    TEST_ASSERT(ld_test_vec3_nearly_equal(Vec3_Sub(p->control_points[5],p->control_points[6]),offset));
    TEST_ASSERT(CameraPoses_Remove(p,0) && p->key_count==2 && CameraPoses_Valid(p));
    TEST_ASSERT(CameraPoses_Remove(p,0) && p->key_count==1 && p->control_point_count==1);
    TEST_ASSERT(!CameraPoses_Remove(p,0));
    while(p->key_count<16) TEST_ASSERT(CameraPoses_Add(p,p->key_count-1,(Vec3){0,0,1},&key));
    LineDrawingScenePath before=*p;
    TEST_ASSERT(!CameraPoses_Add(p,15,(Vec3){0,0,1},&key));
    TEST_ASSERT(!memcmp(p,&before,sizeof(before)));
    return true;
}
static bool van_draft_transactions_roundtrip_and_export(void) {
    ld_test_init_runtime();GlobalState *s=Global_Get();
    TEST_ASSERT(Layout_LoadFromFile(&s->layout,"config/examples/van_construction_f4.layout.json"));
    Editor_ClearHistory(&s->editor);
    char *original=Layout_SaveToString(&s->layout);TEST_ASSERT(original);
    LineDrawingScenePath exterior=s->layout.sceneAuthoring.paths[0];
    TEST_ASSERT(CameraPath_SelectPoint(s,1) && CameraPath_ViewPoint(s,1));
    TEST_ASSERT(ld_test_vec3_nearly_equal(s->cameraView.eye,exterior.control_points[3]));
    char *viewed=Layout_SaveToString(&s->layout);TEST_ASSERT(!strcmp(original,viewed));free(viewed);
    TEST_ASSERT(CameraView_Enter(s,false));
    s->cameraView.eye=(Vec3){0,.8f,1.6f};s->cameraView.pitch=-8;
    TEST_ASSERT(CameraPath_Add(s,true));
    const LineDrawingScenePath *p=CameraPath_Selected(s);
    TEST_ASSERT(p && p->key_count==1 && Editor_UndoCount(&s->editor)==1);
    char path_id[64];snprintf(path_id,sizeof(path_id),"%s",p->path_id);
    s->cameraView.eye=(Vec3){.3f,0,1.4f};s->cameraView.yaw=179;s->cameraView.roll=23;s->cameraView.fov=78;
    CameraPath_NavigationChanged(s);
    TEST_ASSERT(!CameraPath_TogglePlay(s));
    TEST_ASSERT(CameraPath_Add(s,false));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==2);
    p=CameraPath_Selected(s);TEST_ASSERT(p->key_count==2);
    Vec3 eye=s->cameraView.eye,forward,up;CameraView_Basis(&s->cameraView,&forward,NULL,&up);
    TEST_ASSERT(CameraPath_ViewPoint(s,1));
    TEST_ASSERT(ld_test_vec3_nearly_equal(s->cameraView.eye,eye));
    Vec3 f,u;CameraView_Basis(&s->cameraView,&f,NULL,&u);
    TEST_ASSERT(Vec3_Dot(f,forward)>.9999 && Vec3_Dot(u,up)>.9999);
    s->cameraView.eye.x+=.1f;CameraPath_NavigationChanged(s);
    TEST_ASSERT(!CameraPath_Scrub(s,0));
    TEST_ASSERT(CameraPath_Apply(s));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==3);
    TEST_ASSERT(CameraPath_SetSeconds(s,3));
    TEST_ASSERT(CameraPath_Move(s,-1));
    TEST_ASSERT(CameraPath_SelectedAnchor(s)==0);
    size_t undo=Editor_UndoCount(&s->editor);
    TEST_ASSERT(CameraPath_Scrub(s,0) && CameraPath_TogglePlay(s));
    TEST_ASSERT(CameraView_NextUpdateDelayMs(s)==16);
    char *pre=Layout_SaveToString(&s->layout);
    for(int i=0;i<100;++i)(void)CameraPath_Tick(s,.05f);
    TEST_ASSERT(!s->cameraView.path_playing && CameraView_NextUpdateDelayMs(s)==-1);
    char *post=Layout_SaveToString(&s->layout);
    TEST_ASSERT(pre && post && !strcmp(pre,post) && Editor_UndoCount(&s->editor)==undo);
    free(pre);free(post);
    TEST_ASSERT(!memcmp(&exterior,&s->layout.sceneAuthoring.paths[0],sizeof(exterior)));
    char *saved=Layout_SaveToString(&s->layout);TEST_ASSERT(saved);
    cJSON *before=cJSON_Parse(original),*after=cJSON_Parse(saved);
    const char *unchanged[]={"objects3d","engineering","geometricConstraints","physicalContext","scene3d","anchors","walls"};
    for(size_t i=0;i<sizeof(unchanged)/sizeof(unchanged[0]);++i)
        TEST_ASSERT(cJSON_Compare(cJSON_GetObjectItem(before,unchanged[i]),cJSON_GetObjectItem(after,unchanged[i]),true));
    cJSON_Delete(before);cJSON_Delete(after);
    Layout loaded;Layout_Init(&loaded,1);TEST_ASSERT(Layout_LoadFromString(&loaded,saved));
    const LineDrawingScenePath *lp=Layout_SceneAuthoringState_FindPathByIdConst(&loaded.sceneAuthoring,path_id);
    TEST_ASSERT(lp && CameraPoses_Valid(lp) && !lp->playing);
    char *exported=LineDrawingCanonicalScene_ExportLayoutToString(&s->layout,"camera_path_van");TEST_ASSERT(exported);
    char *runtime=NULL,diag[512];
    TEST_ASSERT(core_scene_compile_authoring_to_runtime(exported,&runtime,diag,sizeof(diag)).code==CORE_OK);
    cJSON *runtime_json=cJSON_Parse(runtime);
    const cJSON *paths=cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(runtime_json,"extensions"),"line_drawing"),"camera_paths_v1");
    TEST_ASSERT(cJSON_IsArray(paths) && cJSON_GetArraySize(paths)==3);
    LineDrawingSceneAuthoringState imported;bool records=false;
    TEST_ASSERT(LineDrawingSceneAuthoringImport_ParseCanonical(runtime_json,&imported,&records) && records);
    lp=Layout_SceneAuthoringState_FindPathByIdConst(&imported,path_id);
    TEST_ASSERT(lp && CameraPoses_Valid(lp));
    const LineDrawingSceneCamera *camera=Layout_SceneAuthoringState_FindCameraForPathConst(&imported,lp);
    LineDrawingSceneCameraPose pose;float fov;
    TEST_ASSERT(CameraPoses_Anchor(lp,camera,0,&pose,&fov));
    TEST_ASSERT(fabsf(fov-78)<.0001 && fabsf(pose.position.x-.4f)<.0001);
    cJSON_Delete(runtime_json);free(runtime);free(exported);free(saved);free(original);Layout_Free(&loaded);
    TEST_ASSERT(CameraPath_ViewPoint(s,0));
    s->cameraView.eye.x+=.1;CameraPath_NavigationChanged(s);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));
    TEST_ASSERT(!CameraPath_Apply(s)); /* intervening history cannot overwrite a new target */
    TEST_ASSERT(s->cameraView.path_modified);
    ld_test_shutdown_runtime();return true;
}
static bool responsive_controls_focus_pause_and_history(void) {
    ld_test_init_runtime();GlobalState *s=Global_Get();
    TEST_ASSERT(CameraPath_ViewPoint(s,0));
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(1100,900);
    SDL_Rect r;
    TEST_ASSERT(UIPanel_CameraControlRect(UI_PATH_VIEW,&r));
    int wide=r.w;
    UIPanel_Get()->rightBodyRect.w=110;
    UIPanel_UpdateViewPaneLayout(UIPanel_Get());
    TEST_ASSERT(UIPanel_CameraControlRect(UI_PATH_VIEW,&r));
    TEST_ASSERT(r.w!=wide && r.x+r.w<=UIPanel_Get()->rightBodyRect.x+110);
    TEST_ASSERT(UIPanel_CameraControlRect(UI_PATH_REVERT,&r));
    SDL_Rect view;TEST_ASSERT(UIPanel_CameraControlRect(UI_PATH_VIEW,&view));
    TEST_ASSERT(r.y>view.y);
    TEST_ASSERT(CameraPath_TogglePlay(s));
    SDL_Event e={.type=SDL_WINDOWEVENT};e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    (void)CameraView_HandleEvent(s,&e);
    TEST_ASSERT(!s->cameraView.path_playing && CameraView_NextUpdateDelayMs(s)==-1);
    ld_test_shutdown_runtime();return true;
}
bool camera_path_run_tests(void) {
    const TestCase cases[]={
        {"legacy_anchor_roll_endpoint_linear",legacy_anchor_pose_roll_endpoint_and_linear_identity},
        {"shortest_arc_stationary_turn_json_refusal",orientation_shortest_arc_and_stationary_turn},
        {"cubic_sequence_identity_handles_capacity",cubic_sequence_keeps_handles_keys_and_capacity},
        {"full_van_transactions_roundtrip_compiled_export",van_draft_transactions_roundtrip_and_export},
        {"responsive_visible_controls_focus_idle",responsive_controls_focus_pause_and_history}};
    return run_test_cases("CameraPath",cases,sizeof(cases)/sizeof(cases[0]));
}
