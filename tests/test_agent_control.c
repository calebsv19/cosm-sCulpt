#define _POSIX_C_SOURCE 200809L
#include "test_layout_internal.h"
#include "Core/agent_control.h"
#include "Layout/layout_scene_collections.h"
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>

static void enter_stub(void) {}
static cJSON* command(AppContext* app,const char* op,const char* id,const char* rev,const char* path) {
    cJSON* q=cJSON_CreateObject();cJSON_AddStringToObject(q,"operation",op);
    if(id)cJSON_AddStringToObject(q,"request_id",id);
    if(rev)cJSON_AddStringToObject(q,"expected_revision",rev);
    if(path)cJSON_AddStringToObject(q,"path",path);
    char* data=cJSON_PrintUnformatted(q);char* reply=SculptAgent_Command(app,data);
    cJSON* result=reply?cJSON_Parse(reply):NULL;free(data);free(reply);cJSON_Delete(q);return result;
}
static bool guards_and_snapshot(void) {
    ld_test_init_runtime();AppContext app={0};SculptAgent_SetEnterEditor(enter_stub);
    cJSON* r=command(&app,"state",NULL,NULL,NULL);TEST_ASSERT(r);
    char rev[32];snprintf(rev,32,"%s",cJSON_GetObjectItem(r,"revision")->valuestring);cJSON_Delete(r);
    r=command(&app,"open","unit-stale","wrong","/missing.layout.json");TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok")));cJSON_Delete(r);
    Global_FlagLayoutChanged();
    r=command(&app,"open","unit-dirty",rev,"/missing.layout.json");TEST_ASSERT(strstr(cJSON_GetObjectItem(r,"error")->valuestring,"unsaved"));cJSON_Delete(r);
    r=command(&app,"quit","unit-quit-dirty",rev,NULL);TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok"))&&!app.quit);cJSON_Delete(r);
    char path[1024];const char* temp=getenv("TMPDIR");snprintf(path,sizeof(path),"%s/sculpt-snapshot-XXXXXX",temp?temp:"/tmp");
    int fd=mkstemp(path);TEST_ASSERT(fd>=0);close(fd);unlink(path);
    pid_t writer = fork();
    TEST_ASSERT(writer >= 0);
    if (writer == 0) {
        struct rlimit limit = {0, 0};
        signal(SIGXFSZ, SIG_DFL);
        if (setrlimit(RLIMIT_FSIZE, &limit) != 0) _exit(2);
        (void)command(&app, "save_as", "unit-interrupt", rev, path);
        _exit(1);
    }
    int status;
    TEST_ASSERT(waitpid(writer, &status, 0) == writer && WIFSIGNALED(status) &&
                WTERMSIG(status) == SIGXFSZ && access(path, F_OK) != 0);
    r=command(&app,"snapshot","unit-snapshot",rev,path);TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(r,"ok")));cJSON_Delete(r);
    TEST_ASSERT(Global_IsLayoutDirty());
    Layout check={0};Layout_Init(&check,.05f);TEST_ASSERT(Layout_LoadFromFile(&check,path));Layout_Free(&check);
    r=command(&app,"snapshot","unit-snapshot",rev,path);TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(r,"ok")));cJSON_Delete(r);
    r=command(&app,"snapshot","unit-overwrite",rev,path);TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok")));cJSON_Delete(r);unlink(path);
    TEST_ASSERT(!app.quit);SculptAgent_SetEnterEditor(NULL);ld_test_shutdown_runtime();return true;
}
static bool malformed_camera_is_atomic(void) {
    ld_test_init_runtime();AppContext app={0};
    cJSON* r=command(&app,"state",NULL,NULL,NULL);char rev[32];snprintf(rev,32,"%s",cJSON_GetObjectItem(r,"revision")->valuestring);cJSON_Delete(r);
    char request[512];snprintf(request,sizeof(request),"{\"operation\":\"view\",\"request_id\":\"unit-invalid-eye\",\"expected_revision\":\"%s\",\"view_id\":\"missing\",\"eye_m\":[0,0],\"target_m\":[0,0,0]}",rev);
    char* before=Layout_SaveToString(&Global_Get()->layout);CameraViewSession camera=Global_Get()->cameraView;
    char* reply=SculptAgent_Command(&app,request);r=cJSON_Parse(reply);TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok")));cJSON_Delete(r);free(reply);
    char* after=Layout_SaveToString(&Global_Get()->layout);TEST_ASSERT(!strcmp(before,after));TEST_ASSERT(!memcmp(&camera,&Global_Get()->cameraView,sizeof(camera)));free(before);free(after);
    ld_test_shutdown_runtime();return true;
}
static cJSON* view(AppContext* app,const char* rev,const char* name,const char* id) {
    char input[512];
    snprintf(input,sizeof(input),"{\"operation\":\"view\",\"expected_revision\":\"%s\",\"view_id\":\"%s\",\"request_id\":\"%s\"}",rev,name,id);
    char* wire=SculptAgent_Command(app,input);
    cJSON* reply=cJSON_Parse(wire);
    free(wire);
    return reply;
}

static bool working_view_owner(void) {
    ld_test_init_runtime();
    AppContext app={0};
    GlobalState* g=Global_Get();
    LayoutWorkingView working={0};
    snprintf(working.id,sizeof(working.id),"fixture");
    snprintf(working.name,sizeof(working.name),"Fixture");
    TEST_ASSERT(Layout_CaptureWorkingView(&g->layout.objectStore,&working,true));
    TEST_ASSERT(Layout_SaveWorkingView(&g->layout,&working,NULL,NULL,NULL));
    cJSON* state=command(&app,"state",NULL,NULL,NULL);
    char rev[32];snprintf(rev,sizeof(rev),"%s",cJSON_GetObjectItem(state,"revision")->valuestring);
    cJSON_Delete(state);
    g->inspectionView=true;g->authoringGuides=false;
    CameraViewSession original=g->cameraView;
    cJSON* reply=view(&app,rev,"fixture","empty-scene");
    TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(reply,"ok")));
    TEST_ASSERT(!memcmp(&g->cameraView,&original,sizeof(original)) && g->inspectionView && !g->authoringGuides && !app.redraw_requested);
    cJSON_Delete(reply);
    uint32_t body;
    RectPrismPrimitiveCreateParams params;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&g->layout,&params,&body,NULL));
    state=command(&app,"state",NULL,NULL,NULL);
    snprintf(rev,sizeof(rev),"%s",cJSON_GetObjectItem(state,"revision")->valuestring);cJSON_Delete(state);
    size_t undo=Editor_UndoCount(&g->editor);bool dirty=Global_IsLayoutDirty();
    char* before=Layout_SaveToString(&g->layout);TEST_ASSERT(before);
    reply=view(&app,rev,"fixture","named-view");
    TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(reply,"ok")) && g->authoringGuides && g->cameraView.active && app.redraw_requested);
    TEST_ASSERT(!strcmp(rev,cJSON_GetObjectItem(reply,"revision")->valuestring));
    cJSON_Delete(reply);
    char* after=Layout_SaveToString(&g->layout);
    TEST_ASSERT(after && !strcmp(before,after) && Editor_UndoCount(&g->editor)==undo && Global_IsLayoutDirty()==dirty);
    free(before);free(after);
    reply=view(&app,rev,"other","named-view");
    TEST_ASSERT(strstr(cJSON_GetObjectItem(reply,"error")->valuestring,"reused"));cJSON_Delete(reply);
    reply=view(&app,rev,"other","unknown-view");
    TEST_ASSERT(strstr(cJSON_GetObjectItem(reply,"error")->valuestring,"unknown"));cJSON_Delete(reply);
    Global_OnLayoutLoaded(NULL);
    state=command(&app,"state",NULL,NULL,NULL);
    const char* fresh=cJSON_GetObjectItem(state,"revision")->valuestring;
    TEST_ASSERT(strcmp(fresh,rev) && !g->cameraView.active);
    reply=view(&app,rev,"fixture","late-command");
    TEST_ASSERT(strstr(cJSON_GetObjectItem(reply,"error")->valuestring,"revision") && !g->cameraView.active);cJSON_Delete(reply);
    /* Retained success is historical. It must not enter the new camera. */
    reply=view(&app,rev,"fixture","named-view");
    TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(reply,"ok")) && !g->cameraView.active);cJSON_Delete(reply);
    for(int i=0;i<8;i++){
        char id[32];snprintf(id,sizeof(id),"new-view-%d",i);
        reply=view(&app,fresh,"fixture",id);
        TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(reply,"ok")));cJSON_Delete(reply);
    }
    reply=view(&app,rev,"fixture","named-view");
    TEST_ASSERT(strstr(cJSON_GetObjectItem(reply,"error")->valuestring,"revision"));cJSON_Delete(reply);
    cJSON_Delete(state);ld_test_shutdown_runtime();return true;
}
bool agent_control_run_tests(void) {
    const TestCase tests[]={{"dirty_revision_overwrite_retry_guards",guards_and_snapshot},{"invalid_camera_atomic_refusal",malformed_camera_is_atomic},{"working_view_and_replacement_ownership",working_view_owner}};
    return run_test_cases("AgentControl",tests,sizeof(tests)/sizeof(tests[0]));
}
