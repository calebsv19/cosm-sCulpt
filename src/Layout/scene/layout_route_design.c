#include "Layout/layout_route_design.h"
#include "Layout/layout_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const Object3D* Layout_FindCorridor(const LayoutObjectStore* s,const char* id) {
    for (size_t i=0;i<s->count;++i) {
        const Object3D* o=&s->items[i];
        if (!o->isDeleted && o->kind==OBJECT3D_KIND_RECT_PRISM && !strcmp(o->coreMeta.object_id,id) &&
            !strcmp(o->info.entity_type,"RoutingCorridor") && !o->info.reference && !o->info.volume_role) return o;
    }
    return NULL;
}
bool Layout_MarkCorridor(Layout* l,uint32_t id,LayoutGeometryBeforePublish h,void* context) {
    const Object3D* o=Layout_ObjectStore_FindConst(&l->objectStore,id);
    if (!o || o->kind!=OBJECT3D_KIND_RECT_PRISM || o->info.reference || o->info.volume_role ||
        Layout_FindMotionEnvelope(&l->objectStore,id)) return false;
    LayoutEntityInfo info=o->info;snprintf(info.entity_type,64,"RoutingCorridor");
    if (!info.label[0]) snprintf(info.label,96,"Routing region #%u",id);
    return Layout_SetEntityInfo(l,o->coreMeta.object_id,&info,h,context);
}
bool Layout_RouteDesignValid(const Layout* l,const LayoutPhysicalRoute* r) {
    if (r->corridor_count>LAYOUT_MAX_ROUTE_CORRIDORS) return false;
    for (size_t i=0;i<r->corridor_count;++i) {
        if (!memchr(r->corridor_ids[i],0,64) || !Layout_FindCorridor(&l->objectStore,r->corridor_ids[i])) return false;
        for (size_t j=0;j<i;++j) if (!strcmp(r->corridor_ids[i],r->corridor_ids[j])) return false;
    }
    const LayoutRouteElectrical* e=&r->electrical;
    if (!memchr(e->circuit,0,64) || !memchr(e->power_domain,0,64) || e->voltage_class<0 || e->voltage_class>3) return false;
    const double values[]={r->radius_meters,r->clearance_meters,r->maximum_length_meters,e->nominal_volts,e->design_amps,
        e->area_mm2,e->return_meters,e->allowance_meters,e->fuse_amps};
    for (size_t i=0;i<sizeof(values)/sizeof(values[0]);++i) if (!isfinite(values[i]) || values[i]<0 || values[i]>1e9) return false;
    return true;
}
double Layout_AWGArea(int awg) {
    if (awg<-3 || awg>40) return NAN;
    double diameter=.127*pow(92,(36.0-awg)/39.0);
    return 3.14159265358979323846*diameter*diameter/4;
}
bool Layout_RouteVoltageDrop(const LayoutPhysicalRoute* r,double* volts,double* percent) {
    const LayoutRouteElectrical* e=&r->electrical;
    if (strcmp(r->info.entity_type,"Cable") || e->voltage_class!=1 || !e->copper ||
        !(e->nominal_volts>0) || !(e->design_amps>0) || !(e->area_mm2>0) || !(e->return_meters>0)) return false;
    double length=Layout_RouteLength(r)+e->allowance_meters+e->return_meters;
    double drop=e->design_amps*.017241*length/e->area_mm2;
    if (!isfinite(drop)) return false;
    *volts=drop;*percent=100*drop/e->nominal_volts;return true;
}
static bool numeric(const cJSON* object,const char* key,double* out) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(object,key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<0 || v->valuedouble>1e9) return false;
    *out=v->valuedouble;return true;
}
static bool string(const cJSON* object,const char* key,char* out,size_t capacity) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(object,key);
    if (!cJSON_IsString(v) || strlen(v->valuestring)>=capacity) return false;
    snprintf(out,capacity,"%s",v->valuestring);return true;
}
bool Layout_RouteDesignReadJson(LayoutPhysicalRoute* r,const cJSON* json) {
    const cJSON* d=cJSON_GetObjectItemCaseSensitive(json,"design");
    if (!d) return true; /* Schema 20 routes migrate with unknown design inputs. */
    const cJSON* ids=cJSON_GetObjectItemCaseSensitive(d,"corridors");
    const cJSON* e=cJSON_GetObjectItemCaseSensitive(d,"electrical");
    const cJSON* copper=cJSON_GetObjectItemCaseSensitive(e,"copper");
    double kind;
    if (!cJSON_IsObject(d) || !cJSON_IsArray(ids) || cJSON_GetArraySize(ids)>LAYOUT_MAX_ROUTE_CORRIDORS ||
        !numeric(d,"radius_m",&r->radius_meters) || !numeric(d,"clearance_m",&r->clearance_meters) ||
        !numeric(d,"maximum_length_m",&r->maximum_length_meters) || !string(e,"circuit",r->electrical.circuit,64) ||
        !string(e,"power_domain",r->electrical.power_domain,64) || !numeric(e,"voltage_class",&kind) || kind>3 || floor(kind)!=kind ||
        !numeric(e,"nominal_volts",&r->electrical.nominal_volts) || !numeric(e,"design_amps",&r->electrical.design_amps) ||
        !numeric(e,"area_mm2",&r->electrical.area_mm2) || !numeric(e,"return_m",&r->electrical.return_meters) ||
        !numeric(e,"allowance_m",&r->electrical.allowance_meters) || !numeric(e,"fuse_amps",&r->electrical.fuse_amps) || !cJSON_IsBool(copper)) return false;
    r->electrical.voltage_class=(int)kind;r->electrical.copper=cJSON_IsTrue(copper);
    r->corridor_count=(size_t)cJSON_GetArraySize(ids);
    for (size_t i=0;i<r->corridor_count;++i) {
        const cJSON* id=cJSON_GetArrayItem(ids,(int)i);
        if (!cJSON_IsString(id) || strlen(id->valuestring)>=64) return false;
        snprintf(r->corridor_ids[i],64,"%s",id->valuestring);
    }
    return true;
}
bool Layout_RouteDesignWriteJson(const LayoutPhysicalRoute* r,cJSON* json) {
    cJSON* d=cJSON_AddObjectToObject(json,"design");if (!d) return false;
    cJSON* ids=cJSON_AddArrayToObject(d,"corridors");if (!ids) return false;
    for (size_t i=0;i<r->corridor_count;++i) if (!cJSON_AddItemToArray(ids,cJSON_CreateString(r->corridor_ids[i]))) return false;
    const LayoutRouteElectrical* x=&r->electrical;cJSON* e=cJSON_AddObjectToObject(d,"electrical");
    return e && cJSON_AddNumberToObject(d,"radius_m",r->radius_meters) && cJSON_AddNumberToObject(d,"clearance_m",r->clearance_meters) &&
        cJSON_AddNumberToObject(d,"maximum_length_m",r->maximum_length_meters) &&
        cJSON_AddStringToObject(e,"circuit",x->circuit) && cJSON_AddStringToObject(e,"power_domain",x->power_domain) &&
        cJSON_AddNumberToObject(e,"voltage_class",x->voltage_class) && cJSON_AddNumberToObject(e,"nominal_volts",x->nominal_volts) &&
        cJSON_AddNumberToObject(e,"design_amps",x->design_amps) && cJSON_AddNumberToObject(e,"area_mm2",x->area_mm2) &&
        cJSON_AddNumberToObject(e,"return_m",x->return_meters) && cJSON_AddNumberToObject(e,"allowance_m",x->allowance_meters) &&
        cJSON_AddNumberToObject(e,"fuse_amps",x->fuse_amps) && cJSON_AddBoolToObject(e,"copper",x->copper);
}
typedef struct {LayoutRouteCheck* out;size_t capacity,count;const LayoutPhysicalRoute* route;} Checks;
static void emit(Checks* c,size_t segment,const char* target,const char* code,LayoutSpatialSeverity severity,bool approximate,const char* message) {
    if (c->out && c->count<c->capacity) {
        LayoutRouteCheck* r=&c->out[c->count];memset(r,0,sizeof(*r));r->segment=segment;r->severity=severity;r->approximate=approximate;
        snprintf(r->route_id,64,"%s",c->route->id);snprintf(r->target_id,64,"%s",target);snprintf(r->code,32,"%s",code);snprintf(r->message,160,"%s",message);
    }
    ++c->count;
}
static bool covered(const Layout* l,const LayoutPhysicalRoute* r,const double a[3],const double b[3]) {
    double intervals[LAYOUT_MAX_ROUTE_CORRIDORS][2];size_t n=0;
    for (size_t i=0;i<r->corridor_count;++i) {
        const Object3D* o=Layout_FindCorridor(&l->objectStore,r->corridor_ids[i]);double lo,hi;bool approx;
        if (o && Layout_SpatialSegmentInterval(l,o,a,b,-r->radius_meters-r->clearance_meters,&lo,&hi,&approx) && lo<=hi+1e-9) {
            intervals[n][0]=lo;intervals[n++][1]=hi;
        }
    }
    double reach=0;
    for (size_t pass=0;pass<n;++pass) for (size_t i=0;i<n;++i)
        if (intervals[i][0]<=reach+1e-9 && intervals[i][1]>reach) reach=intervals[i][1];
    return reach>=1-1e-9;
}
size_t Layout_CheckRoute(const Layout* l,const LayoutPhysicalRoute* r,LayoutRouteCheck* out,size_t capacity) {
    if (!l || !r || r->point_count<2 || r->point_count>LAYOUT_MAX_ROUTE_POINTS) return 0;
    Checks c={out,capacity,0,r};
    if (!Layout_RouteEndpointsCurrent(l,r)) emit(&c,0,"","stale_endpoints",LAYOUT_SPATIAL_WARNING,false,"Refresh captured endpoint positions; checks use the saved polyline.");
    if (r->maximum_length_meters>0 && Layout_RouteLength(r)>r->maximum_length_meters)
        emit(&c,0,"","maximum_length",LAYOUT_SPATIAL_ERROR,false,"Route exceeds its authored maximum centerline length.");
    if (!r->corridor_count) emit(&c,0,"","corridor_unassigned",LAYOUT_SPATIAL_WARNING,false,"No corridor coverage requirement is assigned.");
    for (size_t p=1;p<r->point_count;++p) {
        const double* a=r->points_meters[p-1];const double* b=r->points_meters[p];
        if (r->corridor_count && !covered(l,r,a,b)) emit(&c,p,"","outside_corridors",LAYOUT_SPATIAL_ERROR,false,"Segment is not fully covered by the union of assigned boxes, including radius/clearance.");
        for (size_t i=0;i<l->objectStore.count;++i) {
            const Object3D* o=&l->objectStore.items[i];const char* id=o->coreMeta.object_id;
            if (o->isDeleted || o->info.reference || !strcmp(o->info.entity_type,"RoutingCorridor")) continue;
            if (!o->info.volume_role && (!strcmp(id,r->source.entity_id) || !strcmp(id,r->destination.entity_id))) continue;
            const LayoutMotionEnvelope* envelope=Layout_FindMotionEnvelope(&l->objectStore,o->objectId);
            if (envelope && !Layout_MotionEnvelopeCurrent(l,envelope)) {
                if (p==1) emit(&c,0,id,"stale_motion",LAYOUT_SPATIAL_WARNING,true,"Motion envelope is stale; regenerate before relying on route clearance.");
                continue;
            }
            double lo,hi;bool approximate;
            if (!Layout_SpatialSegmentInterval(l,o,a,b,r->radius_meters+r->clearance_meters,&lo,&hi,&approximate)) {
                if (p==1) emit(&c,0,id,"unsupported_geometry",LAYOUT_SPATIAL_WARNING,true,"Obstacle geometry could not be checked.");
                continue;
            }
            if (lo>hi+1e-9) continue;
            double exact_lo,exact_hi;bool proxy;
            bool exact=Layout_SpatialSegmentInterval(l,o,a,b,0,&exact_lo,&exact_hi,&proxy) && exact_lo<=exact_hi+1e-9 && !proxy && !envelope;
            emit(&c,p,id,envelope?"motion_overlap":o->info.volume_role?"reserved_overlap":"obstacle_overlap",
                exact?LAYOUT_SPATIAL_ERROR:LAYOUT_SPATIAL_WARNING,!exact,
                exact?"Centerline intersects this primitive (contact counts).":"Possible radius/clearance or conservative envelope overlap; inspect this segment.");
        }
    }
    if (!strcmp(r->info.entity_type,"Cable") && r->electrical.voltage_class==1) {
        double v,pct;
        if (!Layout_RouteVoltageDrop(r,&v,&pct)) emit(&c,0,"","electrical_incomplete",LAYOUT_SPATIAL_WARNING,false,"DC drop needs voltage, load current, copper area and an explicit return length.");
        else if (v>=r->electrical.nominal_volts) emit(&c,0,"","voltage_drop",LAYOUT_SPATIAL_ERROR,false,"Estimated cable drop equals or exceeds nominal supply voltage.");
        if (!(r->electrical.fuse_amps>0)) emit(&c,0,"","protection_unspecified",LAYOUT_SPATIAL_WARNING,false,"Protection rating is unspecified. Ampacity and fuse coordination are not assessed.");
    }
    if (!c.count) emit(&c,0,"","clear",LAYOUT_SPATIAL_PASS,false,"No issues in implemented centerline/corridor checks; electrical installation is not certified.");
    return c.count;
}
cJSON* Layout_RouteChecksJson(const Layout* l,const LayoutPhysicalRoute* r) {
    size_t capacity=4096;LayoutRouteCheck* results=calloc(capacity,sizeof(*results));if (!results) return NULL;
    size_t count=Layout_CheckRoute(l,r,results,capacity);cJSON* report=cJSON_CreateObject();
    cJSON* array=report?cJSON_AddArrayToObject(report,"results"):NULL;
    if (!array || !cJSON_AddNumberToObject(report,"total",(double)count) || !cJSON_AddBoolToObject(report,"truncated",count>capacity)) {free(results);cJSON_Delete(report);return NULL;}
    for (size_t i=0;i<count && i<capacity;++i) {
        const LayoutRouteCheck* x=&results[i];cJSON* v=cJSON_CreateObject();
        if (!v || !cJSON_AddItemToArray(array,v) || !cJSON_AddStringToObject(v,"code",x->code) || !cJSON_AddStringToObject(v,"target",x->target_id) ||
            !cJSON_AddStringToObject(v,"severity",(const char*[]){"pass","error","warning"}[x->severity]) ||
            !cJSON_AddNumberToObject(v,"segment",(double)x->segment) || !cJSON_AddBoolToObject(v,"approximate",x->approximate) ||
            !cJSON_AddStringToObject(v,"message",x->message)) {free(results);cJSON_Delete(report);return NULL;}
    }
    free(results);return report;
}
