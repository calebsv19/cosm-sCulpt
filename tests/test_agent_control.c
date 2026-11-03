#define _POSIX_C_SOURCE 200809L
#include "test_layout_internal.h"
#include "Core/agent_control.h"
#include <unistd.h>

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
bool agent_control_run_tests(void) {
    const TestCase tests[]={{"dirty_revision_overwrite_retry_guards",guards_and_snapshot},{"invalid_camera_atomic_refusal",malformed_camera_is_atomic}};
    return run_test_cases("AgentControl",tests,sizeof(tests)/sizeof(tests[0]));
}
