#include "Layout/layout_motion.h"
#include "Core/global_state.h"
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
    for (size_t i=0;i<l->objectStore.count;++i)
        if (!l->objectStore.items[i].isDeleted && !strcmp(l->objectStore.items[i].coreMeta.object_id,id)) return &l->objectStore.items[i];
    return NULL;
}
const LayoutMotionEnvelope* Layout_FindMotionEnvelope(const LayoutObjectStore* s, uint32_t id) {
    if (!s) return NULL;
    for (size_t i=0;i<s->motion_envelope_count && i<LAYOUT_MAX_MOTION_ENVELOPES;++i) if (s->motion_envelopes[i].object_id==id) return &s->motion_envelopes[i];
    return NULL;
}
const LayoutMotionEnvelope* Layout_FindRuleEnvelope(const LayoutObjectStore* s, const char* id) {
    if (!s || !id) return NULL;
    for (size_t i=0;i<s->motion_envelope_count && i<LAYOUT_MAX_MOTION_ENVELOPES;++i) if (!strcmp(s->motion_envelopes[i].rule_id,id)) return &s->motion_envelopes[i];
    return NULL;
}
bool Layout_SampleMotion(const Layout* l, const char* id, double position, Object3D* pose) {
    const LayoutConstraint* c=movement(l,id);
    if (!c || !pose || !Layout_ValidateConstraints(l,NULL,0)) return false;
    const Object3D* b=entity(l,c->b.entity_id);
    if (!b || b->info.volume_role || b->info.reference) return false;
    Layout copy=*l;size_t bytes=l->objectStore.count*sizeof(Object3D);
    copy.objectStore.items=malloc(bytes);
    if (!copy.objectStore.items) return false;
    memcpy(copy.objectStore.items,l->objectStore.items,bytes);
    copy.geometryEditActive=copy.geometryGestureActive=copy.geometryGestureCaptured=false;
    bool ok=Layout_SetTravelPosition(&copy,id,position,NULL,NULL);
    const Object3D* sampled=entity(&copy,c->b.entity_id);
    if (ok && sampled) *pose=*sampled;else ok=false;
    free(copy.objectStore.items);return ok;
}
/* Textual scalar hashing avoids padding bytes and survives a JSON round trip.
 * Target and current B pose are intentionally absent: moving within the saved
 * range must not stale the envelope. Driver/reference/shape/range changes do. */
static void hash_text(uint64_t* h, const char* text) {
    do {*h^=(unsigned char)*text;*h*=UINT64_C(1099511628211);} while (*text++);
}
static void hash_number(uint64_t* h, double value) {
    char text[64];snprintf(text,sizeof(text),"%.17g",value==0?0:value);hash_text(h,text);
}
static void hash_ref(uint64_t* h, const LayoutGeometricReference* r) {
    hash_text(h,r->entity_id);hash_number(h,r->kind);hash_number(h,r->face);
    for (int k=0;k<3;++k) hash_number(h,r->local_offset_meters[k]);
}
static void finish(uint64_t h, char digest[17]) {snprintf(digest,17,"%016llx",(unsigned long long)h);}
static bool input_digest(const Layout* l, const LayoutConstraint* c, char digest[17]) {
    const Object3D* b=entity(l,c->b.entity_id);LayoutResolvedReference a;
    if (!b || b->info.reference || b->info.volume_role || (b->kind!=OBJECT3D_KIND_PLANE && b->kind!=OBJECT3D_KIND_RECT_PRISM) ||
        Layout_ResolveReference(l,&c->a,&a)!=LAYOUT_MEASUREMENT_OK) return false;
    uint64_t h=UINT64_C(14695981039346656037);hash_text(&h,"motion-envelope-v1");
    hash_ref(&h,&c->a);hash_ref(&h,&c->b);hash_number(&h,c->kind);
    hash_number(&h,c->axis.x);hash_number(&h,c->axis.y);hash_number(&h,c->axis.z);
    hash_number(&h,c->travel_min);hash_number(&h,c->travel_max);hash_number(&h,c->travel_home);
    for (int k=0;k<3;++k) {
        hash_number(&h,c->travel_offset[k]);hash_number(&h,c->hinge_reference[k]);
        hash_number(&h,a.point_meters[k]);hash_number(&h,a.direction[k]);
        for (int j=0;j<3;++j) hash_number(&h,c->travel_basis[k][j]);
    }
    hash_number(&h,Layout_WorldScale(l));hash_number(&h,b->kind);
    hash_number(&h,b->kind==OBJECT3D_KIND_PLANE?b->plane.width:b->rectPrism.width);
    hash_number(&h,b->kind==OBJECT3D_KIND_PLANE?b->plane.height:b->rectPrism.height);
    hash_number(&h,b->kind==OBJECT3D_KIND_PLANE?0:b->rectPrism.depth);
    hash_number(&h,b->transform.scale.x);hash_number(&h,b->transform.scale.y);hash_number(&h,b->transform.scale.z);
    finish(h,digest);return true;
}
static void bounds_digest(const Object3D* b, char digest[17]) {
    uint64_t h=UINT64_C(14695981039346656037);
    const PlaneFrame3* f=&b->rectPrism.frame;
    const Vec3 vectors[]={f->origin,f->axisU,f->axisV,f->normal,b->transform.position,b->transform.scale};
    hash_number(&h,b->kind);hash_number(&h,b->rectPrism.width);hash_number(&h,b->rectPrism.height);hash_number(&h,b->rectPrism.depth);
    for (size_t i=0;i<sizeof(vectors)/sizeof(vectors[0]);++i) {
        hash_number(&h,vectors[i].x);hash_number(&h,vectors[i].y);hash_number(&h,vectors[i].z);
    }
    finish(h,digest);
}
bool Layout_MotionEnvelopeCurrent(const Layout* l, const LayoutMotionEnvelope* e) {
    if (!l || !e) return false;
    const LayoutConstraint* c=movement(l,e->rule_id);
    const Object3D* o=Layout_ObjectStore_FindConst(&l->objectStore,e->object_id);char input[17],bounds[17];
    if (!c || !o || !input_digest(l,c,input) || strcmp(e->input_digest,input)) return false;
    bounds_digest(o,bounds);return !strcmp(e->bounds_digest,bounds);
}
static bool digest_valid(const char* s) {
    if (s[16]) return false;
    for (int i=0;i<16;++i) if (!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return false;
    return true;
}
bool Layout_ValidateMotionEnvelopes(const Layout* l, char* message, size_t capacity) {
    const LayoutObjectStore* s=&l->objectStore;const char* reason=NULL;
    if (s->motion_envelope_count>LAYOUT_MAX_MOTION_ENVELOPES) reason="Too many motion envelopes.";
    for (size_t i=0;!reason && i<s->motion_envelope_count && i<LAYOUT_MAX_MOTION_ENVELOPES;++i) {
        const LayoutMotionEnvelope* e=&s->motion_envelopes[i];
        const Object3D* o=Layout_ObjectStore_FindConst(s,e->object_id);
        const LayoutConstraint* c=memchr(e->rule_id,0,64)?movement(l,e->rule_id):NULL;
        if (!c || !o || o->info.volume_role!=LAYOUT_VOLUME_KEEPOUT || strcmp(o->info.volume_owner,c->b.entity_id) ||
            e->samples<2 || e->samples>129 || !isfinite(e->padding_meters) || e->padding_meters<0 ||
            !digest_valid(e->input_digest) || !digest_valid(e->bounds_digest)) reason="Invalid motion envelope provenance; delete the envelope before removing its movement.";
        for (size_t j=0;j<i;++j) if (s->motion_envelopes[j].object_id==e->object_id || !strcmp(s->motion_envelopes[j].rule_id,e->rule_id)) reason="One envelope per saved movement.";
    }
    if (reason && message && capacity) snprintf(message,capacity,"%s",reason);
    return !reason;
}
typedef struct {const char* id;uint32_t samples;} Generate;
static bool generate(Layout* l, void* context) {
    Generate* g=context;const LayoutConstraint* c=movement(l,g->id);char input[17];
    if (!c || g->samples<2 || g->samples>129 || !input_digest(l,c,input)) {
        snprintf(l->geometryMessage,sizeof(l->geometryMessage),"Choose saved Travel/Hinge on a design prism or panel.");return false;
    }
    const LayoutMotionEnvelope* old=Layout_FindRuleEnvelope(&l->objectStore,g->id);
    if (!old && l->objectStore.motion_envelope_count>=LAYOUT_MAX_MOTION_ENVELOPES) {
        snprintf(l->geometryMessage,sizeof(l->geometryMessage),"A scene supports up to 32 motion envelopes.");return false;
    }
    uint32_t id=old?old->object_id:0;
    if (old && Layout_MotionEnvelopeCurrent(l,old) && old->samples==g->samples) return true;
    double min[3]={DBL_MAX,DBL_MAX,DBL_MAX},max[3]={-DBL_MAX,-DBL_MAX,-DBL_MAX},radius=0;
    for (uint32_t i=0;i<g->samples;++i) {
        double t=(double)i/(g->samples-1),value=c->travel_min*(1-t)+c->travel_max*t;Object3D pose;Vec3 corners[8];
        if (!Layout_SampleMotion(l,c->id,value,&pose) || !(pose.kind==OBJECT3D_KIND_PLANE?
            Layout_Object3D_ComputePlaneCorners(&pose,corners):Layout_Object3D_ComputeRectPrismCorners(&pose,corners))) {
            snprintf(l->geometryMessage,sizeof(l->geometryMessage),"Motion sampling failed: inspect locks, bounds and linked constraints.");return false;
        }
        Layout sampled=*l;sampled.objectStore.count=1;sampled.objectStore.items=&pose;LayoutResolvedReference pivot;
        if (Layout_ResolveReference(&sampled,&c->b,&pivot)!=LAYOUT_MEASUREMENT_OK) return false;
        for (int j=0;j<(pose.kind==OBJECT3D_KIND_PLANE?4:8);++j) {
            double p[]={corners[j].x*Layout_WorldScale(l),corners[j].y*Layout_WorldScale(l),corners[j].z*Layout_WorldScale(l)},r2=0;
            for (int k=0;k<3;++k) {min[k]=fmin(min[k],p[k]);max[k]=fmax(max[k],p[k]);double d=p[k]-pivot.point_meters[k];r2+=d*d;}
            radius=fmax(radius,sqrt(r2));
        }
    }
    /* Nearest sample is at most half an interval away. Arc length r*dtheta
     * bounds every point's displacement; it covers the spaces between poses. */
    double pad=c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?radius*(c->travel_max-c->travel_min)*.017453292519943295/(2*(g->samples-1)):0;
    double magnitude=0;for (int k=0;k<3;++k)magnitude=fmax(magnitude,fmax(fabs(min[k]),fabs(max[k])));
    pad+=1e-6+32*FLT_EPSILON*fmax(1,magnitude+radius);
    LayoutVolumeEdit v={.object_id=id,.role=LAYOUT_VOLUME_KEEPOUT};
    const Object3D* source=entity(l,c->b.entity_id);const Object3D* saved=Layout_ObjectStore_FindConst(&l->objectStore,id);
    snprintf(v.name,96,"%s",saved?saved->info.label:"");
    if (!saved) snprintf(v.name,96,"Motion: %.80s",source->info.label[0]?source->info.label:c->b.entity_id);
    snprintf(v.owner,64,"%s",c->b.entity_id);
    for (int k=0;k<3;++k) {
        v.center_meters[k]=(min[k]+max[k])*.5;
        v.size_meters[k]=fmax(max[k]-min[k]+2*pad,Layout_PlanePrimitiveMinSize()*Layout_WorldScale(l));
    }
    /* Rebuild a fresh world-aligned box even if a stale envelope was moved. */
    if (saved) {
        Object3D* box=Layout_ObjectStore_Find(&l->objectStore,id);
        if (box->coreMeta.flags.locked) {snprintf(l->geometryMessage,sizeof(l->geometryMessage),"Unlock the envelope before regenerating it.");return false;}
        box->rectPrism.frame.axisU=(Vec3){1,0,0};box->rectPrism.frame.axisV=(Vec3){0,1,0};box->rectPrism.frame.normal=(Vec3){0,0,1};
        box->transform.rotationDeg=(Vec3){0};box->transform.scale=(Vec3){1,1,1};
    }
    /* The outer candidate already owns atomicity; this nested call needs a
     * temporary inactive flag and no history/publication to the live document. */
    l->geometryEditActive=false;bool ok=Layout_EditVolume(l,&v,false,NULL,NULL);l->geometryEditActive=true;
    if (!ok) return false;
    Object3D* box=Layout_ObjectStore_Find(&l->objectStore,id?id:l->objectStore.items[l->objectStore.count-1].objectId);
    snprintf(box->info.entity_type,64,"MotionEnvelope");
    LayoutMotionEnvelope e={.object_id=box->objectId,.samples=g->samples,.padding_meters=pad};
    snprintf(e.rule_id,64,"%s",g->id);snprintf(e.input_digest,17,"%s",input);bounds_digest(box,e.bounds_digest);
    LayoutMotionEnvelope* record=(LayoutMotionEnvelope*)Layout_FindRuleEnvelope(&l->objectStore,g->id);
    if (record) *record=e;else l->objectStore.motion_envelopes[l->objectStore.motion_envelope_count++]=e;
    return true;
}
bool Layout_GenerateMotionEnvelope(Layout* l, const char* id, uint32_t samples, LayoutGeometryBeforePublish history, void* context) {
    if (!l || !id) return false;
    if (l==&Global_Get()->layout && Global_GetWorkspaceMode()!=LINE_DRAWING_WORKSPACE_MODE_SCENE) {
        snprintf(l->geometryMessage,sizeof(l->geometryMessage),"Motion envelopes require the Scene workspace.");return false;
    }
    Generate g={id,samples};return Layout_RunGeometryEdit(l,0,generate,&g,history,context);
}
