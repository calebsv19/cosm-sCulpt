#include "Layout/scene/layout_camera_poses.h"
#include "Layout/scene/layout_scene_path_edit.h"
#include "Layout/scene/layout_scene_path_geometry.h"
#include "Layout/scene/layout_scene_path_traversal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

size_t CameraPoses_Control(const LineDrawingScenePath *p, size_t a) {
    return Layout_ScenePathGeometry_IsCompleteCubic(p) ? a * 3 : a;
}
float CameraPoses_Distance(const LineDrawingScenePath *p, size_t a) {
    LineDrawingScenePathTraversalTable table;
    if (!p || a >= Layout_ScenePathEdit_AnchorCount(p) || !Layout_ScenePathTraversal_Build(p,&table)) return 0;
    size_t i = Layout_ScenePathGeometry_IsCompleteCubic(p)
        ? a * LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT : a;
    return i < table.sample_count ? table.cumulative_distance[i] : 0;
}
static float total_distance(const LineDrawingScenePath *p) {
    LineDrawingScenePathTraversalTable table;
    return Layout_ScenePathTraversal_Build(p,&table) ? table.total_distance : 0;
}
static size_t legs(const LineDrawingScenePath *p) {
    size_t n=Layout_ScenePathEdit_AnchorCount(p);
    return n > 1 ? n - (p->closed ? 0 : 1) : 0;
}
float CameraPoses_Duration(const LineDrawingScenePath *p) {
    if (!p) return 0;
    if (!p->key_count) return p->duration_seconds;
    float duration=0;
    for(size_t i=0;i<legs(p);++i) duration+=p->keys[i].seconds;
    return duration;
}
static void normalize(float q[4]) {
    float n=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    for(int i=0;i<4;++i) q[i]/=n;
}
void CameraPoses_SetRotation(LineDrawingCameraKey *key, Vec3 f, Vec3 u) {
    f=Vec3_Normalize(f);
    Vec3 l=Vec3_Normalize(Vec3_Cross(u,f));
    u=Vec3_Normalize(Vec3_Cross(f,l));
    float m[3][3]={{f.x,l.x,u.x},{f.y,l.y,u.y},{f.z,l.z,u.z}};
    float *q=key->rotation, trace=m[0][0]+m[1][1]+m[2][2];
    if(trace>0) {
        float s=sqrtf(trace+1)*2;
        q[0]=s*.25f; q[1]=(m[2][1]-m[1][2])/s;
        q[2]=(m[0][2]-m[2][0])/s; q[3]=(m[1][0]-m[0][1])/s;
    } else {
        int i=m[1][1]>m[0][0] ? 1 : 0;
        if(m[2][2]>m[i][i]) i=2;
        int j=(i+1)%3,k=(i+2)%3;
        float s=sqrtf(1+m[i][i]-m[j][j]-m[k][k])*2;
        q[i+1]=s*.25f; q[0]=(m[k][j]-m[j][k])/s;
        q[j+1]=(m[j][i]+m[i][j])/s; q[k+1]=(m[k][i]+m[i][k])/s;
    }
    normalize(q);
}
static void rotation_pose(const float q[4], LineDrawingSceneCameraPose *pose) {
    float w=q[0],x=q[1],y=q[2],z=q[3];
    pose->forward=(Vec3){1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)};
    pose->up=(Vec3){2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)};
}
static void blend(const LineDrawingCameraKey *a,const LineDrawingCameraKey *b,float t,
                  LineDrawingSceneCameraPose *pose,float *fov) {
    float q[4],dot=0;
    for(int i=0;i<4;++i) dot+=a->rotation[i]*b->rotation[i];
    float sign=dot<0 ? -1 : 1;
    dot=fminf(1,fabsf(dot));
    float sa=1-t,sb=t;
    if(dot<.9995f) {
        float angle=acosf(dot), divisor=sinf(angle);
        sa=sinf((1-t)*angle)/divisor; sb=sinf(t*angle)/divisor;
    }
    for(int i=0;i<4;++i) q[i]=a->rotation[i]*sa+b->rotation[i]*sign*sb;
    normalize(q); rotation_pose(q,pose);
    if(fov) *fov=a->fov+(b->fov-a->fov)*t;
}
bool CameraPoses_Valid(const LineDrawingScenePath *p) {
    if(!p || !p->key_count || p->key_count!=Layout_ScenePathEdit_AnchorCount(p) ||
       p->key_count>LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) return false;
    for(size_t i=0;i<p->control_point_count;++i) {
        Vec3 v=p->control_points[i]; if(!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z)) return false;
    }
    for(size_t i=0;i<p->key_count;++i) {
        const LineDrawingCameraKey *k=&p->keys[i];
        if(!k->id[0] || !memchr(k->id,0,sizeof(k->id)) || !isfinite(k->fov) ||
           k->fov<1 || k->fov>179 || !isfinite(k->seconds) || k->seconds<=0 || k->seconds>86400) return false;
        float n=0;
        for(int q=0;q<4;++q) { if(!isfinite(k->rotation[q])) return false; n+=k->rotation[q]*k->rotation[q]; }
        if(fabsf(n-1)>.001f) return false;
        for(size_t j=0;j<i;++j) if(!strcmp(k->id,p->keys[j].id)) return false;
    }
    return true;
}
static void new_id(LineDrawingScenePath *p,LineDrawingCameraKey *key) {
    bool found;
    do {
        snprintf(key->id,sizeof(key->id),"pose_%u",++p->next_key_id);
        found=false;
        for(size_t i=0;i<p->key_count;++i) if(!strcmp(key->id,p->keys[i].id)) found=true;
    } while(found);
}
bool CameraPoses_Anchor(const LineDrawingScenePath *p,const LineDrawingSceneCamera *c,size_t a,
                        LineDrawingSceneCameraPose *pose,float *fov) {
    if(!p || !c || !pose || a>=Layout_ScenePathEdit_AnchorCount(p)) return false;
    if(CameraPoses_Valid(p)) {
        rotation_pose(p->keys[a].rotation,pose);
        if(fov) *fov=p->keys[a].fov;
    } else {
        LineDrawingScenePath copy=*p;
        copy.playback_mode=LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE;
        float total=total_distance(p);
        if(!Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(c,&copy,
            total>0 ? CameraPoses_Distance(p,a)/total : 0,pose)) return false;
        if(fov) *fov=c->vertical_fov_degrees;
    }
    pose->position=p->control_points[CameraPoses_Control(p,a)];
    return true;
}
bool CameraPoses_Enable(LineDrawingScenePath *p,const LineDrawingSceneCamera *c) {
    if(p->key_count) return CameraPoses_Valid(p);
    size_t n=Layout_ScenePathEdit_AnchorCount(p);
    if(!n || !c) return false;
    LineDrawingCameraKey keys[LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS]={0};
    float total=total_distance(p);
    for(size_t i=0;i<n;++i) {
        LineDrawingSceneCameraPose pose;
        if(!CameraPoses_Anchor(p,c,i,&pose,&keys[i].fov)) return false;
        CameraPoses_SetRotation(&keys[i],pose.forward,pose.up);
        float end=i+1<n ? CameraPoses_Distance(p,i+1) : total;
        keys[i].seconds=fmaxf(.1f,total>0 ? p->duration_seconds*(end-CameraPoses_Distance(p,i))/total : p->duration_seconds/fmaxf(1,(float)legs(p)));
        new_id(p,&keys[i]);
    }
    memcpy(p->keys,keys,sizeof(keys)); p->key_count=n;
    return CameraPoses_Valid(p);
}
static float anchor_distance(const LineDrawingScenePath *p,const LineDrawingScenePathTraversalTable *table,size_t a) {
    size_t i=Layout_ScenePathGeometry_IsCompleteCubic(p) ? a*LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT : a;
    return i<table->sample_count ? table->cumulative_distance[i] : 0;
}
static bool leg_sample(const LineDrawingScenePath *p,size_t i,float t,
    const LineDrawingScenePathTraversalTable *table,LineDrawingSceneCameraPose *pose,float *fov) {
    size_t next=(i+1)%p->key_count;
    float start=anchor_distance(p,table,i),end=next ? anchor_distance(p,table,next) : table->total_distance;
    LineDrawingScenePathTraversalSample sample;
    if(!Layout_ScenePathTraversal_EvaluateDistance(table,start+(end-start)*t,
        LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE,&sample))return false;
    pose->position=sample.world;blend(&p->keys[i],&p->keys[next],t,pose,fov);return true;
}
bool CameraPoses_DistanceSample(const LineDrawingScenePath *p,float progress,
                               LineDrawingSceneCameraPose *pose,float *fov) {
    if(!CameraPoses_Valid(p) || !pose || !isfinite(progress)) return false;
    if(p->key_count==1) { rotation_pose(p->keys[0].rotation,pose); pose->position=p->control_points[0]; if(fov)*fov=p->keys[0].fov; return true; }
    progress=fmaxf(0,fminf(1,progress));
    LineDrawingScenePathTraversalTable table;
    if(!Layout_ScenePathTraversal_Build(p,&table))return false;
    float d=progress*table.total_distance;
    for(size_t i=0;i<legs(p);++i) {
        float start=anchor_distance(p,&table,i),end=i+1<p->key_count ? anchor_distance(p,&table,i+1) : table.total_distance;
        if(d<=end || i+1==legs(p)) return leg_sample(p,i,end>start ? (d-start)/(end-start) : 0,&table,pose,fov);
    }
    return false;
}
bool CameraPoses_Time(const LineDrawingScenePath *p,const LineDrawingSceneCamera *c,float time,
                      LineDrawingSceneCameraPose *pose,float *fov) {
    LineDrawingScenePathTraversalTable table;
    if(!Layout_ScenePathTraversal_Build(p,&table))return false;
    return CameraPoses_TimeCached(p,c,time,&table,pose,fov);
}
bool CameraPoses_TimeCached(const LineDrawingScenePath *p,const LineDrawingSceneCamera *c,float time,
    const LineDrawingScenePathTraversalTable *table,LineDrawingSceneCameraPose *pose,float *fov) {
    if(!p || !c || !pose || !isfinite(time)) return false;
    float duration=CameraPoses_Duration(p);
    if(!p->key_count) {
        if(fov) *fov=c->vertical_fov_degrees;
        return Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(c,p,duration>0 ? time/duration : 0,pose);
    }
    if(!CameraPoses_Valid(p)) return false;
    if(p->key_count==1) return CameraPoses_Anchor(p,c,0,pose,fov);
    if(p->playback_mode==LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP) { time=fmodf(time,duration); if(time<0)time+=duration; }
    else time=fmaxf(0,fminf(duration,time));
    float start=0;
    for(size_t i=0;i<legs(p);++i) {
        float span=p->keys[i].seconds;
        if(time<=start+span || i+1==legs(p)) return leg_sample(p,i,(time-start)/span,table,pose,fov);
        start+=span;
    }
    return false;
}
void CameraPoses_InsertKey(LineDrawingScenePath *p,size_t index) {
    if(!p->key_count || p->key_count>=LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) return;
    LineDrawingCameraKey k=p->keys[index ? index-1 : 0];
    k.seconds=fmaxf(.1f,k.seconds*.5f);
    if(index) p->keys[index-1].seconds=k.seconds;
    new_id(p,&k);
    memmove(p->keys+index+1,p->keys+index,(p->key_count-index)*sizeof(k));
    p->keys[index]=k; ++p->key_count;
}
void CameraPoses_RemoveKey(LineDrawingScenePath *p,size_t index) {
    if(!p->key_count || index>=p->key_count) return;
    memmove(p->keys+index,p->keys+index+1,(p->key_count-index-1)*sizeof(p->keys[0]));
    memset(&p->keys[--p->key_count],0,sizeof(p->keys[0]));
}
bool CameraPoses_Add(LineDrawingScenePath *p,size_t after,Vec3 point,const LineDrawingCameraKey *key) {
    size_t n=Layout_ScenePathEdit_AnchorCount(p),insert=after+1;
    if(!CameraPoses_Valid(p) || after>=n) return false;
    bool cubic=Layout_ScenePathGeometry_IsCompleteCubic(p);
    if(p->control_point_count+(cubic ? 3 : 1)>LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) return false;
    if(cubic && insert<n) {
        if(!Layout_ScenePathEdit_SplitSegment(p,after,.5f,NULL)) return false;
        if(!Layout_ScenePathEdit_SetElementWorldPoint(p,Layout_ScenePathEdit_ElementForControl(p,insert*3),point)) return false;
    } else {
        if(cubic) {
            Vec3 a=p->control_points[p->control_point_count-1],d=Vec3_Sub(point,a);
            p->control_points[p->control_point_count++]=Vec3_Add(a,Vec3_Scale(d,1.f/3));
            p->control_points[p->control_point_count++]=Vec3_Add(a,Vec3_Scale(d,2.f/3));
            p->control_points[p->control_point_count++]=point;
            p->tangent_modes[insert]=LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
        } else {
            memmove(p->control_points+insert+1,p->control_points+insert,(n-insert)*sizeof(Vec3));
            p->control_points[insert]=point; ++p->control_point_count;
        }
        CameraPoses_InsertKey(p,insert);
    }
    char id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    snprintf(id,sizeof(id),"%s",p->keys[insert].id);
    p->keys[insert]=*key; snprintf(p->keys[insert].id,sizeof(p->keys[insert].id),"%s",id);
    return CameraPoses_Valid(p);
}
bool CameraPoses_Move(LineDrawingScenePath *p,size_t from,size_t to) {
    if(!CameraPoses_Valid(p) || from>=p->key_count || to>=p->key_count || from==to) return false;
    bool cubic=Layout_ScenePathGeometry_IsCompleteCubic(p);
    Vec3 anchors[16],in[16]={{0}},out[16]={{0}};
    LineDrawingCameraKey keys[16]; LineDrawingScenePathTangentMode modes[16];
    size_t order[16],n=p->key_count;
    for(size_t i=0;i<n;++i) {
        size_t c=CameraPoses_Control(p,i); order[i]=i; anchors[i]=p->control_points[c]; keys[i]=p->keys[i];
        modes[i]=Layout_ScenePathEdit_AnchorMode(p,i);
        if(cubic && c>0) in[i]=Vec3_Sub(p->control_points[c-1],anchors[i]);
        if(cubic && c+1<p->control_point_count) out[i]=Vec3_Sub(p->control_points[c+1],anchors[i]);
    }
    if(from<to) for(size_t i=from;i<to;++i)order[i]=i+1;
    else for(size_t i=from;i>to;--i)order[i]=i-1;
    order[to]=from;
    for(size_t i=0;i<n;++i) {
        size_t j=order[i],c=cubic ? i*3 : i;
        p->control_points[c]=anchors[j]; p->keys[i]=keys[j];
        if(cubic) {
            p->tangent_modes[i]=modes[j];
            if(c>0)p->control_points[c-1]=Vec3_Add(anchors[j],in[j]);
            if(c+1<p->control_point_count)p->control_points[c+1]=Vec3_Add(anchors[j],out[j]);
        }
    }
    if(cubic) for(size_t i=0;i<n;++i)
        if(p->tangent_modes[i]==LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC)
            (void)Layout_ScenePathEdit_SetAnchorMode(p,i,p->tangent_modes[i]);
    return true;
}
bool CameraPoses_Remove(LineDrawingScenePath *p,size_t anchor) {
    if(!CameraPoses_Valid(p) || anchor>=p->key_count || p->key_count<=1) return false;
    if(p->key_count==2) {
        p->control_points[0]=p->control_points[CameraPoses_Control(p,1-anchor)];
        p->control_point_count=1; p->closed=false; snprintf(p->curve_type,sizeof(p->curve_type),"linear");
        CameraPoses_RemoveKey(p,anchor); return true;
    }
    return Layout_ScenePathEdit_DeleteElement(p,Layout_ScenePathEdit_ElementForControl(p,CameraPoses_Control(p,anchor)),NULL);
}
