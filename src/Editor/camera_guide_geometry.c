#include "Editor/scene_authoring_path_handles.h"
#include <math.h>
bool SceneAuthoringCameraGuide_Corners(const LineDrawingSceneCameraPose* pose,float fov,
    float aspect,Vec3 near_corners[4],Vec3 far_corners[4]) {
    if(!pose || !near_corners || !far_corners || !isfinite(fov) || fov<=0 || fov>=179 || !isfinite(aspect) || aspect<=0)return false;
    Vec3 right=Vec3_Normalize(Vec3_Cross(pose->forward,pose->up));
    if(Vec3_Length(right)<.5f)return false;
    float h=tanf(DegToRad(fov)*.5f);
    Vec3 near_center=Vec3_Add(pose->position,pose->forward),far_center=Vec3_Add(pose->position,Vec3_Scale(pose->forward,4));
    for(int i=0;i<4;++i) {
        float sx=(i==0 || i==3)?-1:1,sy=i<2?-1:1;
        Vec3 offset=Vec3_Add(Vec3_Scale(right,sx*h*aspect),Vec3_Scale(pose->up,sy*h));
        near_corners[i]=Vec3_Add(near_center,offset);far_corners[i]=Vec3_Add(far_center,Vec3_Scale(offset,4));
    }
    return true;
}
