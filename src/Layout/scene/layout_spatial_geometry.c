#include "Layout/layout_spatial.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {double x,y,z;} D3;
typedef struct {D3 center,axis[3],corner[8];double half[3];bool approximate;} Box;
static D3 add(D3 a,D3 b){return (D3){a.x+b.x,a.y+b.y,a.z+b.z};}
static D3 sub(D3 a,D3 b){return (D3){a.x-b.x,a.y-b.y,a.z-b.z};}
static D3 mul(D3 a,double s){return (D3){a.x*s,a.y*s,a.z*s};}
static double dot(D3 a,D3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static D3 cross(D3 a,D3 b){return (D3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static D3 physical(Vec3 v,double scale){return (D3){v.x*scale,v.y*scale,v.z*scale};}
static bool box(const Layout* l,const Object3D* o,Box* b) {
    Vec3 c[8];double scale=Layout_WorldScale(l);if(!isfinite(scale)||scale<=0)return false;
    memset(b,0,sizeof(*b));
    if (o->kind==OBJECT3D_KIND_PLANE) {
        if(!Layout_Object3D_ComputePlaneCorners(o,c))return false;
        for(int i=0;i<4;++i)c[i+4]=c[i];
    } else if(o->kind==OBJECT3D_KIND_RECT_PRISM) {
        if(!Layout_Object3D_ComputeRectPrismCorners(o,c))return false;
    } else if(o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
        Vec3 min,max;if(!Layout_Object3D_ComputeWorldAABB(o,&min,&max))return false;
        c[0]=(Vec3){min.x,min.y,min.z};c[1]=(Vec3){max.x,min.y,min.z};c[2]=(Vec3){max.x,max.y,min.z};c[3]=(Vec3){min.x,max.y,min.z};
        c[4]=(Vec3){min.x,min.y,max.z};c[5]=(Vec3){max.x,min.y,max.z};c[6]=(Vec3){max.x,max.y,max.z};c[7]=(Vec3){min.x,max.y,max.z};b->approximate=true;
    } else return false;
    for(int i=0;i<8;++i){b->corner[i]=physical(c[i],scale);b->center=add(b->center,mul(b->corner[i],.125));}
    const int ends[]={1,3,4};const D3 fallback[]={{1,0,0},{0,1,0},{0,0,1}};
    for(int k=0;k<3;++k) {
        D3 edge=sub(b->corner[ends[k]],b->corner[0]);double length=sqrt(dot(edge,edge));
        if(!isfinite(length))return false;
        b->half[k]=length*.5;b->axis[k]=length>1e-12?mul(edge,1/length):fallback[k];
    }
    if(o->kind==OBJECT3D_KIND_PLANE)b->axis[2]=cross(b->axis[0],b->axis[1]);
    return true;
}
static bool separates(const Box* a,const Box* b,D3 axis) {
    double length=sqrt(dot(axis,axis));if(length<1e-10)return false;axis=mul(axis,1/length);
    double radius=0;for(int k=0;k<3;++k)radius+=a->half[k]*fabs(dot(a->axis[k],axis))+b->half[k]*fabs(dot(b->axis[k],axis));
    return fabs(dot(sub(b->center,a->center),axis))>radius+1e-6;
}
static bool intersects(const Box* a,const Box* b) {
    for(int i=0;i<3;++i) {
        if(separates(a,b,a->axis[i])||separates(a,b,b->axis[i]))return false;
        for(int j=0;j<3;++j)if(separates(a,b,cross(a->axis[i],b->axis[j])))return false;
    }
    return true;
}
static double point_distance(D3 p,const Box* b) {
    D3 delta=sub(p,b->center);double sum=0;
    for(int k=0;k<3;++k){double d=fmax(0,fabs(dot(delta,b->axis[k]))-b->half[k]);sum+=d*d;}
    return sqrt(sum);
}
/* Closest finite-segment pair, including degenerate panel edges. */
static double segment_distance(D3 p,D3 q,D3 r,D3 t) {
    D3 u=sub(q,p),v=sub(t,r),w=sub(p,r);double a=dot(u,u),b=dot(u,v),c=dot(v,v),d=dot(u,w),e=dot(v,w),s=0,h=0;
    if(a<1e-24 && c<1e-24)return sqrt(dot(w,w));
    if(a<1e-24)h=fmax(0,fmin(1,e/c));
    else if(c<1e-24)s=fmax(0,fmin(1,-d/a));
    else {
        double denominator=a*c-b*b;
        if(denominator>1e-24)s=fmax(0,fmin(1,(b*e-c*d)/denominator));
        h=(b*s+e)/c;
        if(h<0){h=0;s=fmax(0,fmin(1,-d/a));}
        else if(h>1){h=1;s=fmax(0,fmin(1,(b-d)/a));}
    }
    D3 delta=sub(add(w,mul(u,s)),mul(v,h));return sqrt(fmax(0,dot(delta,delta)));
}
bool Layout_SpatialDistance(const Layout* l,const Object3D* a,const Object3D* b,double* meters,bool* overlap,bool* approximate) {
    if(!l||!a||!b||!meters||!overlap||!approximate)return false;
    Box x,y;if(!box(l,a,&x)||!box(l,b,&y))return false;
    *approximate=x.approximate||y.approximate;*overlap=intersects(&x,&y);*meters=0;
    if(*overlap)return true;
    double best=DBL_MAX;
    for(int i=0;i<8;++i){best=fmin(best,point_distance(x.corner[i],&y));best=fmin(best,point_distance(y.corner[i],&x));}
    static const int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for(int i=0;i<12;++i)for(int j=0;j<12;++j)
        best=fmin(best,segment_distance(x.corner[edges[i][0]],x.corner[edges[i][1]],y.corner[edges[j][0]],y.corner[edges[j][1]]));
    *meters=best;return isfinite(best);
}
static bool member(const LayoutObjectStore* s,const Object3D* o,const char* id) {
    return !strcmp(o->coreMeta.object_id,id)||Layout_IsDescendant(s,o->info.parent_id,id);
}
static LayoutSpatialResult check_pair(const Layout* l,const Object3D* a,const Object3D* b,const char* rule,double required,bool clearance) {
    LayoutSpatialResult r={.required_meters=required};snprintf(r.rule_id,64,"%s",rule);snprintf(r.source,64,"%s",a->coreMeta.object_id);snprintf(r.target,64,"%s",b->coreMeta.object_id);
    r.measurable=Layout_SpatialDistance(l,a,b,&r.distance_meters,&r.overlap,&r.approximate);
    bool failed=clearance ? r.distance_meters+1e-6<required : r.overlap;
    r.severity=!r.measurable || r.approximate ? LAYOUT_SPATIAL_WARNING : failed ? LAYOUT_SPATIAL_ERROR : LAYOUT_SPATIAL_PASS;
    snprintf(r.message,sizeof(r.message),"%s",!r.measurable?"Geometry unavailable; check unresolved.":r.approximate ?
        (failed?"Bounds indicate a possible conflict; inspect mesh geometry.":"Bounds satisfy this check; mesh geometry not tested."):
        failed?(clearance?"Minimum clearance not met.":"Objects intersect or touch."):"Check passed.");
    return r;
}
static void emit(LayoutSpatialResult r,LayoutSpatialResult* output,size_t capacity,size_t* count) {
    if(output && *count<capacity)output[*count]=r;
    ++*count;
}
size_t Layout_CheckSpatial(const Layout* l,LayoutSpatialResult* output,size_t capacity) {
    if(!l)return 0;
    size_t count=0;const LayoutObjectStore* s=&l->objectStore;
    if(!Layout_ValidateEngineering(l,NULL,0)) {
        LayoutSpatialResult r={.severity=LAYOUT_SPATIAL_WARNING};snprintf(r.message,sizeof(r.message),"Invalid engineering records; checks unresolved.");emit(r,output,capacity,&count);return count;
    }
    for(size_t i=0;i<s->count;++i) {
        const Object3D* v=&s->items[i];if(v->isDeleted||!v->info.volume_role)continue;
        for(size_t j=0;j<s->count;++j) {
            const Object3D* o=&s->items[j];
            if(o->isDeleted||o==v||o->info.volume_role||o->info.reference || (v->info.volume_owner[0] && member(s,o,v->info.volume_owner)))continue;
            LayoutSpatialResult result=check_pair(l,v,o,v->coreMeta.object_id,0,false);
            /* Automatic checks report obstructions/unresolved geometry, not thousands of clear pairs. */
            if(result.overlap||!result.measurable)emit(result,output,capacity,&count);
        }
    }
    for(size_t k=0;k<s->spatial_rule_count;++k) {
        const LayoutSpatialRule* rule=&s->spatial_rules[k];size_t pairs=0;
        for(size_t i=0;i<s->count;++i) {
            const Object3D* a=&s->items[i];if(a->isDeleted||!member(s,a,rule->source))continue;
            for(size_t j=0;j<s->count;++j) {
                const Object3D* b=&s->items[j];if(b->isDeleted||a==b||!member(s,b,rule->target))continue;
                emit(check_pair(l,a,b,rule->id,rule->clearance_meters,rule->kind==LAYOUT_SPATIAL_CLEARANCE),output,capacity,&count);++pairs;
            }
        }
        if(!pairs) {
            LayoutSpatialResult r={.severity=LAYOUT_SPATIAL_WARNING};snprintf(r.rule_id,64,"%s",rule->id);snprintf(r.source,64,"%s",rule->source);snprintf(r.target,64,"%s",rule->target);
            snprintf(r.message,sizeof(r.message),"No distinct geometry pairs; check unresolved.");emit(r,output,capacity,&count);
        }
    }
    return count;
}

cJSON* Layout_SpatialReportJson(const Layout* l) {
    if(!l)return NULL;
    size_t total=Layout_CheckSpatial(l,NULL,0),count=total>4096?4096:total;
    LayoutSpatialResult* results=count?calloc(count,sizeof(*results)):NULL;
    if(count && !results)return NULL;
    (void)Layout_CheckSpatial(l,results,count);
    cJSON* report=cJSON_CreateObject();
    if(!report){free(results);return NULL;}
    cJSON* array=cJSON_AddArrayToObject(report,"results");
    bool ok=array && cJSON_AddStringToObject(report,"schema","line_drawing_spatial_report_v1") && cJSON_AddStringToObject(report,"unit","m") &&
        cJSON_AddNumberToObject(report,"total",(double)total) && cJSON_AddBoolToObject(report,"truncated",total>count);
    for(size_t i=0;ok && i<count;++i) {
        const LayoutSpatialResult* r=&results[i];cJSON* item=cJSON_CreateObject();
        if(!item){ok=false;break;}
        cJSON_AddItemToArray(array,item);
        ok=cJSON_AddStringToObject(item,"ruleId",r->rule_id) && cJSON_AddStringToObject(item,"source",r->source) && cJSON_AddStringToObject(item,"target",r->target) &&
            cJSON_AddStringToObject(item,"severity",(const char*[]){"pass","error","warning"}[r->severity]) && cJSON_AddBoolToObject(item,"approximate",r->approximate) &&
            cJSON_AddBoolToObject(item,"measurable",r->measurable) && cJSON_AddBoolToObject(item,"intersects",r->overlap) &&
            cJSON_AddNumberToObject(item,"distanceMeters",r->distance_meters) && cJSON_AddNumberToObject(item,"requiredMeters",r->required_meters) && cJSON_AddStringToObject(item,"message",r->message);
    }
    free(results);if(!ok){cJSON_Delete(report);return NULL;}return report;
}
