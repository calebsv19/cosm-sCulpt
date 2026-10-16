#include "Layout/scene/layout_folding_brace.h"
#include "Layout/layout_motion.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static bool refused(int line) {
    if(getenv("LINE_DRAWING_BRACE_DIAGNOSTICS")) fprintf(stderr,"Folding brace refusal at line %d\n",line);
    return false;
}

bool Layout_FoldingElbow(Vec3 a,Vec3 b,Vec3 normal,double l1,double l2,int branch,Vec3* out) {
    double n=hypot(hypot(normal.x,normal.y),normal.z);
    double dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z,d=hypot(hypot(dx,dy),dz);
    if(!out || !isfinite(d) || !isfinite(n) || n<1e-9 || !isfinite(l1) || !isfinite(l2) ||
        l1<=1e-6 || l2<=1e-6 || l1>100 || l2>100 || (branch!=1 && branch!=-1) ||
        d<1e-6 || d>l1+l2+1e-6 || d<fabs(l1-l2)-1e-6) return refused(__LINE__);
    Vec3 u={(float)(dx/d),(float)(dy/d),(float)(dz/d)};normal=Vec3_Scale(normal,(float)(1/n));
    if(fabs(Vec3_Dot(u,normal))>1e-5) return refused(__LINE__);
    double x=(l1*l1-l2*l2+d*d)/(2*d),h2=l1*l1-x*x;
    if(fabs(d-(l1+l2))<=1e-6 || (fabs(l1-l2)>1e-6 && fabs(d-fabs(l1-l2))<=1e-7)) h2=0;
    if(h2 < -1e-6*fmax(l1,l2)) return refused(__LINE__);
    Vec3 result=Vec3_Add(a,Vec3_Add(Vec3_Scale(u,(float)x),Vec3_Scale(Vec3_Cross(normal,u),(float)(branch*sqrt(fmax(0,h2))))));
    if(!isfinite(result.x)||!isfinite(result.y)||!isfinite(result.z)) return refused(__LINE__);
    *out=result;return true;
}
static const Object3D* entity(const Layout* l,const char* id) {
    if(!memchr(id,0,64)) return NULL;
    for(size_t i=0;i<l->objectStore.count;++i)
        if(!l->objectStore.items[i].isDeleted && !strcmp(id,l->objectStore.items[i].coreMeta.object_id)) return &l->objectStore.items[i];
    return NULL;
}
static PlaneFrame3 frame(const Object3D* o) {return o->kind==OBJECT3D_KIND_PLANE?o->plane.frame:o->rectPrism.frame;}
static Vec3 anchor(const Layout* l,const Object3D* o,const double offset[3]) {
    PlaneFrame3 f=frame(o);
    return Vec3_Add(Vec3_Scale(f.origin,Layout_WorldScale(l)),Vec3_Add(Vec3_Add(Vec3_Scale(f.axisU,offset[0]),Vec3_Scale(f.axisV,offset[1])),Vec3_Scale(f.normal,offset[2])));
}
static bool geometry(const Layout* l,const LayoutConstraint* c,Vec3* a,Vec3* b,Vec3* elbow) {
    const Object3D* fixed=entity(l,c->a.entity_id),*leaf=entity(l,c->b.entity_id);
    if(!fixed || !leaf || (fixed->kind!=OBJECT3D_KIND_RECT_PRISM && fixed->kind!=OBJECT3D_KIND_PLANE) ||
        (leaf->kind!=OBJECT3D_KIND_RECT_PRISM && leaf->kind!=OBJECT3D_KIND_PLANE)) return refused(__LINE__);
    *a=anchor(l,fixed,c->folding_brace.fixed_offset_m);*b=anchor(l,leaf,c->folding_brace.leaf_offset_m);
    return Layout_FoldingElbow(*a,*b,c->axis,c->folding_brace.length_m[0],c->folding_brace.length_m[1],c->folding_brace.branch,elbow);
}
static double precise_dot(Vec3 a,Vec3 b) {
    return (double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z;
}
/* Exact extrema of anchor separation over the declared angular interval.
 * Evaluate endpoints and sinusoid stationary points, not sampled reachability. */
static bool reachable_range(const Layout* l,const LayoutConstraint* c) {
    Vec3 a,b,e;LayoutResolvedReference pivot;
    if(!geometry(l,c,&a,&b,&e)||Layout_ResolveReference(l,&c->a,&pivot)!=LAYOUT_MEASUREMENT_OK)return false;
    LayoutMeasurementResult measured=Layout_Measure(l,&c->a,&c->b,LAYOUT_MEASURE_PLANAR_ANGLE,c->axis);
    if(measured.status!=LAYOUT_MEASUREMENT_OK)return false;
    Vec3 origin={pivot.point_meters[0],pivot.point_meters[1],pivot.point_meters[2]};
    Vec3 n=Vec3_Scale(c->axis,1/Vec3_Length(c->axis)),q=Vec3_Sub(a,origin),r=Vec3_Sub(b,origin);
    double qa=precise_dot(q,n),ra=precise_dot(r,n);
    if(fabs(qa-ra)>1e-5)return false;
    Vec3 qp=Vec3_Sub(q,Vec3_Scale(n,qa)),rp=Vec3_Sub(r,Vec3_Scale(n,ra));
    double constant=precise_dot(q,q)+precise_dot(r,r)-2*qa*ra;
    double cosine=-2*precise_dot(qp,rp),sine=-2*precise_dot(qp,Vec3_Cross(n,rp));
    double low=(c->travel_min-measured.value)*.017453292519943295,high=(c->travel_max-measured.value)*.017453292519943295;
    double minimum=fmin(constant+cosine*cos(low)+sine*sin(low),constant+cosine*cos(high)+sine*sin(high));
    double maximum=fmax(constant+cosine*cos(low)+sine*sin(low),constant+cosine*cos(high)+sine*sin(high));
    double phase=atan2(sine,cosine);
    for(int i=-4;i<=4;++i){double angle=phase+i*3.141592653589793;if(angle<low||angle>high)continue;double value=constant+cosine*cos(angle)+sine*sin(angle);minimum=fmin(minimum,value);maximum=fmax(maximum,value);}
    double shortest=fabs(c->folding_brace.length_m[0]-c->folding_brace.length_m[1]),longest=c->folding_brace.length_m[0]+c->folding_brace.length_m[1];
    return minimum>1e-12&&sqrt(fmax(0,minimum))>=shortest-1e-6&&sqrt(fmax(0,maximum))<=longest+1e-6;
}
static Vec3 euler(Vec3 x,Vec3 y,Vec3 z) {
    /* atan2 retains the small horizontal component near a vertical link. */
    double pitch=atan2(-x.z,hypot(x.x,x.y)),roll,yaw;
    if(fabs(cos(pitch))>1e-6) {roll=atan2(y.z,z.z);yaw=atan2(x.y,x.x);}
    else {roll=atan2(-z.y,y.y);yaw=0;}
    return (Vec3){roll*57.295779513,pitch*57.295779513,yaw*57.295779513};
}
static bool poses(const Layout* l,const LayoutConstraint* c,Vec3 positions[9],Vec3 rotations[9]) {
    Vec3 a,b,e;if(!geometry(l,c,&a,&b,&e)) return refused(__LINE__);
    PlaneFrame3 f=frame(entity(l,c->a.entity_id)),leaf=frame(entity(l,c->b.entity_id));Vec3 normal=Vec3_Scale(c->axis,1/Vec3_Length(c->axis));
    positions[0]=Vec3_Scale(Vec3_Add(a,e),.5f);positions[1]=Vec3_Scale(Vec3_Add(e,b),.5f);
    positions[2]=positions[4]=a;positions[6]=b;positions[5]=positions[8]=e;
    /* Mounting plate top meets the 18mm leaf underside; the pin lies in the tab envelope. */
    positions[3]=Vec3_Add(b,Vec3_Scale(leaf.normal,.003f));
    LayoutResolvedReference pivot;LayoutMeasurementResult measured=Layout_Measure(l,&c->a,&c->b,LAYOUT_MEASURE_PLANAR_ANGLE,c->axis);
    if(Layout_ResolveReference(l,&c->a,&pivot)!=LAYOUT_MEASUREMENT_OK||measured.status!=LAYOUT_MEASUREMENT_OK)return refused(__LINE__);
    Vec3 origin={pivot.point_meters[0],pivot.point_meters[1],pivot.point_meters[2]},r=Vec3_Sub(b,origin);
    double angle=(c->travel_home-measured.value)*.017453292519943295;
    Vec3 deployed=Vec3_Add(origin,Vec3_Add(Vec3_Add(Vec3_Scale(r,cos(angle)),Vec3_Scale(Vec3_Cross(normal,r),sin(angle))),Vec3_Scale(normal,Vec3_Dot(normal,r)*(1-cos(angle))))),stop;
    if(!Layout_FoldingElbow(a,deployed,normal,c->folding_brace.length_m[0],c->folding_brace.length_m[1],c->folding_brace.branch,&stop))return refused(__LINE__);
    /* Stationary illustrative stop touches the deployed elbow-pin envelope. */
    positions[7]=Vec3_Sub(stop,Vec3_Scale(f.normal,.025f));
    for(int i=0;i<2;++i) {Vec3 x=Vec3_Sub(i?b:e,i?e:a);x=Vec3_Scale(x,1/Vec3_Length(x));rotations[i]=euler(x,normal,Vec3_Cross(x,normal));}
    for(int i=2;i<9;++i) rotations[i]=euler(f.axisU,f.axisV,f.normal);
    rotations[3]=rotations[6]=euler(leaf.axisU,leaf.axisV,leaf.normal);
    const LayoutFoldingBrace* mount=&c->folding_brace;
    if(mount->compact_mount){
        for(int i=0;i<3;++i)positions[i+4]=Vec3_Add(positions[i+4],Vec3_Scale(normal,mount->pin_lateral_m[i]));
        for(int i=0;i<2;++i)positions[i]=Vec3_Add(positions[i],Vec3_Scale(normal,mount->link_lateral_m[i]));
        positions[2]=Vec3_Add(a,Vec3_Add(Vec3_Add(Vec3_Scale(f.axisU,mount->fixed_plate_offset_m[0]),Vec3_Scale(f.axisV,mount->fixed_plate_offset_m[1])),Vec3_Scale(f.normal,mount->fixed_plate_offset_m[2])));
        positions[3]=Vec3_Add(b,Vec3_Add(Vec3_Add(Vec3_Scale(leaf.axisU,mount->leaf_plate_offset_m[0]),Vec3_Scale(leaf.axisV,mount->leaf_plate_offset_m[1])),Vec3_Scale(leaf.normal,mount->leaf_plate_offset_m[2])));
        /* Staggered straps rotate on a common shaft. Guide remains attached to
         * the elbow assembly; the manual locking key withdraws axially. */
        Vec3 x=Vec3_Sub(e,a);x=Vec3_Scale(x,1/Vec3_Length(x));
        positions[7]=Vec3_Add(Vec3_Sub(e,Vec3_Scale(x,.005f)),Vec3_Scale(normal,.022f));
        positions[8]=Vec3_Add(e,Vec3_Scale(normal,mount->lock_engaged?0:.040f));
        rotations[7]=rotations[8]=rotations[0];
    }
    return true;
}
static bool pose_matches(const Layout* l,const Object3D* o,Vec3 position,Vec3 rotation,double tolerance) {
    Object3D desired=*o;desired.transform.position=Vec3_Scale(position,1/Layout_WorldScale(l));desired.transform.rotationDeg=rotation;
    Vec3 a[8],b[8];if(!Layout_Object3D_ComputeMeshInstanceCorners(o,a)||!Layout_Object3D_ComputeMeshInstanceCorners(&desired,b))return false;
    for(int i=0;i<8;++i)if(Vec3_Distance(a[i],b[i])*Layout_WorldScale(l)>tolerance)return false;
    return true;
}
bool Layout_FoldingBracesValidate(const Layout* l,bool check_pose) {
    for(size_t i=0;i<l->objectStore.constraintCount;++i) {
        const LayoutConstraint* owner=&l->objectStore.constraints[i];
        if(owner->paired_folding_brace.enabled&&(!owner->folding_brace.enabled||owner->paired_folding_brace.manual_lock))return refused(__LINE__);
        for(int mechanism=0;mechanism<2;++mechanism) {
        LayoutConstraint copy=*owner;if(mechanism)copy.folding_brace=owner->paired_folding_brace;
        const LayoutConstraint* c=&copy;const LayoutFoldingBrace* f=&c->folding_brace;
        if(!f->enabled) continue;
        if(c->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL || (f->branch!=1 && f->branch!=-1)) return refused(__LINE__);
        for(int k=0;k<3;++k) if(!isfinite(f->fixed_offset_m[k]) || !isfinite(f->leaf_offset_m[k]) || fabs(f->fixed_offset_m[k])>100 || fabs(f->leaf_offset_m[k])>100) return refused(__LINE__);
        for(int k=0;k<2;++k) if(!isfinite(f->length_m[k]) || f->length_m[k]<=1e-6 || f->length_m[k]>100) return refused(__LINE__);
        if(f->compact_mount){
            for(int k=0;k<3;++k)if(!isfinite(f->pin_lateral_m[k])||fabs(f->pin_lateral_m[k])>.1)return refused(__LINE__);
            for(int k=0;k<2;++k)if(!isfinite(f->link_lateral_m[k])||fabs(f->link_lateral_m[k])>.1)return refused(__LINE__);
            for(int k=0;k<3;++k)if(!isfinite(f->fixed_plate_offset_m[k])||!isfinite(f->leaf_plate_offset_m[k])||fabs(f->fixed_plate_offset_m[k])>.1||fabs(f->leaf_plate_offset_m[k])>.1)return refused(__LINE__);
        }
        if(!Layout_FoldingLockValid(l,c)||!reachable_range(l,c)) return refused(__LINE__);
        if(f->manual_lock&&f->lock_engaged){Vec3 a,b,e;if(!geometry(l,c,&a,&b,&e)||fabs(Vec3_Distance(a,b)-f->length_m[0]-f->length_m[1])>2e-6)return refused(__LINE__);}
        Vec3 p[9],rot[9];if(check_pose && !poses(l,c,p,rot)) return refused(__LINE__);
        for(int k=0;k<9;++k) {
            const Object3D* o=entity(l,f->parts[k]);
            if(!o || o->kind!=OBJECT3D_KIND_MESH_ASSET_INSTANCE || o->info.reference || o->info.volume_role || (c->motion_assembly[0] && Layout_IsDescendant(&l->objectStore,o->info.parent_id,c->motion_assembly))) return refused(__LINE__);
            for(int j=0;j<k;++j) if(!strcmp(f->parts[k],f->parts[j])) return refused(__LINE__);
            if(mechanism)for(int j=0;j<9;++j)if(!strcmp(f->parts[k],owner->folding_brace.parts[j]))return refused(__LINE__);
            for(size_t j=0;j<l->objectStore.constraintCount;++j) {
                const LayoutConstraint* other=&l->objectStore.constraints[j];
                if(!strcmp(o->coreMeta.object_id,other->a.entity_id) || !strcmp(o->coreMeta.object_id,other->b.entity_id) || (j!=i && Layout_MotionScopeContains(&l->objectStore,other,o))) return refused(__LINE__);
                if(j!=i && other->folding_brace.enabled) for(int n=0;n<9;++n) if(!strcmp(o->coreMeta.object_id,other->folding_brace.parts[n])) return refused(__LINE__);
            }
            double scale=Layout_WorldScale(l);Vec3 lo=o->meshInstance.localBoundsMin,hi=o->meshInstance.localBoundsMax;
            if(fabs(o->transform.scale.x*scale-1)>1e-6 || fabs(o->transform.scale.y*scale-1)>1e-6 || fabs(o->transform.scale.z*scale-1)>1e-6 || Vec3_Length(Vec3_Add(lo,hi))>1e-6) return refused(__LINE__);
            if(k<2 && fabs((hi.x-lo.x)-f->length_m[k])>1e-6) return refused(__LINE__);
            if(check_pose && !pose_matches(l,o,p[k],rot[k],2e-5)) return refused(__LINE__);
        }
        }
    }
    return true;
}
static bool apply_one(Layout* l,const LayoutConstraint* c) {
    if(!c->folding_brace.enabled) return true;
    Vec3 p[9],rot[9];if(!l->geometryEditActive || !poses(l,c,p,rot)) return refused(__LINE__);
    for(int i=0;i<9;++i) {
        const Object3D* found=entity(l,c->folding_brace.parts[i]);if(!found) return refused(__LINE__);
        /* Preserve exact rigid parent moves and stationary/locked anchor parts. */
        if(pose_matches(l,found,p[i],rot[i],1e-6)) continue;
        if(found->coreMeta.flags.locked) return refused(__LINE__);
        Object3D* o=Layout_ObjectStore_Find(&l->objectStore,found->objectId);bool adjusted=false;
        /* Canonical source frame, no incremental stretch or regeneration. Candidate
         * publication and rollback remain owned by the geometry transaction. */
        o->transform.rotationDeg=rot[i];
        if(!Layout_SetObject3DPosition(l,o->objectId,Vec3_Scale(p[i],1/Layout_WorldScale(l)),&adjusted) || adjusted) return refused(__LINE__);
    }
    return true;
}

bool Layout_FoldingBraceApply(Layout* l,const LayoutConstraint* c) {
    if(!apply_one(l,c))return false;
    if(!c->paired_folding_brace.enabled)return true;
    LayoutConstraint copy=*c;copy.folding_brace=c->paired_folding_brace;
    return apply_one(l,&copy);
}
