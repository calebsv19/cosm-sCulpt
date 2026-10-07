#include "Core/camera_path_proof.h"
#include "Core/camera_path.h"
#include "Core/global_state.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_camera.h"
#include "UI/ui_panel_camera_path.h"
#include "UI/ui_panel_right_scroll.h"
#include "UI/ui_panel_view_layout.h"
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

bool CameraPathProof_Prepare(const char *mode) {
    GlobalState *s=Global_Get();
    if(s->layout.objectStore.count!=130 || !CameraPath_Add(s,true))return false;
    s->cameraView.eye=(Vec3){.2f,.2f,1.5f};s->cameraView.yaw=0;s->cameraView.pitch=-8;
    CameraPath_NavigationChanged(s);
    if(!CameraPath_Add(s,false))return false;
    s->cameraView.eye=(Vec3){-.2f,-.7f,1.5f};s->cameraView.yaw=180;s->cameraView.roll=8;
    CameraPath_NavigationChanged(s);
    if(!CameraPath_Add(s,false) || !CameraPath_ViewPoint(s,0))return false;
    if(!strcmp(mode,"camera-path-narrow")) {
        UIPanel_Get()->rightBodyRect.w=110;
        UIPanel_UpdateViewPaneLayout(UIPanel_Get());
    }
    if(!strcmp(mode,"camera-path-actions")) {
        SDL_Rect r;
        if(!UIPanel_CameraControlRect(UI_PATH_ACTIONS,&r) || !UIPanel_CameraClick(r.x+2,r.y+2))return false;
    }
    s->inspectionView=true;
    return true;
}
bool CameraPathProof_Check(void) {
    GlobalState *s=Global_Get();
    if(s->layout.objectStore.count!=130 || s->cameraView.path_playing ||
       CameraView_NextUpdateDelayMs(s)!=-1 || Layout_MeshSolidPreviewNextUpdateDelayMs()!=-1)return false;
    SDL_Rect r;
    const int actions[]={UI_PATH_VIEW,UI_PATH_APPLY,UI_PATH_ADD,UI_PATH_PLAY,UI_PATH_SCRUB};
    for(size_t i=0;i<sizeof(actions)/sizeof(actions[0]);++i)
        if(!UIPanel_CameraControlRect(actions[i],&r) || r.w<=0 ||
           r.x<UIPanel_Get()->viewPane.summaryRect.x ||
           r.x+r.w>UIPanel_Get()->viewPane.summaryRect.x+UIPanel_Get()->viewPane.summaryRect.w)return false;
    fprintf(stderr,"C3 native van: 130 objects; point view, controls and settled preview passed\n");
    return true;
}
static const AppCallbacks *original;
static Uint32 idle_start;
static unsigned renders,updates;
static double cpu_seconds(void) {
    struct rusage r;
    if(getrusage(RUSAGE_SELF,&r))return 0;
    return r.ru_utime.tv_sec+r.ru_utime.tv_usec*1e-6+r.ru_stime.tv_sec+r.ru_stime.tv_usec*1e-6;
}
static Uint32 quit_timer(Uint32 interval,void *context) {
    (void)interval;(void)context;
    SDL_Event e={.type=SDL_USEREVENT};e.user.code=0x4333;
    (void)SDL_PushEvent(&e);return 0;
}
static void idle_input(AppContext *app,SDL_Event *e) {
    if(e->type==SDL_USEREVENT && e->user.code==0x4333)app->quit=true;
    else if(original->handleInput)original->handleInput(app,e);
}
static void idle_update(AppContext *app) {
    if(SDL_GetTicks()-idle_start>1000)++updates;
    original->handleUpdate(app);
}
static void idle_render(AppContext *app) {
    if(SDL_GetTicks()-idle_start>1000)++renders;
    original->handleRender(app);
}
bool CameraPathProof_Idle(AppContext *app,const AppCallbacks *callbacks) {
    original=callbacks;idle_start=SDL_GetTicks();renders=0;updates=0;
    double cpu_start=cpu_seconds();
    SDL_TimerID timer=SDL_AddTimer(3500,quit_timer,NULL);
    if(!timer)return false;
    AppCallbacks proof={.handleInput=idle_input,.handleUpdate=idle_update,.handleRender=idle_render,
        .nextUpdateDelayMs=callbacks->nextUpdateDelayMs};
    App_Run(app,&proof);
    (void)SDL_RemoveTimer(timer);
    double wall=(SDL_GetTicks()-idle_start)*.001,cpu=cpu_seconds()-cpu_start;
    fprintf(stderr,"C3 actual demand loop: wall=%.3fs cpu=%.3fs settled_renders=%u settled_updates=%u\n",wall,cpu,renders,updates);
    return wall>=3 && renders<=2 && callbacks->nextUpdateDelayMs(app)==-1 && CameraPathProof_Check();
}
