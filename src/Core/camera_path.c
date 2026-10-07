#include "Core/camera_path.h"
#include "Core/global_state.h"
#include "Layout/scene/layout_camera_poses.h"
#include "Layout/scene/layout_scene_path_edit.h"
#include "Editor/scene_authoring_path_handles.h"
#include <stdio.h>
#include <string.h>

const LineDrawingScenePath *CameraPath_Selected(const GlobalState *s) {
    if(!s) return NULL;
    const LineDrawingSceneAuthoringState *a=&s->layout.sceneAuthoring;
    if(a->selected_kind==LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH && a->selected_index<a->path_count &&
       a->paths[a->selected_index].role==LINE_DRAWING_SCENE_PATH_ROLE_CAMERA) return &a->paths[a->selected_index];
    if(s->cameraView.selected>=a->camera_count) return NULL;
    return Layout_SceneAuthoringState_FindPathByIdConst(a,a->cameras[s->cameraView.selected].path_id);
}
size_t CameraPath_SelectedAnchor(const GlobalState *s) {
    const LineDrawingScenePath *p=CameraPath_Selected(s);
    if(!p) return 0;
    const EditorState *e=&s->editor;
    if(e->selectedSceneAuthoringPathIndex>=0 && (size_t)e->selectedSceneAuthoringPathIndex==
       (size_t)(p-s->layout.sceneAuthoring.paths) && e->selectedSceneAuthoringControlPointIndex>=0) {
        size_t a=Layout_ScenePathEdit_ElementForControl(p,(size_t)e->selectedSceneAuthoringControlPointIndex).anchor_index;
        if(a<Layout_ScenePathEdit_AnchorCount(p)) return a;
    }
    return !strcmp(p->path_id,s->cameraView.path_source) && s->cameraView.path_anchor<Layout_ScenePathEdit_AnchorCount(p)
        ? s->cameraView.path_anchor : 0;
}
bool CameraPath_SelectPoint(GlobalState *s,size_t anchor) {
    const LineDrawingScenePath *p=CameraPath_Selected(s);
    if(!p || anchor>=Layout_ScenePathEdit_AnchorCount(p)) return false;
    size_t pi=(size_t)(p-s->layout.sceneAuthoring.paths),control=CameraPoses_Control(p,anchor);
    LineDrawingScenePathElementRef element=Layout_ScenePathEdit_ElementForControl(p,control);
    SceneAuthoringPathHandles_Select(&s->editor,(SceneAuthoringPathHandleRef){
        .kind=SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,.path_index=pi,.control_index=control,
        .element_kind=element.kind,.segment_index=element.segment_index});
    Global_FlagHitboxesDirty(); return true;
}
static uint64_t signature(const LineDrawingScenePath *p,const LineDrawingSceneCamera *camera) {
    LineDrawingScenePath copy=*p; copy.playing=false; copy.normalized_distance=0;
    uint64_t h=UINT64_C(1469598103934665603);
    const unsigned char *b=(const unsigned char *)&copy;
    for(size_t i=0;i<sizeof(copy);++i)h=(h^b[i])*UINT64_C(1099511628211);
    b=(const unsigned char *)camera;
    for(size_t i=0;i<sizeof(*camera);++i)h=(h^b[i])*UINT64_C(1099511628211);
    return h;
}
static void set_pose(CameraViewSession *v,const LineDrawingSceneCameraPose *pose,float fov) {
    v->eye=pose->position;
    v->yaw=RadToDeg(atan2f(pose->forward.y,pose->forward.x));
    v->pitch=RadToDeg(asinf(fmaxf(-1,fminf(1,pose->forward.z))));
    FreeViewCamera basis={.enabled=true,.yawDeg=v->yaw,.pitchDeg=v->pitch};
    v->roll=RadToDeg(atan2f(Vec3_Dot(pose->up,FreeView_Right(&basis)),Vec3_Dot(pose->up,FreeView_Up(&basis))));
    v->fov=fov;
}
static bool enter(GlobalState *s,const LineDrawingScenePath *p,const LineDrawingSceneCamera *camera,
                  const LineDrawingSceneCameraPose *pose,float fov) {
    if(!s->cameraView.active && !CameraView_Enter(s,false)) return false;
    CameraViewSession *v=&s->cameraView;
    set_pose(v,pose,fov); v->near_clip=camera->near_clip; v->far_clip=camera->far_clip;
    snprintf(v->source_id,sizeof(v->source_id),"%s",camera->camera_id);
    snprintf(v->path_source,sizeof(v->path_source),"%s",p->path_id);
    v->path_signature=signature(p,camera); v->path_modified=false;
    CameraView_ResetInput(s); Global_FlagHitboxesDirty(); return true;
}
static bool draft_guard(GlobalState *s) {
    if(!s->cameraView.path_modified) return true;
    snprintf(s->cameraView.message,sizeof(s->cameraView.message),"Apply or Revert the draft first.");
    return false;
}
bool CameraPath_ViewPoint(GlobalState *s,size_t anchor) {
    const LineDrawingScenePath *p=CameraPath_Selected(s);
    const LineDrawingSceneCamera *camera=p ? Layout_SceneAuthoringState_FindCameraForPathConst(&s->layout.sceneAuthoring,p) : NULL;
    LineDrawingSceneCameraPose pose; float fov;
    if(!camera || !draft_guard(s) || !CameraPoses_Anchor(p,camera,anchor,&pose,&fov)) return false;
    if(!enter(s,p,camera,&pose,fov)) return false;
    CameraViewSession *v=&s->cameraView;
    v->path_anchor=anchor; v->path_playing=false; v->path_time=0;
    for(size_t i=0;i<anchor;++i) v->path_time+=p->key_count ? p->keys[i].seconds : 0;
    if(!p->key_count) {
        float total=CameraPoses_Distance(p,Layout_ScenePathEdit_AnchorCount(p)-1);
        v->path_time=total>0 ? p->duration_seconds*CameraPoses_Distance(p,anchor)/total : 0;
    }
    snprintf(v->anchor_source,sizeof(v->anchor_source),"%s",p->key_count ? p->keys[anchor].id : "");
    v->message[0]=0;
    (void)CameraPath_SelectPoint(s,anchor); return true;
}
static LineDrawingScenePath *source(GlobalState *s,LineDrawingSceneCamera **camera) {
    CameraViewSession *v=&s->cameraView;
    LineDrawingSceneAuthoringState *a=&s->layout.sceneAuthoring;
    LineDrawingScenePath *p=Layout_SceneAuthoringState_FindPathById(a,v->path_source);
    *camera=p ? Layout_SceneAuthoringState_FindCameraForPath(a,p) : NULL;
    if(!p || !*camera || signature(p,*camera)!=v->path_signature || v->path_anchor>=Layout_ScenePathEdit_AnchorCount(p) ||
       (v->anchor_source[0] && strcmp(v->anchor_source,p->keys[v->path_anchor].id))) {
        snprintf(v->message,sizeof(v->message),"Point changed. Revert/re-enter before applying."); return NULL;
    }
    return p;
}
static LineDrawingCameraKey draft_key(const CameraViewSession *v) {
    LineDrawingCameraKey k={.fov=v->fov,.seconds=2}; Vec3 f,u;
    CameraView_Basis(v,&f,NULL,&u); CameraPoses_SetRotation(&k,f,u); return k;
}
static bool publish(GlobalState *s,LineDrawingScenePath *target,const LineDrawingScenePath *copy,size_t anchor,bool lens) {
    if(!Editor_TryHistoryCapture(&s->editor,&s->layout)) return false;
    *target=*copy; target->playing=false;
    LineDrawingSceneCamera *camera=Layout_SceneAuthoringState_FindCameraForPath(&s->layout.sceneAuthoring,target);
    if(camera && target->key_count) {
        camera->orientation_mode=LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PER_POINT;
        camera->position=target->control_points[0];
        if(lens) {camera->near_clip=s->cameraView.near_clip;camera->far_clip=s->cameraView.far_clip;}
    }
    s->layout.sceneAuthoring.selected_kind=LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH;
    s->layout.sceneAuthoring.selected_index=(size_t)(target-s->layout.sceneAuthoring.paths);
    s->cameraView.path_modified=false;
    Global_FlagLayoutChanged();
    (void)CameraPath_SelectPoint(s,anchor);
    return CameraPath_ViewPoint(s,anchor);
}
bool CameraPath_Apply(GlobalState *s) {
    if(!s || !s->cameraView.active) return false;
    LineDrawingSceneCamera *camera; LineDrawingScenePath *p=source(s,&camera);
    if(!p) return false;
    if(CameraPath_Selected(s)!=p || CameraPath_SelectedAnchor(s)!=s->cameraView.path_anchor) {
        snprintf(s->cameraView.message,sizeof(s->cameraView.message),"View the selected point before Apply."); return false;
    }
    LineDrawingScenePath copy=*p; size_t anchor=s->cameraView.path_anchor;
    if(!CameraPoses_Enable(&copy,camera)) return false;
    LineDrawingCameraKey k=draft_key(&s->cameraView);
    k.seconds=copy.keys[anchor].seconds; snprintf(k.id,sizeof(k.id),"%s",copy.keys[anchor].id);
    copy.keys[anchor]=k;
    if(!Layout_ScenePathEdit_SetElementWorldPoint(&copy,Layout_ScenePathEdit_ElementForControl(&copy,CameraPoses_Control(&copy,anchor)),s->cameraView.eye) || !CameraPoses_Valid(&copy)) return false;
    return publish(s,p,&copy,anchor,true);
}
bool CameraPath_Add(GlobalState *s,bool new_path) {
    if(!s || !s->cameraView.active) return false;
    LineDrawingSceneAuthoringState *a=&s->layout.sceneAuthoring;
    LineDrawingCameraKey k=draft_key(&s->cameraView);
    if(new_path) {
        if(a->path_count>=LINE_DRAWING_SCENE_AUTHORING_MAX_PATHS || a->camera_count>=LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERAS) {
            snprintf(s->cameraView.message,sizeof(s->cameraView.message),"Camera/path limit reached."); return false;
        }
        LineDrawingScenePath p={.role=LINE_DRAWING_SCENE_PATH_ROLE_CAMERA,.control_point_count=1,.key_count=1,.duration_seconds=2,.next_key_id=1};
        unsigned n=1;
        do { snprintf(p.path_id,sizeof(p.path_id),"view_path_%u",n++); } while(Layout_SceneAuthoringState_FindPathById(a,p.path_id));
        snprintf(p.bound_camera_id,sizeof(p.bound_camera_id),"view_camera_%u",n-1);
        while(Layout_SceneAuthoringState_FindCameraById(a,p.bound_camera_id)) snprintf(p.bound_camera_id,sizeof(p.bound_camera_id),"view_camera_%u",n++);
        snprintf(p.label,sizeof(p.label),"View path %u",n-1); snprintf(p.curve_type,sizeof(p.curve_type),"linear");
        p.control_points[0]=s->cameraView.eye; snprintf(k.id,sizeof(k.id),"pose_1"); p.keys[0]=k;
        LineDrawingSceneCamera camera; Layout_SceneCamera_SetDefaults(&camera,p.bound_camera_id,p.label,p.path_id);
        camera.position=p.control_points[0]; camera.near_clip=s->cameraView.near_clip; camera.far_clip=s->cameraView.far_clip;
        if(!Editor_TryHistoryCapture(&s->editor,&s->layout)) return false;
        a->paths[a->path_count]=p; a->selected_kind=LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH; a->selected_index=a->path_count++;
        s->cameraView.selected=a->camera_count; a->cameras[a->camera_count++]=camera;
        s->cameraView.path_modified=false; Global_FlagLayoutChanged(); return CameraPath_ViewPoint(s,0);
    }
    LineDrawingScenePath *p=(LineDrawingScenePath *)CameraPath_Selected(s);
    LineDrawingSceneCamera *camera=p ? Layout_SceneAuthoringState_FindCameraForPath(a,p) : NULL;
    if(!camera) return false;
    if(s->cameraView.path_modified && strcmp(p->path_id,s->cameraView.path_source)) return draft_guard(s);
    LineDrawingScenePath copy=*p; size_t anchor=CameraPath_SelectedAnchor(s);
    if(!CameraPoses_Enable(&copy,camera) || !CameraPoses_Add(&copy,anchor,s->cameraView.eye,&k)) {
        snprintf(s->cameraView.message,sizeof(s->cameraView.message),"Point limit reached; path unchanged."); return false;
    }
    return publish(s,p,&copy,anchor+1,false);
}
bool CameraPath_Move(GlobalState *s,int direction) {
    LineDrawingScenePath *p=(LineDrawingScenePath *)CameraPath_Selected(s);
    if(!p || !draft_guard(s)) return false;
    LineDrawingScenePath copy=*p;
    const LineDrawingSceneCamera *camera=Layout_SceneAuthoringState_FindCameraForPathConst(&s->layout.sceneAuthoring,p);
    size_t a=CameraPath_SelectedAnchor(s);
    if(!CameraPoses_Enable(&copy,camera) || (direction<0 && !a) || (direction>0 && a+1>=copy.key_count)) return false;
    size_t b=direction<0 ? a-1 : a+1;
    return CameraPoses_Move(&copy,a,b) && publish(s,p,&copy,b,false);
}
bool CameraPath_Remove(GlobalState *s) {
    LineDrawingScenePath *p=(LineDrawingScenePath *)CameraPath_Selected(s);
    if(!p || !draft_guard(s)) return false;
    LineDrawingScenePath copy=*p;
    size_t anchor=CameraPath_SelectedAnchor(s);
    const LineDrawingSceneCamera *c=Layout_SceneAuthoringState_FindCameraForPathConst(&s->layout.sceneAuthoring,p);
    if(!CameraPoses_Enable(&copy,c) || !CameraPoses_Remove(&copy,anchor)) return false;
    return publish(s,p,&copy,anchor<copy.key_count ? anchor : copy.key_count-1,false);
}
bool CameraPath_SetSeconds(GlobalState *s,float seconds) {
    LineDrawingScenePath *p=(LineDrawingScenePath *)CameraPath_Selected(s);
    if(!p || !draft_guard(s) || !isfinite(seconds) || seconds<=0 || seconds>86400) return false;
    LineDrawingScenePath copy=*p;
    const LineDrawingSceneCamera *c=Layout_SceneAuthoringState_FindCameraForPathConst(&s->layout.sceneAuthoring,p);
    size_t anchor=CameraPath_SelectedAnchor(s);
    if(!CameraPoses_Enable(&copy,c)) return false;
    copy.keys[anchor].seconds=seconds;
    return publish(s,p,&copy,anchor,false);
}
bool CameraPath_Scrub(GlobalState *s,float seconds) {
    const LineDrawingScenePath *p=CameraPath_Selected(s);
    const LineDrawingSceneCamera *camera=p ? Layout_SceneAuthoringState_FindCameraForPathConst(&s->layout.sceneAuthoring,p) : NULL;
    LineDrawingSceneCameraPose pose; float fov;
    if(!camera || !draft_guard(s) || !CameraPoses_Time(p,camera,seconds,&pose,&fov) || !enter(s,p,camera,&pose,fov)) return false;
    s->cameraView.path_time=fmaxf(0,fminf(CameraPoses_Duration(p),seconds));
    s->cameraView.path_playing=false; s->cameraView.anchor_source[0]=0;
    /* Scrub is deliberately not an Apply target; View point makes that choice. */
    s->cameraView.path_anchor=LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS;
    return true;
}
bool CameraPath_TogglePlay(GlobalState *s) {
    if(s->cameraView.path_playing) { s->cameraView.path_playing=false; return true; }
    const LineDrawingScenePath *p=CameraPath_Selected(s);
    if(!p || Layout_ScenePathEdit_AnchorCount(p)<2 || !draft_guard(s)) return false;
    float time=!strcmp(p->path_id,s->cameraView.path_source) ? s->cameraView.path_time : 0;
    if(time>=CameraPoses_Duration(p)) time=0;
    if(!CameraPath_Scrub(s,time)) return false;
    s->cameraView.path_playing=true; return true;
}
void CameraPath_Stop(GlobalState *s) { if(s)s->cameraView.path_playing=false; }
void CameraPath_NavigationChanged(GlobalState *s) {
    if(!s)return;
    CameraPath_Stop(s);
    if(s->cameraView.path_source[0]) s->cameraView.path_modified=true;
}
void CameraPath_Reconcile(GlobalState *s) {
    CameraPath_Stop(s);
    if(!s)return;
    const LineDrawingScenePath *p=Layout_SceneAuthoringState_FindPathByIdConst(&s->layout.sceneAuthoring,s->cameraView.path_source);
    if(!p) { s->cameraView.path_signature=0; return; }
    if(s->cameraView.anchor_source[0]) for(size_t i=0;i<p->key_count;++i)
        if(!strcmp(p->keys[i].id,s->cameraView.anchor_source)) { s->cameraView.path_anchor=i; break; }
}
bool CameraPath_Tick(GlobalState *s,float delta) {
    CameraViewSession *v=&s->cameraView;
    if(!v->active || !v->path_playing || !isfinite(delta) || delta<=0) return false;
    const LineDrawingScenePath *p=Layout_SceneAuthoringState_FindPathByIdConst(&s->layout.sceneAuthoring,v->path_source);
    const LineDrawingSceneCamera *c=p ? Layout_SceneAuthoringState_FindCameraForPathConst(&s->layout.sceneAuthoring,p) : NULL;
    if(!c || signature(p,c)!=v->path_signature) { CameraPath_Stop(s); return false; }
    float duration=CameraPoses_Duration(p),time=v->path_time+fminf(delta,.05f);
    if(p->playback_mode==LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP) time=fmodf(time,duration);
    else if(time>=duration) { time=duration; v->path_playing=false; }
    LineDrawingSceneCameraPose pose; float fov;
    static LineDrawingScenePathTraversalTable table;
    static uint64_t cached_signature;
    if(cached_signature!=v->path_signature) {
        if(!Layout_ScenePathTraversal_Build(p,&table)) { CameraPath_Stop(s); return false; }
        cached_signature=v->path_signature;
    }
    if(!CameraPoses_TimeCached(p,c,time,&table,&pose,&fov)) { CameraPath_Stop(s); return false; }
    v->path_time=time; set_pose(v,&pose,fov); Global_FlagHitboxesDirty(); return true;
}
