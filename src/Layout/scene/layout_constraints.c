#include "Layout/layout_routes.h"
#include "Layout/layout_constraints.h"
#include "Layout/layout_engineering.h"
#include "Layout/layout_relationships.h"
#include "Layout/layout_spatial.h"
#include "Layout/layout_motion.h"
#include "Core/global_state.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LayoutGeometryBeforePublish history_hook;
void Layout_SetGeometryHistoryHook(LayoutGeometryBeforePublish hook) { history_hook = hook; }
bool Layout_GeometryHistory(const Layout* layout, void* context) {
    (void)context;
    if (layout != &Global_Get()->layout || !history_hook) return true;
    if (layout->geometryGestureActive && layout->geometryGestureCaptured) return true;
    if (!history_hook(layout, NULL)) return false;
    if (layout->geometryGestureActive) Global_Get()->layout.geometryGestureCaptured=true;
    return true;
}
void Layout_BeginGeometryGesture(Layout* layout) {
    if (layout) { layout->geometryGestureActive=true; layout->geometryGestureCaptured=false; }
}
void Layout_EndGeometryGesture(Layout* layout) {
    if (layout) { layout->geometryGestureActive=false; layout->geometryGestureCaptured=false; }
}
static bool fail(char* message, size_t size, const char* text) {
    if (message && size) snprintf(message, size, "%s", text);
    return false;
}
static bool named(const char* text, size_t size) {
    return text && text[0] && memchr(text, 0, size);
}
static Object3D* entity(Layout* layout, const char* id) {
    for (size_t i=0; i<layout->objectStore.count; ++i) {
        Object3D* o=&layout->objectStore.items[i];
        if (!o->isDeleted && strcmp(o->coreMeta.object_id,id)==0) return o;
    }
    return NULL;
}
bool Layout_HasConstraintParticipant(const LayoutObjectStore* store, uint32_t id) {
    const Object3D* o=Layout_ObjectStore_FindConst(store,id);
    if (!o) return false;
    for (size_t i=0; i<store->constraintCount; ++i)
        if (!strcmp(store->constraints[i].a.entity_id,o->coreMeta.object_id) ||
            !strcmp(store->constraints[i].b.entity_id,o->coreMeta.object_id)) return true;
    return false;
}
bool Layout_CanDeleteObject(const LayoutObjectStore* store, uint32_t id) {
    const Object3D* object=Layout_ObjectStore_FindConst(store,id);
    if (object && Layout_RouteEntityReferenced(store,object->coreMeta.object_id)) {
        if (store==&Global_Get()->layout.objectStore)
            snprintf(Global_Get()->layout.geometryMessage,sizeof(Global_Get()->layout.geometryMessage),"Remove or reassign the object's route endpoints in Routes first.");
        return false;
    }
    if (object && Layout_FindMotionEnvelope(store,id)) {
        if (store==&Global_Get()->layout.objectStore)
            snprintf(Global_Get()->layout.geometryMessage,sizeof(Global_Get()->layout.geometryMessage),"Delete motion envelopes in Parts / Volumes to remove their provenance safely.");
        return false;
    }
    if (object && Layout_SpatialEntityReferenced(store,object->coreMeta.object_id)) {
        if (store==&Global_Get()->layout.objectStore)
            snprintf(Global_Get()->layout.geometryMessage,sizeof(Global_Get()->layout.geometryMessage),"Remove the object's checks or volume ownership in Parts first.");
        return false;
    }
    if (object && Layout_QueryRelationships(store,object->coreMeta.object_id,-1,0,NULL,0)) {
        if (store==&Global_Get()->layout.objectStore)
            snprintf(Global_Get()->layout.geometryMessage,sizeof(Global_Get()->layout.geometryMessage),"Remove the object's links in Parts before deleting or replacing it.");
        return false;
    }
    if (!Layout_HasConstraintParticipant(store,id)) return true;
    if (store == &Global_Get()->layout.objectStore)
        snprintf(Global_Get()->layout.geometryMessage, sizeof(Global_Get()->layout.geometryMessage),
                 "Remove the object's constraints before deleting or replacing it.");
    return false;
}
static bool travel_basis(const Layout* layout, const LayoutConstraint* c, double basis[3][3]) {
    for (int k=0; k<3; ++k) {
        LayoutGeometricReference ref=c->b;
        ref.kind=(LayoutReferenceKind)(LAYOUT_REFERENCE_AXIS_U+k);
        ref.face=OBJECT3D_FACE_NONE;
        LayoutResolvedReference resolved;
        if (Layout_ResolveReference(layout,&ref,&resolved)!=LAYOUT_MEASUREMENT_OK || !resolved.has_direction) return false;
        memcpy(basis[k],resolved.direction,sizeof(basis[k]));
    }
    return true;
}
static bool travel_contract(const LayoutConstraint* c) {
    if (!isfinite(c->travel_min) || !isfinite(c->travel_max) || !isfinite(c->travel_home) ||
        !isfinite(c->travel_max-c->travel_min) || c->travel_min>c->travel_max || c->target<c->travel_min || c->target>c->travel_max ||
        c->travel_home<c->travel_min || c->travel_home>c->travel_max) return false;
    double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z);
    if (!isfinite(length) || length<=1e-12) return false;
    double axis[3]={c->axis.x/length,c->axis.y/length,c->axis.z/length},dot=0;
    for (int k=0;k<3;++k) {
        if (!isfinite(c->travel_offset[k])) return false;
        dot+=axis[k]*c->travel_offset[k];
        for (int j=0;j<3;++j) if (!isfinite(c->travel_basis[k][j])) return false;
    }
    if (fabs(dot)>1e-6) return false;
    for (int k=0;k<3;++k) for (int j=0;j<3;++j) {
        double d=0; for (int n=0;n<3;++n) d+=c->travel_basis[k][n]*c->travel_basis[j][n];
        if (fabs(d-(k==j ? 1 : 0))>1e-5) return false;
    }
    return true;
}
static bool travel_on_rail(const Layout* layout, const LayoutConstraint* c) {
    LayoutResolvedReference a,b;
    double basis[3][3];
    if (!travel_contract(c) || !travel_basis(layout,c,basis) ||
        Layout_ResolveReference(layout,&c->a,&a)!=LAYOUT_MEASUREMENT_OK ||
        Layout_ResolveReference(layout,&c->b,&b)!=LAYOUT_MEASUREMENT_OK) return false;
    double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z);
    double axis[3]={c->axis.x/length,c->axis.y/length,c->axis.z/length},projection=0;
    for(int k=0;k<3;++k) projection+=(b.point_meters[k]-a.point_meters[k])*axis[k];
    for(int k=0;k<3;++k) {
        if (fabs(b.point_meters[k]-a.point_meters[k]-axis[k]*projection-c->travel_offset[k])>1e-6) return false;
        for(int j=0;j<3;++j) if(fabs(basis[k][j]-c->travel_basis[k][j])>1e-5) return false;
    }
    return true;
}
bool Layout_InitLinearTravel(const Layout* layout, LayoutConstraint* rule, double minimum, double maximum) {
    if (!layout || !rule) return false;
    LayoutConstraint c=*rule;
    c.kind=LAYOUT_CONSTRAINT_LINEAR_TRAVEL;
    LayoutResolvedReference a,b;
    LayoutMeasurementResult m=Layout_Measure(layout,&c.a,&c.b,LAYOUT_MEASURE_PROJECTED_DISTANCE,c.axis);
    if(m.status!=LAYOUT_MEASUREMENT_OK || !travel_basis(layout,&c,c.travel_basis) ||
       Layout_ResolveReference(layout,&c.a,&a)!=LAYOUT_MEASUREMENT_OK ||
       Layout_ResolveReference(layout,&c.b,&b)!=LAYOUT_MEASUREMENT_OK) return false;
    if(!isfinite(minimum) || !isfinite(maximum) || minimum>maximum || m.value<minimum-1e-6 || m.value>maximum+1e-6)return false;
    c.target=c.travel_home=fmax(minimum,fmin(maximum,m.value)); c.travel_min=minimum; c.travel_max=maximum;
    double length=hypot(hypot(c.axis.x,c.axis.y),c.axis.z);
    double axis[3]={c.axis.x/length,c.axis.y/length,c.axis.z/length};
    for(int k=0;k<3;++k)c.travel_offset[k]=b.point_meters[k]-a.point_meters[k]-axis[k]*m.value;
    if(!travel_contract(&c))return false;
    *rule=c;return true;
}
/* The hinge captures a complete orientation, not only a direction: twist and
 * out-of-plane edits cannot silently become extra degrees of freedom. */
static bool hinge_contract(const LayoutConstraint* c) {
    if (!isfinite(c->travel_min) || !isfinite(c->travel_max) || !isfinite(c->travel_home) ||
        c->travel_min<=-180 || c->travel_max>180 || c->travel_min>c->travel_max ||
        c->target<c->travel_min || c->target>c->travel_max ||
        c->travel_home<c->travel_min || c->travel_home>c->travel_max) return false;
    double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z),dot=0,norm=0;
    if (!isfinite(length) || length<=1e-12) return false;
    double axis[]={c->axis.x/length,c->axis.y/length,c->axis.z/length};
    for (int k=0;k<3;++k) {
        if (!isfinite(c->hinge_reference[k])) return false;
        dot+=axis[k]*c->hinge_reference[k]; norm+=c->hinge_reference[k]*c->hinge_reference[k];
        for (int j=0;j<3;++j) if (!isfinite(c->travel_basis[k][j])) return false;
    }
    if (fabs(dot)>1e-5 || fabs(norm-1)>1e-5) return false;
    for (int k=0;k<3;++k) for (int j=0;j<3;++j) {
        double d=0;for (int n=0;n<3;++n)d+=c->travel_basis[k][n]*c->travel_basis[j][n];
        if (fabs(d-(k==j ? 1 : 0))>1e-5) return false;
    }
    const double (*b)[3]=c->travel_basis;
    double det=b[0][0]*(b[1][1]*b[2][2]-b[1][2]*b[2][1])-
        b[0][1]*(b[1][0]*b[2][2]-b[1][2]*b[2][0])+b[0][2]*(b[1][0]*b[2][1]-b[1][1]*b[2][0]);
    return fabs(det-1)<1e-5;
}
static bool hinge_orientation(const Layout* layout,const LayoutConstraint* c) {
    LayoutResolvedReference a;
    double basis[3][3];
    if (!hinge_contract(c) || !travel_basis(layout,c,basis) ||
        Layout_ResolveReference(layout,&c->a,&a)!=LAYOUT_MEASUREMENT_OK || !a.has_direction) return false;
    double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z);
    double axis[]={c->axis.x/length,c->axis.y/length,c->axis.z/length};
    const double* home=c->hinge_reference;const double* now=a.direction;
    double cross[]={home[1]*now[2]-home[2]*now[1],home[2]*now[0]-home[0]*now[2],home[0]*now[1]-home[1]*now[0]};
    double dot=0,signed_cross=0;
    for(int k=0;k<3;++k){dot+=home[k]*now[k];signed_cross+=axis[k]*cross[k];}
    double angle=atan2(signed_cross,dot)+(c->target-c->travel_home)*3.14159265358979323846/180;
    double co=cos(angle),si=sin(angle);
    for(int j=0;j<3;++j) {
        const double* v=c->travel_basis[j];double av=0;for(int k=0;k<3;++k)av+=axis[k]*v[k];
        double axv[]={axis[1]*v[2]-axis[2]*v[1],axis[2]*v[0]-axis[0]*v[2],axis[0]*v[1]-axis[1]*v[0]};
        for(int k=0;k<3;++k)if(fabs(basis[j][k]-(v[k]*co+axv[k]*si+axis[k]*av*(1-co)))>1e-5)return false;
    }
    return true;
}
bool Layout_InitAngularTravel(const Layout* layout,LayoutConstraint* rule,double minimum,double maximum) {
    if(!layout || !rule)return false;
    LayoutConstraint c=*rule;c.kind=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL;
    LayoutResolvedReference a;
    LayoutMeasurementResult angle=Layout_Measure(layout,&c.a,&c.b,LAYOUT_MEASURE_PLANAR_ANGLE,c.axis);
    if(angle.status!=LAYOUT_MEASUREMENT_OK || !travel_basis(layout,&c,c.travel_basis) ||
       Layout_ResolveReference(layout,&c.a,&a)!=LAYOUT_MEASUREMENT_OK || !a.has_direction)return false;
    if(!isfinite(minimum) || !isfinite(maximum) || angle.value<minimum-1e-4 || angle.value>maximum+1e-4)return false;
    c.target=c.travel_home=fmax(minimum,fmin(maximum,angle.value));c.travel_min=minimum;c.travel_max=maximum;
    memcpy(c.hinge_reference,a.direction,sizeof(c.hinge_reference));
    if(!hinge_contract(&c))return false;
    *rule=c;return true;
}
/* One incoming rule per entity gives a deterministic directed forest. Sort rules
 * by dependency rather than their insertion/storage order. */
static bool order_rules(const Layout* layout, size_t order[LAYOUT_MAX_CONSTRAINTS], char* message, size_t size) {
    const LayoutObjectStore* store=&layout->objectStore;
    if (store->constraintCount > LAYOUT_MAX_CONSTRAINTS) return fail(message,size,"Too many constraints.");
    if (!Layout_ValidateMotionScopes(layout,message,size)) return false;
    bool done[LAYOUT_MAX_CONSTRAINTS]={0};
    for (size_t i=0; i<store->constraintCount; ++i) {
        const LayoutConstraint* c=&store->constraints[i];
        LayoutResolvedReference a,b;
        if (!named(c->id,sizeof(c->id)) || c->kind < LAYOUT_CONSTRAINT_DISTANCE || c->kind > LAYOUT_CONSTRAINT_ANGULAR_TRAVEL ||
            !isfinite(c->target) || !isfinite(c->axis.x) || !isfinite(c->axis.y) || !isfinite(c->axis.z) ||
            (c->kind != LAYOUT_CONSTRAINT_COINCIDENT && hypot(hypot(c->axis.x,c->axis.y),c->axis.z)<=1e-12) ||
            (c->kind == LAYOUT_CONSTRAINT_PLANAR_MATE && (c->target<=-180 || c->target>180)) ||
            Layout_ResolveReference(layout,&c->a,&a)!=LAYOUT_MEASUREMENT_OK ||
            Layout_ResolveReference(layout,&c->b,&b)!=LAYOUT_MEASUREMENT_OK)
            return fail(message,size,"Constraint has invalid or unresolved operands, axis or target.");
        if (c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL && !travel_contract(c)) return fail(message,size,"Travel needs finite ordered limits containing both position and reset pose, and a valid rail frame.");
        if(c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL && !hinge_contract(c)) return fail(message,size,"Hinge needs ordered degree limits in (-180,180] containing position and reset, and a valid captured frame.");
        if (!strcmp(c->a.entity_id,c->b.entity_id)) return fail(message,size,"A constraint needs two different objects.");
        for (size_t j=0; j<i; ++j) {
            if (!strcmp(c->id,store->constraints[j].id)) return fail(message,size,"Duplicate constraint ID.");
            if (!strcmp(c->b.entity_id,store->constraints[j].b.entity_id)) return fail(message,size,"A dependent may have only one driving rule.");
        }
    }
    for (size_t n=0; n<store->constraintCount; ++n) {
        size_t ready=store->constraintCount;
        for (size_t i=0; i<store->constraintCount; ++i) {
            if (done[i]) continue;
            bool blocked=false;
            for (size_t j=0; j<store->constraintCount; ++j)
                if (!done[j] && !strcmp(store->constraints[i].a.entity_id,store->constraints[j].b.entity_id)) blocked=true;
            if (!blocked) { ready=i; break; }
        }
        if (ready==store->constraintCount) return fail(message,size,"Constraint cycle: choose a grounded driver.");
        order[n]=ready; done[ready]=true;
    }
    return true;
}
LayoutConstraintFeedback Layout_ConstraintFeedback(const Layout* layout, const LayoutConstraint* c) {
    LayoutConstraintFeedback f={0};
    f.position.status=f.angle.status=LAYOUT_MEASUREMENT_INVALID;
    if (!c) return f;
    LayoutMeasurementKind kind=(c->kind==LAYOUT_CONSTRAINT_DISTANCE || c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) ? LAYOUT_MEASURE_PROJECTED_DISTANCE : LAYOUT_MEASURE_POINT_DISTANCE;
    f.position=Layout_Measure(layout,&c->a,&c->b,kind,c->axis);
    double target=(c->kind==LAYOUT_CONSTRAINT_DISTANCE || c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) ? c->target : 0;
    f.satisfied=isfinite(target) && f.position.status==LAYOUT_MEASUREMENT_OK &&
        fabs(f.position.value-target)<=1e-6+1e-7*fabs(target);
    if (c->kind==LAYOUT_CONSTRAINT_PLANAR_MATE || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) {
        f.angle=Layout_Measure(layout,&c->a,&c->b,LAYOUT_MEASURE_PLANAR_ANGLE,c->axis);
        f.satisfied=f.satisfied && isfinite(c->target) && c->target>-180 && c->target<=180 &&
            f.angle.status==LAYOUT_MEASUREMENT_OK && fabs(remainder(f.angle.value-c->target,360))<=1e-4;
        if(c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) f.satisfied=f.satisfied && hinge_orientation(layout,c);
    } else if (c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) f.satisfied=f.satisfied && travel_on_rail(layout,c);
    else if (c->kind!=LAYOUT_CONSTRAINT_DISTANCE && c->kind!=LAYOUT_CONSTRAINT_COINCIDENT) f.satisfied=false;
    return f;
}
static bool rule_satisfied(const Layout* layout, const LayoutConstraint* c) {
    return Layout_ConstraintFeedback(layout,c).satisfied;
}
bool Layout_ValidateConstraints(const Layout* layout, char* message, size_t size) {
    size_t order[LAYOUT_MAX_CONSTRAINTS];
    if (!layout || !order_rules(layout,order,message,size)) return false;
    for (size_t i=0; i<layout->objectStore.constraintCount; ++i)
        if (!rule_satisfied(layout,&layout->objectStore.constraints[i])) return fail(message,size,"Constraint does not match stored geometry.");
    return true;
}
static bool solve(Layout* layout, uint32_t edited, char* message, size_t size) {
    size_t order[LAYOUT_MAX_CONSTRAINTS];
    if (!order_rules(layout,order,message,size)) return false;
    for (size_t i=0; i<layout->objectStore.constraintCount; ++i) {
        const LayoutConstraint* c=&layout->objectStore.constraints[order[i]];
        if (rule_satisfied(layout,c)) continue;
        Object3D* b=entity(layout,c->b.entity_id);
        if (!b || b->coreMeta.flags.locked) return fail(message,size,"Dependent is locked; constraint cannot be maintained.");
        Object3D before_b=*b;
        if (b->objectId==edited) return fail(message,size,"Edit conflicts with the object's driving rule. Edit its target or driver instead.");
        if (c->kind==LAYOUT_CONSTRAINT_PLANAR_MATE || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) {
            LayoutMeasurementResult angle=Layout_Measure(layout,&c->a,&c->b,LAYOUT_MEASURE_PLANAR_ANGLE,c->axis);
            if (angle.status!=LAYOUT_MEASUREMENT_OK) return fail(message,size,"Planar mate directions must lie in the chosen plane.");
            Object3D baseline=*b; bool adjusted=false;
            if (!Layout_RotateObject3D(layout,b->objectId,c->axis,(float)remainder(c->target-angle.value,360),&baseline,&adjusted) || adjusted)
                return fail(message,size,"Mate rotation conflicts with geometry bounds or locks.");
        }
        LayoutResolvedReference a,rb;
        if (Layout_ResolveReference(layout,&c->a,&a)!=LAYOUT_MEASUREMENT_OK || Layout_ResolveReference(layout,&c->b,&rb)!=LAYOUT_MEASUREMENT_OK)
            return fail(message,size,"Could not resolve constraint after rotation.");
        double delta[3];
        for (int k=0; k<3; ++k) delta[k]=a.point_meters[k]-rb.point_meters[k];
        if (c->kind==LAYOUT_CONSTRAINT_DISTANCE) {
            double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z);
            double axis[3]={c->axis.x/length,c->axis.y/length,c->axis.z/length};
            double current=0;
            for (int k=0; k<3; ++k) current-=delta[k]*axis[k];
            for (int k=0; k<3; ++k) delta[k]=(c->target-current)*axis[k];
        }
        if (c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL) {
            double length=hypot(hypot(c->axis.x,c->axis.y),c->axis.z);
            double axis[3]={c->axis.x/length,c->axis.y/length,c->axis.z/length};
            for(int k=0;k<3;++k) delta[k]+=c->travel_offset[k]+axis[k]*c->target;
        }
        Vec3 center;
        if (!Layout_Object3D_ComputeVisualCenter(b,&center)) return fail(message,size,"Invalid dependent center.");
        double world[3]={center.x+delta[0]/Layout_WorldScale(layout),center.y+delta[1]/Layout_WorldScale(layout),center.z+delta[2]/Layout_WorldScale(layout)};
        for (int k=0; k<3; ++k) if (!isfinite(world[k]) || fabs(world[k])>FLT_MAX) return fail(message,size,"Constraint exceeds numeric range.");
        bool adjusted=false;
        if (!Layout_SetObject3DPosition(layout,b->objectId,(Vec3){(float)world[0],(float)world[1],(float)world[2]},&adjusted) || adjusted || !rule_satisfied(layout,c))
            return fail(message,size,"Constraint conflicts with bounds, plane locks or numeric precision.");
        if (!Layout_ApplyMotionScope(layout,c,&before_b)) return false;
    }
    return Layout_ValidateConstraints(layout,message,size);
}
bool Layout_SolveGeometryCandidate(Layout* candidate) {
    return candidate && candidate->geometryEditActive &&
        solve(candidate,0,candidate->geometryMessage,sizeof(candidate->geometryMessage));
}
bool Layout_RunGeometryEdit(Layout* layout, uint32_t edited, LayoutGeometryMutation mutate, void* context,
    LayoutGeometryBeforePublish before_publish, void* history_context) {
    if (!layout || !mutate || layout->geometryEditActive || layout->objectStore.constraintCount > LAYOUT_MAX_CONSTRAINTS ||
        layout->objectStore.assembly_count>LAYOUT_MAX_ASSEMBLIES || layout->objectStore.relationship_count>LAYOUT_MAX_RELATIONSHIPS || layout->objectStore.spatial_rule_count>LAYOUT_MAX_SPATIAL_RULES || layout->objectStore.motion_envelope_count>LAYOUT_MAX_MOTION_ENVELOPES || layout->objectStore.route_count>LAYOUT_MAX_ROUTES) return false;
    layout->geometryMessage[0]=0;
    const Object3D* object=Layout_ObjectStore_FindConst(&layout->objectStore,edited);
    if (object && object->coreMeta.flags.locked) return fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Object is locked.");
    Layout candidate=*layout;
    candidate.geometryEditActive=true;
    size_t bytes=layout->objectStore.count*sizeof(Object3D);
    candidate.objectStore.items=bytes ? malloc(bytes) : NULL;
    if (bytes && !candidate.objectStore.items) return fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Could not allocate geometry transaction.");
    if (bytes) memcpy(candidate.objectStore.items,layout->objectStore.items,bytes);
    bool ok=mutate(&candidate,context);
    if (ok && candidate.objectStore.count<layout->objectStore.count) ok=fail(candidate.geometryMessage,sizeof(candidate.geometryMessage),"Geometry commands cannot shrink the store.");
    if (!ok && candidate.geometryMessage[0]) snprintf(layout->geometryMessage,sizeof(layout->geometryMessage),"%s",candidate.geometryMessage);
    /* Direct edits of B may translate along its rail; all other degrees of
     * freedom remain constrained. The accepted pose updates the saved target. */
    if (ok && edited) for(size_t i=0;i<candidate.objectStore.constraintCount;++i) {
        LayoutConstraint* c=&candidate.objectStore.constraints[i];
        Object3D* b=entity(&candidate,c->b.entity_id);
        if((c->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL && c->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) || !b || b->objectId!=edited)continue;
        if(c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) {
            LayoutMeasurementResult angle=Layout_Measure(&candidate,&c->a,&c->b,LAYOUT_MEASURE_PLANAR_ANGLE,c->axis);
            if(angle.status!=LAYOUT_MEASUREMENT_OK || angle.value<c->travel_min-1e-4 || angle.value>c->travel_max+1e-4) {
                ok=fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Hinge rotation must stay in its plane and within Min and Max.");break;
            }
            c->target=fmax(c->travel_min,fmin(c->travel_max,angle.value));
            if(!rule_satisfied(&candidate,c)) {ok=fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Hinge permits rotation about its pivot only; translation and twist are fixed.");break;}
            continue;
        }
        LayoutMeasurementResult m=Layout_Measure(&candidate,&c->a,&c->b,LAYOUT_MEASURE_PROJECTED_DISTANCE,c->axis);
        if(m.status!=LAYOUT_MEASUREMENT_OK || m.value<c->travel_min-1e-6 || m.value>c->travel_max+1e-6 || !travel_on_rail(&candidate,c)) {
            ok=fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Travel permits translation along its rail within Min and Max; orientation stays fixed."); break;
        }
        c->target=fmax(c->travel_min,fmin(c->travel_max,m.value));
    }
    if (ok && edited && object) for (size_t i=0;i<candidate.objectStore.constraintCount;++i) {
        const LayoutConstraint* c=&candidate.objectStore.constraints[i];
        if (c->motion_assembly[0] && !strcmp(c->b.entity_id,object->coreMeta.object_id))
            ok=Layout_ApplyMotionScope(&candidate,c,object);
        if (!ok) {snprintf(layout->geometryMessage,sizeof(layout->geometryMessage),"%s",candidate.geometryMessage);break;}
    }
    if (ok) ok=Layout_ValidateEngineering(&candidate,layout->geometryMessage,sizeof(layout->geometryMessage));
    if (ok) ok=solve(&candidate,edited,layout->geometryMessage,sizeof(layout->geometryMessage));
    if (!ok && !layout->geometryMessage[0]) fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Geometry edit rejected.");
    bool changed=ok && (candidate.objectStore.route_count!=layout->objectStore.route_count ||
        candidate.objectStore.next_route_id!=layout->objectStore.next_route_id ||
        memcmp(candidate.objectStore.routes,layout->objectStore.routes,sizeof(layout->objectStore.routes)) ||
        candidate.objectStore.count!=layout->objectStore.count ||
        candidate.objectStore.nextObjectId!=layout->objectStore.nextObjectId ||
        (bytes && memcmp(candidate.objectStore.items,layout->objectStore.items,bytes)) ||
        candidate.objectStore.motion_envelope_count!=layout->objectStore.motion_envelope_count ||
        memcmp(candidate.objectStore.motion_envelopes,layout->objectStore.motion_envelopes,sizeof(layout->objectStore.motion_envelopes)) ||
        candidate.objectStore.spatial_rule_count!=layout->objectStore.spatial_rule_count ||
        candidate.objectStore.next_spatial_rule_id!=layout->objectStore.next_spatial_rule_id ||
        memcmp(candidate.objectStore.spatial_rules,layout->objectStore.spatial_rules,sizeof(layout->objectStore.spatial_rules)) ||
        candidate.objectStore.relationship_count!=layout->objectStore.relationship_count ||
        candidate.objectStore.next_relationship_id!=layout->objectStore.next_relationship_id ||
        memcmp(candidate.objectStore.relationships,layout->objectStore.relationships,sizeof(layout->objectStore.relationships)) ||
        candidate.objectStore.assembly_count!=layout->objectStore.assembly_count ||
        candidate.objectStore.next_assembly_id!=layout->objectStore.next_assembly_id ||
        memcmp(candidate.objectStore.assemblies,layout->objectStore.assemblies,sizeof(layout->objectStore.assemblies)) ||
        candidate.objectStore.constraintCount!=layout->objectStore.constraintCount ||
        candidate.objectStore.nextConstraintId!=layout->objectStore.nextConstraintId ||
        memcmp(candidate.objectStore.constraints,layout->objectStore.constraints,sizeof(layout->objectStore.constraints)));
    if (changed && before_publish && !before_publish(layout,history_context)) ok=fail(layout->geometryMessage,sizeof(layout->geometryMessage),"Could not reserve undo history.");
    if (ok && changed) {
        if (candidate.objectStore.count!=layout->objectStore.count) {
            free(layout->objectStore.items);
            layout->objectStore.items=candidate.objectStore.items;
            candidate.objectStore.items=NULL;
            layout->objectStore.count=candidate.objectStore.count;
        } else if (bytes) memcpy(layout->objectStore.items,candidate.objectStore.items,bytes);
        memcpy(layout->objectStore.routes,candidate.objectStore.routes,sizeof(layout->objectStore.routes));
        layout->objectStore.route_count=candidate.objectStore.route_count;
        layout->objectStore.next_route_id=candidate.objectStore.next_route_id;
        layout->objectStore.nextObjectId=candidate.objectStore.nextObjectId;
        memcpy(layout->objectStore.motion_envelopes,candidate.objectStore.motion_envelopes,sizeof(layout->objectStore.motion_envelopes));
        layout->objectStore.motion_envelope_count=candidate.objectStore.motion_envelope_count;
        memcpy(layout->objectStore.spatial_rules,candidate.objectStore.spatial_rules,sizeof(layout->objectStore.spatial_rules));
        layout->objectStore.spatial_rule_count=candidate.objectStore.spatial_rule_count;
        layout->objectStore.next_spatial_rule_id=candidate.objectStore.next_spatial_rule_id;
        memcpy(layout->objectStore.assemblies,candidate.objectStore.assemblies,sizeof(layout->objectStore.assemblies));
        memcpy(layout->objectStore.relationships,candidate.objectStore.relationships,sizeof(layout->objectStore.relationships));
        layout->objectStore.relationship_count=candidate.objectStore.relationship_count;
        layout->objectStore.next_relationship_id=candidate.objectStore.next_relationship_id;
        layout->objectStore.assembly_count=candidate.objectStore.assembly_count;
        layout->objectStore.next_assembly_id=candidate.objectStore.next_assembly_id;
        memcpy(layout->objectStore.constraints,candidate.objectStore.constraints,sizeof(layout->objectStore.constraints));
        layout->objectStore.constraintCount=candidate.objectStore.constraintCount;
        layout->objectStore.nextConstraintId=candidate.objectStore.nextConstraintId;
        if (layout==&Global_Get()->layout) { Global_FlagLayoutChanged(); Global_FlagHitboxesDirty(); }
    }
    free(candidate.objectStore.items);
    return ok;
}
static bool replace(Layout* layout, void* context) {
    const Object3D* candidate=context;
    Object3D* live=Layout_ObjectStore_Find(&layout->objectStore,candidate->objectId);
    if (!live || strcmp(live->coreMeta.object_id,candidate->coreMeta.object_id)) return false;
    *live=*candidate;
    return Layout_ObjectStore_ValidateObject(live);
}
bool Layout_ReplaceGeometryObject(Layout* layout, const Object3D* object, LayoutGeometryBeforePublish hook, void* context) {
    return object && Layout_RunGeometryEdit(layout,object->objectId,replace,(void*)object,hook,context);
}
typedef struct { const LayoutConstraint* rule; const char* remove; } RuleEdit;
static bool change_rule(Layout* layout, void* context) {
    RuleEdit* edit=context; LayoutObjectStore* s=&layout->objectStore;
    const char* id=edit->rule ? edit->rule->id : edit->remove;
    if (!id || (edit->rule && !memchr(id, 0, sizeof(edit->rule->id)))) return false;
    for (size_t i=0; i<s->constraintCount; ++i) if (!strcmp(id,s->constraints[i].id)) {
        if (edit->rule) s->constraints[i]=*edit->rule;
        else { memmove(&s->constraints[i],&s->constraints[i+1],(s->constraintCount-i-1)*sizeof(LayoutConstraint)); memset(&s->constraints[--s->constraintCount],0,sizeof(LayoutConstraint)); }
        return true;
    }
    if (!edit->rule || s->constraintCount>=LAYOUT_MAX_CONSTRAINTS) return false;
    LayoutConstraint rule=*edit->rule;
    if (!rule.id[0]) {
        if (!s->nextConstraintId) s->nextConstraintId=1;
        bool unique=false;
        while (!unique) {
            if (s->nextConstraintId==UINT32_MAX) return false;
            snprintf(rule.id,sizeof(rule.id),"constraint_%u",s->nextConstraintId++);
            unique=true;
            for (size_t i=0; i<s->constraintCount; ++i) if (!strcmp(rule.id,s->constraints[i].id)) unique=false;
        }
    }
    s->constraints[s->constraintCount++]=rule;
    return true;
}
bool Layout_ConstraintEdit(Layout* layout, const LayoutConstraint* rule, const char* remove_id, LayoutGeometryBeforePublish hook, void* context) {
    if (rule && layout == &Global_Get()->layout && Global_Get()->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_SCENE)
        return fail(layout->geometryMessage, sizeof(layout->geometryMessage), "Persistent rules require the Scene workspace; object-asset files cannot store them.");
    RuleEdit edit={rule,remove_id};
    return Layout_RunGeometryEdit(layout,0,change_rule,&edit,hook,context);
}

bool Layout_SetTravelPosition(Layout* layout, const char* id, double position, LayoutGeometryBeforePublish hook, void* context) {
    if (!layout || !id || !isfinite(position)) return false;
    for(size_t i=0;i<layout->objectStore.constraintCount;++i) {
        LayoutConstraint c=layout->objectStore.constraints[i];
        if(strcmp(c.id,id) || (c.kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL && c.kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL))continue;
        c.target=position;
        return Layout_ConstraintEdit(layout,&c,NULL,hook,context);
    }
    return false;
}
