#include "Layout/layout_collection_inspection.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_guide_groups.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

const Object3D* Layout_InspectionObject(const Layout* l,const char* id) {
    if(!l || !id)return NULL;const Object3D* found=NULL;
    for(size_t i=0;i<l->objectStore.count;++i)if(!l->objectStore.items[i].isDeleted&&!strcmp(l->objectStore.items[i].coreMeta.object_id,id)) {
        if(found)return NULL;found=&l->objectStore.items[i];
    }
    return found;
}
bool Layout_InspectionCorners(const Object3D* o,Vec3 out[8],size_t* n) {
    if((o->kind==OBJECT3D_KIND_PLANE||o->kind==OBJECT3D_KIND_RECT_PRISM)&&
       (o->transform.scale.x!=1||o->transform.scale.y!=1||o->transform.scale.z!=1))return false;
    *n=o->kind==OBJECT3D_KIND_PLANE?4:8;
    return o->kind==OBJECT3D_KIND_PLANE?Layout_Object3D_ComputePlaneCorners(o,out):
        o->kind==OBJECT3D_KIND_RECT_PRISM?Layout_Object3D_ComputeRectPrismCorners(o,out):
        o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE&&Layout_Object3D_ComputeMeshInstanceCorners(o,out);
}
LayoutMeasurementStatus Layout_InspectionFrame(const Layout* l,const char* id,PlaneFrame3* out) {
    if(!l || !id || !out || !isfinite(Layout_WorldScale(l)) || Layout_WorldScale(l)<=0)return LAYOUT_MEASUREMENT_INVALID;
    PlaneFrame3 f={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}};
    if(id[0]) {
        const LayoutAssembly* a=NULL;const Object3D* o=NULL;size_t matches=0;
        for(size_t i=0;i<l->objectStore.assembly_count;++i)if(!strcmp(l->objectStore.assemblies[i].id,id)){a=&l->objectStore.assemblies[i];++matches;}
        for(size_t i=0;i<l->objectStore.count;++i)if(!l->objectStore.items[i].isDeleted&&!strcmp(l->objectStore.items[i].coreMeta.object_id,id)){o=&l->objectStore.items[i];++matches;}
        if(matches>1)return LAYOUT_MEASUREMENT_UNRESOLVED;
        if(a)f=a->frame;
        else if(!o)return LAYOUT_MEASUREMENT_UNRESOLVED;
        else if(o->kind==OBJECT3D_KIND_PLANE)f=o->plane.frame;
        else if(o->kind==OBJECT3D_KIND_RECT_PRISM)f=o->rectPrism.frame;
        else if(o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
            f.origin=o->transform.position;
            f.axisU=Vec3_Sub(Layout_Transform3D_ApplyLocalPoint(o->transform,(Vec3){1,0,0}),f.origin);
            f.axisV=Vec3_Sub(Layout_Transform3D_ApplyLocalPoint(o->transform,(Vec3){0,1,0}),f.origin);
            f.normal=Vec3_Sub(Layout_Transform3D_ApplyLocalPoint(o->transform,(Vec3){0,0,1}),f.origin);
        }else return LAYOUT_MEASUREMENT_UNSUPPORTED;
    }
    Vec3* axes[]={&f.axisU,&f.axisV,&f.normal};
    if(!isfinite(f.origin.x)||!isfinite(f.origin.y)||!isfinite(f.origin.z))return LAYOUT_MEASUREMENT_INVALID;
    for(int k=0;k<3;++k) {
        float length=Vec3_Length(*axes[k]);if(!isfinite(length)||length<1e-8f)return LAYOUT_MEASUREMENT_DEGENERATE;
        *axes[k]=Vec3_Scale(*axes[k],1/length);
    }
    if(fabsf(Vec3_Dot(f.axisU,f.axisV))>1e-5f || fabsf(Vec3_Dot(f.axisU,f.normal))>1e-5f ||
       fabsf(Vec3_Dot(f.axisV,f.normal))>1e-5f || Vec3_Dot(Vec3_Cross(f.axisU,f.axisV),f.normal)<.9999f)return LAYOUT_MEASUREMENT_UNSUPPORTED;
    *out=f;return LAYOUT_MEASUREMENT_OK;
}
bool Layout_InspectionMember(const Layout* l,const char* collection,const Object3D* o) {
    return l&&o&&!o->isDeleted&&o->coreMeta.flags.visible&&!Layout_GuideCategory(&l->objectStore,o)&&
        Layout_SceneCollectionContains(&l->objectStore,collection,o->coreMeta.object_id);
}
LayoutCollectionInspection Layout_InspectCollection(const Layout* l,const char* id,const char* frame) {
    LayoutCollectionInspection r={.status=LAYOUT_MEASUREMENT_UNRESOLVED};
    for(int k=0;k<3;++k){r.min_meters[k]=r.world_min_meters[k]=DBL_MAX;r.max_meters[k]=r.world_max_meters[k]=-DBL_MAX;}
    if(!l || !id || !Layout_FindSceneCollection(&l->objectStore,id))goto done;
    r.status=Layout_InspectionFrame(l,frame?frame:"",&r.frame);if(r.status!=LAYOUT_MEASUREMENT_OK)goto done;
    Vec3 axes[]={r.frame.axisU,r.frame.axisV,r.frame.normal};double scale=Layout_WorldScale(l);
    for(size_t i=0;i<l->objectStore.count;++i) {
        const Object3D* o=&l->objectStore.items[i];
        if(!Layout_SceneCollectionContains(&l->objectStore,id,o->coreMeta.object_id))continue;
        if(!Layout_InspectionMember(l,id,o)){++r.excluded;continue;}
        Vec3 p[8];size_t n;
        if(!Layout_InspectionCorners(o,p,&n)){++r.unsupported;continue;}
        ++r.parts;if(o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE)++r.mesh_parts;
        for(size_t j=0;j<n;++j)for(int k=0;k<3;++k) {
            double world=k==0?p[j].x:k==1?p[j].y:p[j].z;
            double local=((double)p[j].x-r.frame.origin.x)*axes[k].x+((double)p[j].y-r.frame.origin.y)*axes[k].y+((double)p[j].z-r.frame.origin.z)*axes[k].z;
            world*=scale;local*=scale;
            if(!isfinite(local)||!isfinite(world)){r.status=LAYOUT_MEASUREMENT_INVALID;goto done;}
            r.min_meters[k]=fmin(r.min_meters[k],local);r.max_meters[k]=fmax(r.max_meters[k],local);
            r.world_min_meters[k]=fmin(r.world_min_meters[k],world);r.world_max_meters[k]=fmax(r.world_max_meters[k],world);
        }
    }
    if(r.unsupported)r.status=LAYOUT_MEASUREMENT_UNSUPPORTED;
    else if(!r.parts)r.status=LAYOUT_MEASUREMENT_UNRESOLVED;
done:
    if(r.status!=LAYOUT_MEASUREMENT_OK)for(int k=0;k<3;++k)r.min_meters[k]=r.max_meters[k]=r.world_min_meters[k]=r.world_max_meters[k]=NAN;
    snprintf(r.message,sizeof(r.message),"%s",r.status==LAYOUT_MEASUREMENT_OK?(r.mesh_parts?"Current pose; conservative STL envelopes included":"Current authored geometry"):Layout_MeasurementStatusLabel(r.status));
    return r;
}
LayoutMeasurementStatus Layout_InspectionDatumPoint(const Layout* l,const LayoutInspectionDatum* d,double out[3]) {
    if(!l||!d||!out||!memchr(d->entity_id,0,64)||d->feature<0||d->feature>3)return LAYOUT_MEASUREMENT_INVALID;
    const Object3D* o=Layout_InspectionObject(l,d->entity_id);if(!o)return LAYOUT_MEASUREMENT_UNRESOLVED;
    PlaneFrame3 frame;LayoutMeasurementStatus status=Layout_InspectionFrame(l,d->entity_id,&frame);if(status!=LAYOUT_MEASUREMENT_OK)return status;
    Vec3 point=frame.origin;
    if(d->feature) {
        Vec3 p[8];size_t n;if(!Layout_InspectionCorners(o,p,&n))return LAYOUT_MEASUREMENT_UNSUPPORTED;
        double lo[]={DBL_MAX,DBL_MAX,DBL_MAX},hi[]={-DBL_MAX,-DBL_MAX,-DBL_MAX};Vec3 axes[]={frame.axisU,frame.axisV,frame.normal};
        for(size_t j=0;j<n;++j)for(int k=0;k<3;++k){double v=Vec3_Dot(Vec3_Sub(p[j],frame.origin),axes[k]);lo[k]=fmin(lo[k],v);hi[k]=fmax(hi[k],v);}
        for(int k=0;k<3;++k){double v=d->feature==1?lo[k]:d->feature==3?hi[k]:(lo[k]+hi[k])*.5;point=Vec3_Add(point,Vec3_Scale(axes[k],(float)v));}
    }
    double scale=Layout_WorldScale(l);out[0]=point.x*scale;out[1]=point.y*scale;out[2]=point.z*scale;
    for(int k=0;k<3;++k)if(!isfinite(out[k]))return LAYOUT_MEASUREMENT_INVALID;
    return LAYOUT_MEASUREMENT_OK;
}
bool Layout_InspectionLocalSize(const Layout* l,const Object3D* o,double out[3]) {
    if(!l||!o||!out)return false;double scale=Layout_WorldScale(l);
    if(o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
        Vec3 span=Vec3_Sub(o->meshInstance.localBoundsMax,o->meshInstance.localBoundsMin);
        out[0]=span.x*o->transform.scale.x*scale;out[1]=span.y*o->transform.scale.y*scale;out[2]=span.z*o->transform.scale.z*scale;
    }else if(o->kind==OBJECT3D_KIND_RECT_PRISM){out[0]=o->rectPrism.width*scale;out[1]=o->rectPrism.height*scale;out[2]=o->rectPrism.depth*scale;}
    else if(o->kind==OBJECT3D_KIND_PLANE){out[0]=o->plane.width*scale;out[1]=o->plane.height*scale;out[2]=0;}
    else return false;
    for(int k=0;k<3;++k)if(!isfinite(out[k])||out[k]<0)return false;
    return true;
}
