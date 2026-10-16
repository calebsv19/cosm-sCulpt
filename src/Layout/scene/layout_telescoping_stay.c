#include "Layout/scene/layout_telescoping_stay.h"
#include "Layout/scene/layout_anchor_span.h"
#include "Layout/layout_motion.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static bool refused(int line){if(getenv("LINE_DRAWING_STAY_DIAGNOSTICS"))fprintf(stderr,"Telescoping stay refusal at line %d\n",line);return false;}

static const Object3D* entity(const Layout* l,const char* id) {
    if(!memchr(id,0,64))return NULL;
    for(size_t i=0;i<l->objectStore.count;++i)
        if(!l->objectStore.items[i].isDeleted && !strcmp(id,l->objectStore.items[i].coreMeta.object_id))return &l->objectStore.items[i];
    return NULL;
}
static PlaneFrame3 frame(const Object3D* o){return o->kind==OBJECT3D_KIND_PLANE?o->plane.frame:o->rectPrism.frame;}
static Vec3 offset(PlaneFrame3 f,const double v[3]){
    return Vec3_Add(Vec3_Add(Vec3_Scale(f.axisU,v[0]),Vec3_Scale(f.axisV,v[1])),Vec3_Scale(f.normal,v[2]));
}
bool Layout_TelescopingStayAnchors(const Layout* l,const LayoutConstraint* c,Vec3* a,Vec3* b){
    const Object3D* fixed=entity(l,c->a.entity_id),*leaf=entity(l,c->b.entity_id);
    if(!a||!b||!fixed||!leaf || (fixed->kind!=OBJECT3D_KIND_RECT_PRISM && fixed->kind!=OBJECT3D_KIND_PLANE) ||
        (leaf->kind!=OBJECT3D_KIND_RECT_PRISM && leaf->kind!=OBJECT3D_KIND_PLANE))return refused(__LINE__);
    PlaneFrame3 f=frame(fixed),m=frame(leaf);
    *a=Vec3_Add(Vec3_Scale(f.origin,Layout_WorldScale(l)),offset(f,c->telescoping_stay.fixed_offset_m));
    *b=Vec3_Add(Vec3_Scale(m.origin,Layout_WorldScale(l)),offset(m,c->telescoping_stay.leaf_offset_m));return true;
}
static Vec3 euler(Vec3 x,Vec3 y,Vec3 z){
    double pitch=atan2(-x.z,hypot(x.x,x.y)),roll,yaw;
    if(fabs(cos(pitch))>1e-6){roll=atan2(y.z,z.z);yaw=atan2(x.y,x.x);}else{roll=atan2(-z.y,y.y);yaw=0;}
    return (Vec3){roll*57.295779513,pitch*57.295779513,yaw*57.295779513};
}
static bool poses(const Layout* l,const LayoutConstraint* c,Vec3 p[6],Vec3 r[6]){
    Vec3 a,b;if(!Layout_TelescopingStayAnchors(l,c,&a,&b))return refused(__LINE__);
    const LayoutTelescopingStay* stay=&c->telescoping_stay;
    double distance=Vec3_Distance(a,b),insertion=stay->length_m[0]+stay->length_m[1]-distance;
    if(distance<=1e-6 || insertion<stay->insertion_min_m-1e-6 || insertion>stay->insertion_max_m+1e-6)return refused(__LINE__);
    Vec3 x=Vec3_Scale(Vec3_Sub(b,a),1/distance),y=Vec3_Scale(c->axis,1/Vec3_Length(c->axis));
    /* The hinge plane also fixes roll, so body and rod share a stable rigid basis. */
    if(fabs(Vec3_Dot(x,y))>1e-5)return refused(__LINE__);
    Vec3 rotation=euler(x,y,Vec3_Cross(x,y));
    p[0]=Vec3_Add(a,Vec3_Scale(x,stay->length_m[0]*.5));
    p[1]=Vec3_Sub(b,Vec3_Scale(x,stay->length_m[1]*.5));p[2]=a;p[3]=b;
    PlaneFrame3 f=frame(entity(l,c->a.entity_id)),m=frame(entity(l,c->b.entity_id));
    p[4]=Vec3_Add(a,offset(f,stay->fixed_plate_offset_m));p[5]=Vec3_Add(b,offset(m,stay->leaf_plate_offset_m));
    for(int i=0;i<4;++i)r[i]=rotation;
    r[4]=euler(f.axisU,f.axisV,f.normal);r[5]=euler(m.axisU,m.axisV,m.normal);return true;
}
static bool matches(const Layout* l,const Object3D* o,Vec3 p,Vec3 r,double tolerance){
    Object3D wanted=*o;wanted.transform.position=Vec3_Scale(p,1/Layout_WorldScale(l));wanted.transform.rotationDeg=r;
    Vec3 a[8],b[8];if(!Layout_Object3D_ComputeMeshInstanceCorners(o,a)||!Layout_Object3D_ComputeMeshInstanceCorners(&wanted,b))return refused(__LINE__);
    for(int i=0;i<8;++i)if(Vec3_Distance(a[i],b[i])*Layout_WorldScale(l)>tolerance)return false;
    return true;
}
bool Layout_TelescopingStaysValidate(const Layout* l,bool check_pose){
    for(size_t i=0;i<l->objectStore.constraintCount;++i){
        const LayoutConstraint* c=&l->objectStore.constraints[i];const LayoutTelescopingStay* f=&c->telescoping_stay;if(!f->enabled)continue;
        if(c->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL || c->folding_brace.enabled)return refused(__LINE__);
        for(int k=0;k<3;++k)if(!isfinite(f->fixed_offset_m[k])||!isfinite(f->leaf_offset_m[k])||fabs(f->fixed_offset_m[k])>100||fabs(f->leaf_offset_m[k])>100 ||
            !isfinite(f->fixed_plate_offset_m[k])||!isfinite(f->leaf_plate_offset_m[k])||fabs(f->fixed_plate_offset_m[k])>.1||fabs(f->leaf_plate_offset_m[k])>.1)return refused(__LINE__);
        for(int k=0;k<2;++k)if(!isfinite(f->length_m[k])||f->length_m[k]<=1e-6||f->length_m[k]>100)return refused(__LINE__);
        if(!isfinite(f->insertion_min_m)||!isfinite(f->insertion_max_m)||f->insertion_min_m<=0||f->insertion_max_m<f->insertion_min_m ||
            f->insertion_max_m>=fmin(f->length_m[0],f->length_m[1]))return refused(__LINE__);
        Vec3 a,b,p[6],r[6];double shortest,longest;
        if(!Layout_TelescopingStayAnchors(l,c,&a,&b)||!Layout_AnchorSpanRange(l,c,a,b,&shortest,&longest) ||
            f->length_m[0]+f->length_m[1]-longest<f->insertion_min_m-1e-6 || f->length_m[0]+f->length_m[1]-shortest>f->insertion_max_m+1e-6)return refused(__LINE__);
        if(check_pose && !poses(l,c,p,r))return refused(__LINE__);
        for(int k=0;k<6;++k){
            const Object3D* o=entity(l,f->parts[k]);if(!o||o->kind!=OBJECT3D_KIND_MESH_ASSET_INSTANCE||o->info.reference||o->info.volume_role ||
                (c->motion_assembly[0]&&Layout_IsDescendant(&l->objectStore,o->info.parent_id,c->motion_assembly)))return refused(__LINE__);
            for(int j=0;j<k;++j)if(!strcmp(f->parts[k],f->parts[j]))return refused(__LINE__);
            for(size_t j=0;j<l->objectStore.constraintCount;++j){
                const LayoutConstraint* other=&l->objectStore.constraints[j];
                if(!strcmp(o->coreMeta.object_id,other->a.entity_id)||!strcmp(o->coreMeta.object_id,other->b.entity_id) ||
                    (j!=i&&Layout_MotionScopeContains(&l->objectStore,other,o)))return refused(__LINE__);
            }
            double scale=Layout_WorldScale(l);Vec3 lo=o->meshInstance.localBoundsMin,hi=o->meshInstance.localBoundsMax;
            if(fabs(o->transform.scale.x*scale-1)>1e-6||fabs(o->transform.scale.y*scale-1)>1e-6||fabs(o->transform.scale.z*scale-1)>1e-6||Vec3_Length(Vec3_Add(lo,hi))>1e-6)return refused(__LINE__);
            if(k<2&&fabs(hi.x-lo.x-f->length_m[k])>1e-6)return refused(__LINE__);
            if(check_pose&&!matches(l,o,p[k],r[k],2e-5))return refused(__LINE__);
        }
    }return true;
}
bool Layout_TelescopingStayApply(Layout* l,const LayoutConstraint* c){
    if(!c->telescoping_stay.enabled)return true;
    Vec3 p[6],r[6];if(!l->geometryEditActive||!poses(l,c,p,r))return refused(__LINE__);
    for(int i=0;i<6;++i){
        const Object3D* found=entity(l,c->telescoping_stay.parts[i]);if(!found)return refused(__LINE__);
        if(matches(l,found,p[i],r[i],1e-6))continue;if(found->coreMeta.flags.locked)return refused(__LINE__);
        Object3D* o=Layout_ObjectStore_Find(&l->objectStore,found->objectId);bool adjusted=false;o->transform.rotationDeg=r[i];
        if(!Layout_SetObject3DPosition(l,o->objectId,Vec3_Scale(p[i],1/Layout_WorldScale(l)),&adjusted)||adjusted)return refused(__LINE__);
    }return true;
}
