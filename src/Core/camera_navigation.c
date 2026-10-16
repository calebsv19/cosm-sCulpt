#include "Core/camera_view.h"
#include "Core/global_state.h"
#include "Core/camera_path.h"
#include "Layout/layout_engineering.h"
#include <float.h>
/* Framing uses authoritative native corners, excluding spatial planning guides. */
bool CameraView_Frame(GlobalState* s,bool selection) {
    if(!s || !s->cameraView.active)return false;
    Vec3 lo={FLT_MAX,FLT_MAX,FLT_MAX},hi={-FLT_MAX,-FLT_MAX,-FLT_MAX};bool found=false;
    for(size_t i=0;i<s->layout.objectStore.count;++i) {
        const Object3D* o=&s->layout.objectStore.items[i];Vec3 p[8];size_t n=8;
        if(!Layout_ObjectShown(&s->layout.objectStore,o) || Layout_EntityIsSpatialGuide(&o->info) ||
            (selection && o->objectId!=s->editor.selectedObject3DId))continue;
        bool ok=o->kind==OBJECT3D_KIND_PLANE ? (n=4,Layout_Object3D_ComputePlaneCorners(o,p)) :
            o->kind==OBJECT3D_KIND_RECT_PRISM ? Layout_Object3D_ComputeRectPrismCorners(o,p) :
            Layout_Object3D_ComputeMeshInstanceCorners(o,p);
        if(!ok)continue;
        for(size_t j=0;j<n;++j) {
            lo=(Vec3){fminf(lo.x,p[j].x),fminf(lo.y,p[j].y),fminf(lo.z,p[j].z)};
            hi=(Vec3){fmaxf(hi.x,p[j].x),fmaxf(hi.y,p[j].y),fmaxf(hi.z,p[j].z)};found=true;
        }
    }
    if(!found)return false;
    return CameraView_FrameBounds(s,lo,hi);
}
bool CameraView_FrameBounds(GlobalState* s,Vec3 lo,Vec3 hi) {
    if(!s||!s->cameraView.active||!isfinite(lo.x)||!isfinite(lo.y)||!isfinite(lo.z)||!isfinite(hi.x)||!isfinite(hi.y)||!isfinite(hi.z)||lo.x>hi.x||lo.y>hi.y||lo.z>hi.z)return false;
    CameraViewSession* c=&s->cameraView;CameraView_RefreshViewport(s);
    c->orbit_target=Vec3_Scale(Vec3_Add(lo,hi),.5f);
    float radius=fmaxf(Vec3_Distance(lo,hi)*.5f,(float)(.05/Layout_WorldScale(&s->layout)));
    float aspect=c->viewport[3]>0?c->viewport[2]/c->viewport[3]:1;
    float half=atanf(tanf(DegToRad(c->fov)*.5f)*fminf(1,aspect));
    float distance=radius/sinf(half)*1.15f;
    c->yaw=-135;c->pitch=-22;c->roll=0;
    Vec3 forward;CameraView_Basis(c,&forward,NULL,NULL);
    c->eye=Vec3_Sub(c->orbit_target,Vec3_Scale(forward,distance));
    c->near_clip=(float)(.01/Layout_WorldScale(&s->layout));
    c->far_clip=fmaxf((float)(100/Layout_WorldScale(&s->layout)),distance+radius*2);
    CameraPath_NavigationChanged(s);Global_FlagHitboxesDirty();return true;
}
void CameraView_Drag(GlobalState* s,float dx,float dy,bool pan) {
    CameraViewSession* c=&s->cameraView;Vec3 f,r,u;CameraView_Basis(c,&f,&r,&u);
    float distance=fmaxf(Vec3_Distance(c->eye,c->orbit_target),c->near_clip*10);
    if(pan) {
        float scale=2*distance*tanf(DegToRad(c->fov)*.5f)/fmaxf(1,c->viewport[3]);
        Vec3 d=Vec3_Add(Vec3_Scale(r,-dx*scale),Vec3_Scale(u,dy*scale));
        c->eye=Vec3_Add(c->eye,d);c->orbit_target=Vec3_Add(c->orbit_target,d);
    } else {
        c->yaw=Angle_NormalizeSignedDeg(c->yaw-dx*.25f);
        c->pitch=fmaxf(-89.5f,fminf(89.5f,c->pitch-dy*.25f));
        CameraView_Basis(c,&f,NULL,NULL);c->eye=Vec3_Sub(c->orbit_target,Vec3_Scale(f,distance));
    }
    CameraPath_NavigationChanged(s);Global_FlagHitboxesDirty();
}
void CameraView_Dolly(GlobalState* s,float amount) {
    CameraViewSession* c=&s->cameraView;Vec3 f;CameraView_Basis(c,&f,NULL,NULL);
    float distance=fmaxf(Vec3_Distance(c->eye,c->orbit_target),c->near_clip*10);
    c->eye=Vec3_Add(c->eye,Vec3_Scale(f,distance*(1-expf(-amount*.12f))));
    CameraPath_NavigationChanged(s);Global_FlagHitboxesDirty();
}
