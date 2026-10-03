#include "Layout/layout_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fail(char* message, size_t capacity, const char* reason) {
    if (message && capacity) snprintf(message,capacity,"%s",reason);
    return false;
}
bool Layout_MotionScopeContains(const LayoutObjectStore* s, const LayoutConstraint* c, const Object3D* o) {
    return s && c && o && !o->isDeleted && (c->motion_assembly[0] ?
        Layout_IsDescendant(s,o->info.parent_id,c->motion_assembly) : !strcmp(o->coreMeta.object_id,c->b.entity_id));
}
bool Layout_ValidateMotionScopes(const Layout* l, char* message, size_t capacity) {
    if (!l || l->objectStore.constraintCount>LAYOUT_MAX_CONSTRAINTS) return false;
    const LayoutObjectStore* s=&l->objectStore;
    for (size_t i=0;i<s->constraintCount;++i) {
        const LayoutConstraint* c=&s->constraints[i];
        if (!memchr(c->motion_assembly,0,sizeof(c->motion_assembly))) return fail(message,capacity,"Invalid motion scope ID.");
        if (!c->motion_assembly[0]) continue;
        if ((c->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL && c->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) || !Layout_FindAssembly(s,c->motion_assembly))
            return fail(message,capacity,"Motion scope requires a saved Travel/Hinge and an existing assembly.");
        size_t count=0;bool has_b=false;
        for (size_t j=0;j<s->count;++j) {
            const Object3D* o=&s->items[j];if (!Layout_MotionScopeContains(s,c,o)) continue;
            ++count;has_b=has_b || !strcmp(o->coreMeta.object_id,c->b.entity_id);
            if (o->info.reference || o->info.volume_role || (o->kind!=OBJECT3D_KIND_PLANE && o->kind!=OBJECT3D_KIND_RECT_PRISM))
                return fail(message,capacity,"Moving assemblies support design panels/prisms only; move reference/reserved/mesh parts outside this scope.");
            if (!strcmp(o->coreMeta.object_id,c->a.entity_id)) return fail(message,capacity,"Fixed A must be outside the moving assembly.");
            for (size_t k=0;k<s->constraintCount;++k) if (k!=i &&
                (!strcmp(o->coreMeta.object_id,s->constraints[k].a.entity_id) || !strcmp(o->coreMeta.object_id,s->constraints[k].b.entity_id)))
                return fail(message,capacity,"Moving assembly members cannot have other geometric rules in this slice. Remove those rules or choose B only.");
        }
        if (!has_b || count>LAYOUT_MAX_MOTION_MEMBERS) return fail(message,capacity,"Choose an assembly containing B and at most 64 design parts.");
    }
    return true;
}
static PlaneFrame3 frame(const Object3D* o) {return o->kind==OBJECT3D_KIND_PLANE?o->plane.frame:o->rectPrism.frame;}
static Vec3 map_vector(PlaneFrame3 before, PlaneFrame3 after, Vec3 value) {
    return Vec3_Add(Vec3_Add(Vec3_Scale(after.axisU,Vec3_Dot(before.axisU,value)),
        Vec3_Scale(after.axisV,Vec3_Dot(before.axisV,value))),Vec3_Scale(after.normal,Vec3_Dot(before.normal,value)));
}
/* Called only inside a candidate transaction, after B acquired its requested pose.
 * The same rigid delta moves peers and nested assembly frames; no live history here. */
bool Layout_ApplyMotionScope(Layout* l, const LayoutConstraint* c, const Object3D* before_b) {
    if (!c->motion_assembly[0]) return true;
    if (!l->geometryEditActive) return false;
    LayoutObjectStore* s=&l->objectStore;const Object3D* b=Layout_ObjectStore_FindConst(s,before_b->objectId);
    if (!b) return false;
    PlaneFrame3 before=frame(before_b),after=frame(b);
    if (!memcmp(&before,&after,sizeof(before))) return true;
    Vec3 axis=c->axis;float length=Vec3_Length(axis);if (length<=0) return false;axis=Vec3_Scale(axis,1/length);
    float angle=0;
    if (c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL) {
        const Vec3 candidates[]={before.axisU,before.axisV,before.normal};Vec3 v=candidates[0];
        for (int i=1;i<3;++i) if (fabsf(Vec3_Dot(candidates[i],axis))<fabsf(Vec3_Dot(v,axis))) v=candidates[i];
        Vec3 w=map_vector(before,after,v);
        angle=atan2f(Vec3_Dot(axis,Vec3_Cross(v,w)),Vec3_Dot(v,w))*57.29577951308232f;
    }
    for (size_t i=0;i<s->count;++i) {
        Object3D* o=&s->items[i];if (o->objectId==b->objectId || !Layout_MotionScopeContains(s,c,o)) continue;
        if (o->coreMeta.flags.locked) return fail(l->geometryMessage,sizeof(l->geometryMessage),"Moving assembly has a locked member.");
        Vec3 position=Vec3_Add(after.origin,map_vector(before,after,Vec3_Sub(o->transform.position,before.origin)));
        bool adjusted=false;
        if (fabsf(angle)>1e-6f) {
            Object3D baseline=*o;
            if (!Layout_RotateObject3D(l,o->objectId,axis,angle,&baseline,&adjusted) || adjusted)
                return fail(l->geometryMessage,sizeof(l->geometryMessage),"Assembly motion conflicts with member rotation locks or bounds.");
        }
        if (!Layout_SetObject3DPosition(l,o->objectId,position,&adjusted) || adjusted ||
            Vec3_Distance(o->transform.position,position)*Layout_WorldScale(l)>1e-5)
            return fail(l->geometryMessage,sizeof(l->geometryMessage),"Assembly motion conflicts with member position locks, bounds or precision.");
    }
    for (size_t i=0;i<s->assembly_count;++i) {
        LayoutAssembly* a=&s->assemblies[i];
        if (strcmp(a->id,c->motion_assembly) && !Layout_IsDescendant(s,a->info.parent_id,c->motion_assembly)) continue;
        a->frame.origin=Vec3_Add(after.origin,map_vector(before,after,Vec3_Sub(a->frame.origin,before.origin)));
        a->frame.axisU=map_vector(before,after,a->frame.axisU);a->frame.axisV=map_vector(before,after,a->frame.axisV);a->frame.normal=map_vector(before,after,a->frame.normal);
    }
    return true;
}
