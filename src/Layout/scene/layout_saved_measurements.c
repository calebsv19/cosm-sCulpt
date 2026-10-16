#include "Layout/layout_collection_inspection.h"
#include "Layout/layout_scene_collections.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool text(const char* s,size_t n,bool empty) {return s&&memchr(s,0,n)&&(empty||s[0]);}
static bool valid(const LayoutSavedMeasurement* m) {
    return m&&text(m->id,64,false)&&text(m->name,96,false)&&text(m->collection,64,false)&&
        text(m->frame_entity,64,true)&&m->kind>=0&&m->kind<=6&&
        text(m->a.entity_id,64,m->kind<3)&&text(m->b.entity_id,64,m->kind<3)&&
        m->a.feature>=0&&m->a.feature<=3&&m->b.feature>=0&&m->b.feature<=3;
}
LayoutMeasurementResult Layout_EvaluateSavedMeasurement(const Layout* l,const LayoutSavedMeasurement* m) {
    LayoutMeasurementResult r={.status=LAYOUT_MEASUREMENT_INVALID,.value=NAN};
    if(!l||!valid(m))goto done;
    if(!Layout_FindSceneCollection(&l->objectStore,m->collection)){r.status=LAYOUT_MEASUREMENT_UNRESOLVED;goto done;}
    if(m->kind<3) {
        LayoutCollectionInspection bounds=Layout_InspectCollection(l,m->collection,m->frame_entity);r.status=bounds.status;
        if(r.status==LAYOUT_MEASUREMENT_OK)r.value=bounds.max_meters[m->kind]-bounds.min_meters[m->kind];
    }else {
        const Object3D* a=Layout_InspectionObject(l,m->a.entity_id),*b=Layout_InspectionObject(l,m->b.entity_id);
        if(!Layout_InspectionMember(l,m->collection,a)||!Layout_InspectionMember(l,m->collection,b)){r.status=LAYOUT_MEASUREMENT_UNRESOLVED;goto done;}
        double p[3],q[3];PlaneFrame3 frame;r.status=Layout_InspectionFrame(l,m->frame_entity,&frame);
        if(r.status==LAYOUT_MEASUREMENT_OK)r.status=Layout_InspectionDatumPoint(l,&m->a,p);
        if(r.status==LAYOUT_MEASUREMENT_OK)r.status=Layout_InspectionDatumPoint(l,&m->b,q);
        if(r.status!=LAYOUT_MEASUREMENT_OK)goto done;
        if(m->kind==3)r.value=hypot(hypot(q[0]-p[0],q[1]-p[1]),q[2]-p[2]);
        else {Vec3 axis=m->kind==4?frame.axisU:m->kind==5?frame.axisV:frame.normal;r.value=(q[0]-p[0])*axis.x+(q[1]-p[1])*axis.y+(q[2]-p[2])*axis.z;}
    }
    if(!isfinite(r.value)&&r.status==LAYOUT_MEASUREMENT_OK)r.status=LAYOUT_MEASUREMENT_INVALID;
done:
    snprintf(r.message,sizeof(r.message),"%s",r.status==LAYOUT_MEASUREMENT_OK?"Current geometry; envelope datums are not surface clearance":Layout_MeasurementStatusLabel(r.status));return r;
}
bool Layout_ValidateMeasurements(const Layout* l) {
    if(!l||l->objectStore.measurement_count>LAYOUT_MAX_SAVED_MEASUREMENTS)return false;
    for(size_t i=0;i<l->objectStore.measurement_count;++i) {
        if(!valid(&l->objectStore.measurements[i]))return false;
        for(size_t j=0;j<i;++j)if(!strcmp(l->objectStore.measurements[i].id,l->objectStore.measurements[j].id))return false;
    }
    return true; /* missing references stay inert and never prevent geometry editing */
}
typedef struct {const LayoutSavedMeasurement* m;const char* remove;} Edit;
static bool apply(Layout* l,void* context) {
    Edit* e=context;LayoutObjectStore* s=&l->objectStore;const char* id=e->remove?e->remove:e->m->id;
    size_t at=0;while(at<s->measurement_count&&strcmp(s->measurements[at].id,id))++at;
    if(e->remove) {
        if(at==s->measurement_count)return false;
        memmove(s->measurements+at,s->measurements+at+1,(--s->measurement_count-at)*sizeof(*s->measurements));return true;
    }
    if(at==LAYOUT_MAX_SAVED_MEASUREMENTS||!valid(e->m)||Layout_EvaluateSavedMeasurement(l,e->m).status!=LAYOUT_MEASUREMENT_OK)return false;
    s->measurements[at]=*e->m;if(at==s->measurement_count)++s->measurement_count;return Layout_ValidateMeasurements(l);
}
bool Layout_SaveMeasurement(Layout* l,const LayoutSavedMeasurement* m,const char* remove,LayoutGeometryBeforePublish history,void* context) {
    if(!l||l->geometryEditActive||(!m&&!remove)||(m&&remove))return false;Edit e={m,remove};
    /* Metadata is not part of the geometry transaction's publication set. Borrow
     * immutable geometry, stage only records, then reserve history before publish. */
    Layout* candidate=malloc(sizeof(*candidate));if(!candidate)return false;*candidate=*l;
    bool ok=apply(candidate,&e)&&Layout_ValidateMeasurements(candidate);
    bool changed=ok&&(l->objectStore.measurement_count!=candidate->objectStore.measurement_count||
        memcmp(l->objectStore.measurements,candidate->objectStore.measurements,sizeof(l->objectStore.measurements)));
    if(changed&&history&&!history(l,context))ok=false;
    if(ok&&changed){l->objectStore.measurement_count=candidate->objectStore.measurement_count;memcpy(l->objectStore.measurements,candidate->objectStore.measurements,sizeof(l->objectStore.measurements));}
    free(candidate);return ok;
}
bool Layout_HasInspectionRecords(const Layout* l) {
    if(l->objectStore.measurement_count)return true;
    for(size_t i=0;i<l->objectStore.scene_collection_count;++i)if(l->objectStore.scene_collections[i].inspection_frame[0])return true;
    return false;
}
