#include "Layout/layout_motion.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const LayoutConstraint* movement(const Layout* l, const char* id) {
    if (!l || !id || l->objectStore.constraintCount>LAYOUT_MAX_CONSTRAINTS) return NULL;
    for (size_t i=0;i<l->objectStore.constraintCount;++i) {
        const LayoutConstraint* c=&l->objectStore.constraints[i];
        if (!strcmp(c->id,id) && (c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL)) return c;
    }
    return NULL;
}
static const Object3D* entity(const Layout* l, const char* id) {
    for (size_t i=0;i<l->objectStore.count;++i) {
        const Object3D* o=&l->objectStore.items[i];
        if (!o->isDeleted && !strcmp(o->coreMeta.object_id,id)) return o;
    }
    return NULL;
}
bool Layout_MotionTargetStatic(const Layout* l, const char* id, const Object3D* target) {
    const LayoutConstraint* c=movement(l,id);
    if (!c || !target || target->isDeleted || Layout_FindMotionEnvelope(&l->objectStore,target->objectId)) return false;
    /* A downstream follower is not a static obstacle. Follow the one-driver forest;
     * do not silently compare its authored pose against another moving member. */
    const Object3D* current=target;
    for (size_t n=0;n<=l->objectStore.constraintCount;++n) {
        if (Layout_MotionScopeContains(&l->objectStore,c,current)) return false;
        const LayoutConstraint* parent=NULL;
        for (size_t i=0;i<l->objectStore.constraintCount;++i)
            if (!strcmp(l->objectStore.constraints[i].b.entity_id,current->coreMeta.object_id)) parent=&l->objectStore.constraints[i];
        if (!parent) return true;
        current=entity(l,parent->a.entity_id);
        if (!current) return false;
    }
    return false;
}
/* A midpoint pose plus a point-displacement bound contains every intermediate
 * pose of this rigid one-axis movement. Each member gets its own radius/box.
 * Translation is bounded per coordinate; hinge arc length bounds displacement. */
static bool interval_bounds(const Layout* l, const LayoutConstraint* c, double low, double high,
    Object3D* poses, LayoutMotionBounds* bounds, double* displacement, size_t* count) {
    double midpoint=low*.5+high*.5,scale=Layout_WorldScale(l);
    if (!isfinite(scale) || scale<=0 || !Layout_SampleMotionSet(l,c->id,midpoint,poses,LAYOUT_MAX_MOTION_MEMBERS,count)) return false;
    const Object3D* b=NULL;
    for (size_t i=0;i<*count;++i) if (!strcmp(poses[i].coreMeta.object_id,c->b.entity_id)) b=&poses[i];
    if (!b) return false;
    Layout isolated=*l;isolated.objectStore.items=(Object3D*)b;isolated.objectStore.count=1;
    LayoutResolvedReference pivot;
    if (Layout_ResolveReference(&isolated,&c->b,&pivot)!=LAYOUT_MEASUREMENT_OK) return false;
    double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z),axis[]={c->axis.x/length,c->axis.y/length,c->axis.z/length};
    if (!isfinite(length) || length<=1e-12) return false;
    for (size_t i=0;i<*count;++i) {
        Vec3 corners[8];const Object3D* o=&poses[i];LayoutMotionBounds* box=&bounds[i];
        if (!(o->kind==OBJECT3D_KIND_PLANE?Layout_Object3D_ComputePlaneCorners(o,corners):Layout_Object3D_ComputeRectPrismCorners(o,corners))) return false;
        double radius=0,magnitude=0;
        for (int k=0;k<3;++k) {box->min_meters[k]=DBL_MAX;box->max_meters[k]=-DBL_MAX;}
        for (int j=0;j<(o->kind==OBJECT3D_KIND_PLANE?4:8);++j) {
            double p[]={corners[j].x*scale,corners[j].y*scale,corners[j].z*scale},r2=0;
            for (int k=0;k<3;++k) {
                if (!isfinite(p[k])) return false;
                box->min_meters[k]=fmin(box->min_meters[k],p[k]);box->max_meters[k]=fmax(box->max_meters[k],p[k]);
                magnitude=fmax(magnitude,fabs(p[k]));double d=p[k]-pivot.point_meters[k];r2+=d*d;
            }
            radius=fmax(radius,sqrt(r2));
        }
        double span=(high-low)*.5;
        double arc=c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?radius*span*.017453292519943295:0;
        double guard=1e-5+64*FLT_EPSILON*fmax(1,magnitude+radius+arc);
        if (displacement) displacement[i]=(c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?arc:span)+sqrt(3)*guard;
        for (int k=0;k<3;++k) {
            double padding=guard+(c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?arc:fabs(axis[k])*span);
            box->min_meters[k]-=padding;box->max_meters[k]+=padding;
            if (!isfinite(box->min_meters[k]) || !isfinite(box->max_meters[k])) return false;
        }
    }
    return true;
}
void Layout_FreeMotionCoverage(LayoutMotionCoverage* coverage) {
    if (coverage) {free(coverage->pieces);memset(coverage,0,sizeof(*coverage));}
}
bool Layout_BuildMotionCoverage(const Layout* l, const LayoutMotionEnvelope* e, LayoutMotionCoverage* out) {
    if (!out || !Layout_MotionEnvelopeCurrent(l,e)) return false;
    const LayoutConstraint* c=movement(l,e->rule_id);if (!c || e->samples<2 || e->samples>129) return false;
    LayoutMotionCoverage coverage={0};size_t members=c->motion_assembly[0]?e->member_count:1;
    if (!members || members>LAYOUT_MAX_MOTION_MEMBERS) return false;
    coverage.pieces=calloc(members*(e->samples-1),sizeof(*coverage.pieces));
    Object3D* poses=calloc(LAYOUT_MAX_MOTION_MEMBERS,sizeof(*poses));
    if (!coverage.pieces || !poses) {free(poses);Layout_FreeMotionCoverage(&coverage);return false;}
    for (uint32_t i=0;i+1<e->samples;++i) {
        double low=c->travel_min+(c->travel_max-c->travel_min)*(double)i/(e->samples-1);
        double high=c->travel_min+(c->travel_max-c->travel_min)*(double)(i+1)/(e->samples-1);size_t count;
        if (!interval_bounds(l,c,low,high,poses,coverage.pieces+coverage.count,NULL,&count) || count!=members) {
            free(poses);Layout_FreeMotionCoverage(&coverage);return false;
        }
        coverage.count+=count;
    }
    free(poses);snprintf(coverage.rule_id,64,"%s",c->id);*out=coverage;return true;
}
bool Layout_MotionCoverageDistance(const Layout* l, const LayoutMotionCoverage* coverage,
    const Object3D* target, double* lower, bool* contact) {
    if (!l || !coverage || !memchr(coverage->rule_id,0,sizeof(coverage->rule_id)) || !coverage->pieces || !coverage->count || coverage->count>LAYOUT_MAX_MOTION_MEMBERS*128 ||
        !lower || !contact || !Layout_MotionTargetStatic(l,coverage->rule_id,target)) return false;
    Vec3 min,max;double scale=Layout_WorldScale(l);
    if (!Layout_Object3D_ComputeWorldAABB(target,&min,&max)) return false;
    double target_min[]={min.x*scale,min.y*scale,min.z*scale},target_max[]={max.x*scale,max.y*scale,max.z*scale};
    for (int k=0;k<3;++k) if (!isfinite(target_min[k]) || !isfinite(target_max[k]) || target_min[k]>target_max[k]) return false;
    *lower=DBL_MAX;*contact=false;
    for (size_t i=0;i<coverage->count;++i) {
        double coarse_squared=0;
        for (int k=0;k<3;++k) {
            if (!isfinite(coverage->pieces[i].min_meters[k]) || !isfinite(coverage->pieces[i].max_meters[k]) ||
                coverage->pieces[i].min_meters[k]>coverage->pieces[i].max_meters[k]) return false;
            double d=fmax(0,fmax(target_min[k]-coverage->pieces[i].max_meters[k],coverage->pieces[i].min_meters[k]-target_max[k]));
            coarse_squared+=d*d;
        }
        /* A target AABB gap is itself a lower bound: skip only pieces that cannot
         * improve the already measured minimum, retaining the contact tolerance. */
        if (sqrt(coarse_squared)>*lower+1e-6) continue;
        double gap;bool overlap;
        if (!Layout_SpatialBoundsDistance(l,coverage->pieces[i].min_meters,coverage->pieces[i].max_meters,target,&gap,&overlap)) return false;
        *lower=fmin(*lower,gap);*contact=*contact || overlap;
        if (*contact) break;
    }
    return true;
}

typedef struct {
    const Layout* layout;
    const LayoutConstraint* rule;
    const Object3D* target;
    Object3D* poses;
    LayoutMotionBounds bounds[LAYOUT_MAX_MOTION_MEMBERS];
    double displacement[LAYOUT_MAX_MOTION_MEMBERS];
    uint32_t budget;
    double required;
    bool clearance;
    LayoutMotionRangeResult result;
} RangeCheck;
static bool check_endpoint(RangeCheck* check, double position) {
    size_t count;
    if (check->result.tested_poses>=check->budget) return true;
    if (!Layout_SampleMotionSet(check->layout,check->rule->id,position,check->poses,LAYOUT_MAX_MOTION_MEMBERS,&count)) return false;
    ++check->result.tested_poses;
    for (size_t i=0;i<count;++i) {
        double gap;bool contact,approximate;
        if (!Layout_SpatialDistance(check->layout,&check->poses[i],check->target,&gap,&contact,&approximate) || approximate) return false;
        if (check->clearance?gap+1e-6<check->required:contact) {
            check->result.status=LAYOUT_MOTION_RANGE_FAILURE;check->result.position=position;check->result.gap_meters=gap;
            check->result.interval_min=check->result.interval_max=position;
            snprintf(check->result.member_id,64,"%s",check->poses[i].coreMeta.object_id);return true;
        }
    }
    return true;
}
static void unresolved(RangeCheck* check, double low, double high) {
    if (check->result.status==LAYOUT_MOTION_RANGE_CLEAR) {
        check->result.status=LAYOUT_MOTION_RANGE_UNRESOLVED;
        check->result.interval_min=low;check->result.interval_max=high;check->result.position=low*.5+high*.5;
    }
}
static bool check_interval(RangeCheck* check, double low, double high, unsigned depth) {
    double midpoint=low*.5+high*.5;size_t count;
    if (check->result.tested_poses>=check->budget) {
        unresolved(check,low,high);return true;
    }
    if (!interval_bounds(check->layout,check->rule,low,high,check->poses,check->bounds,check->displacement,&count)) return false;
    ++check->result.tested_poses;bool separated=true;double lower=DBL_MAX;
    for (size_t i=0;i<count;++i) {
        double gap;bool hit,approximate;
        if (!Layout_SpatialDistance(check->layout,&check->poses[i],check->target,&gap,&hit,&approximate) || approximate) return false;
        if (check->clearance?gap+1e-6<check->required:hit) {
            check->result.status=LAYOUT_MOTION_RANGE_FAILURE;check->result.position=midpoint;check->result.gap_meters=gap;
            check->result.interval_min=low;check->result.interval_max=high;
            snprintf(check->result.member_id,64,"%s",check->poses[i].coreMeta.object_id);return true;
        }
        /* Distance to a static set is 1-Lipschitz under point displacement.
         * This oriented-gap bound avoids endless subdivision inside the corners
         * of an instantaneous AABB. Both bounds are valid; use the stronger one. */
        double oriented_lower=fmax(0,gap-check->displacement[i]);
        if (!Layout_SpatialBoundsDistance(check->layout,check->bounds[i].min_meters,check->bounds[i].max_meters,check->target,&gap,&hit)) return false;
        gap=fmax(gap,oriented_lower);lower=fmin(lower,gap);
        if (gap<=check->required+1e-6) separated=false;
    }
    if (separated) {check->result.gap_meters=fmin(check->result.gap_meters,lower);return true;}
    if (depth>=12 || midpoint==low || midpoint==high) {
        unresolved(check,low,high);return true;
    }
    if (!check_interval(check,low,midpoint,depth+1)) return false;
    if (check->result.status==LAYOUT_MOTION_RANGE_FAILURE) return true;
    return check_interval(check,midpoint,high,depth+1);
}
bool Layout_CheckMotionRange(const Layout* l, const LayoutMotionEnvelope* e,
    const char* target_id, double required, bool clearance, uint32_t budget, LayoutMotionRangeResult* out) {
    if (!out || !l || !e || !target_id || !isfinite(required) || required<0 || budget<1 || budget>4097 || !Layout_MotionEnvelopeCurrent(l,e)) return false;
    const LayoutConstraint* c=movement(l,e->rule_id);const Object3D* target=entity(l,target_id);
    if (!c || !target || !Layout_MotionTargetStatic(l,c->id,target) || (target->kind!=OBJECT3D_KIND_PLANE && target->kind!=OBJECT3D_KIND_RECT_PRISM)) return false;
    RangeCheck check={.layout=l,.rule=c,.target=target,.budget=budget,.required=clearance?required:0,.clearance=clearance,
        .result={.status=LAYOUT_MOTION_RANGE_CLEAR,.gap_meters=DBL_MAX}};
    check.poses=calloc(LAYOUT_MAX_MOTION_MEMBERS,sizeof(*check.poses));if (!check.poses) return false;
    bool ok=true;
    if (budget>=3) {
        ok=check_endpoint(&check,c->travel_min);
        if (ok && check.result.status!=LAYOUT_MOTION_RANGE_FAILURE && c->travel_min!=c->travel_max) ok=check_endpoint(&check,c->travel_max);
    }
    if (ok && check.result.status!=LAYOUT_MOTION_RANGE_FAILURE) ok=check_interval(&check,c->travel_min,c->travel_max,0);
    free(check.poses);
    if (!ok) return false;
    if (check.result.status==LAYOUT_MOTION_RANGE_CLEAR) {check.result.interval_min=c->travel_min;check.result.interval_max=c->travel_max;check.result.position=c->travel_min;}
    if (check.result.status==LAYOUT_MOTION_RANGE_UNRESOLVED) check.result.gap_meters=0;
    *out=check.result;return true;
}
