#include "Layout/layout_constraints.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool number(const cJSON* root, const char* name, double* value) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(root,name);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble)) return false;
    *value=v->valuedouble; return true;
}
static bool text(const cJSON* root, const char* name, char* out, size_t size) {
    const cJSON* v=cJSON_GetObjectItemCaseSensitive(root,name);
    if (!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring)>=size) return false;
    snprintf(out,size,"%s",v->valuestring); return true;
}
static cJSON* reference_json(const LayoutGeometricReference* ref) {
    cJSON* o=cJSON_CreateObject();
    if (!o) return NULL;
    if (!cJSON_AddStringToObject(o,"entityId",ref->entity_id) ||
        !cJSON_AddNumberToObject(o,"feature",ref->kind) || !cJSON_AddNumberToObject(o,"face",ref->face) ||
        !cJSON_AddNumberToObject(o,"offsetU_m",ref->local_offset_meters[0]) ||
        !cJSON_AddNumberToObject(o,"offsetV_m",ref->local_offset_meters[1]) ||
        !cJSON_AddNumberToObject(o,"offsetN_m",ref->local_offset_meters[2])) {
        cJSON_Delete(o); return NULL;
    }
    return o;
}
static bool read_reference(const cJSON* o, LayoutGeometricReference* ref, bool offsets_required) {
    double feature,face;
    if (!cJSON_IsObject(o) || !text(o,"entityId",ref->entity_id,sizeof(ref->entity_id)) ||
        !number(o,"feature",&feature) || floor(feature)!=feature || feature<0 || feature>LAYOUT_REFERENCE_FACE ||
        !number(o,"face",&face) || floor(face)!=face || face<0 || face>OBJECT3D_FACE_RECT_PRISM_POS_U) return false;
    const char* fields[]={"offsetU_m","offsetV_m","offsetN_m"};
    bool has_offset=false;
    for (int i=0; i<3; ++i) if (cJSON_GetObjectItemCaseSensitive(o,fields[i])) has_offset=true;
    if (offsets_required || has_offset)
        for (int i=0; i<3; ++i) if (!number(o,fields[i],&ref->local_offset_meters[i])) return false;
    ref->kind=(LayoutReferenceKind)feature; ref->face=(Object3DFaceKind)face; return true;
}
bool Layout_ConstraintsWriteJson(const Layout* layout, cJSON* root) {
    if (!Layout_ValidateConstraints(layout,NULL,0)) return false;
    cJSON* array=cJSON_AddArrayToObject(root,"geometricConstraints");
    if (!array || !cJSON_AddNumberToObject(root,"nextConstraintId",layout->objectStore.nextConstraintId ? layout->objectStore.nextConstraintId : 1)) return false;
    for (size_t i=0; i<layout->objectStore.constraintCount; ++i) {
        const LayoutConstraint* c=&layout->objectStore.constraints[i];
        cJSON* o=cJSON_CreateObject();
        if (!o) return false;
        if (!cJSON_AddItemToArray(array,o)) { cJSON_Delete(o); return false; }
        cJSON* a=reference_json(&c->a); cJSON* b=reference_json(&c->b);
        if (!a || !b) { cJSON_Delete(a); cJSON_Delete(b); return false; }
        if (!cJSON_AddItemToObject(o,"a",a)) { cJSON_Delete(a); cJSON_Delete(b); return false; }
        if (!cJSON_AddItemToObject(o,"b",b)) { cJSON_Delete(b); return false; }
        if (!cJSON_AddStringToObject(o,"id",c->id) || !cJSON_AddNumberToObject(o,"kind",c->kind) ||
            !cJSON_AddNumberToObject(o,"target",c->target) || !cJSON_AddNumberToObject(o,"axisX",c->axis.x) ||
            !cJSON_AddNumberToObject(o,"axisY",c->axis.y) || !cJSON_AddNumberToObject(o,"axisZ",c->axis.z)) return false;
    }
    return true;
}
bool Layout_ConstraintsReadJson(Layout* layout, const cJSON* root, bool required) {
    const cJSON* array=cJSON_GetObjectItemCaseSensitive(root,"geometricConstraints");
    if (!array && !required) return true;
    double next;
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array)>LAYOUT_MAX_CONSTRAINTS ||
        !number(root,"nextConstraintId",&next) || next<1 || next>UINT32_MAX || floor(next)!=next) return false;
    layout->objectStore.nextConstraintId=(uint32_t)next;
    const cJSON* version=cJSON_GetObjectItemCaseSensitive(root,"schemaVersion");
    bool offsets_required=cJSON_IsNumber(version) && version->valuedouble>=12;
    const cJSON* o;
    cJSON_ArrayForEach(o,array) {
        LayoutConstraint c={0}; double kind,x,y,z;
        if (!cJSON_IsObject(o) || !text(o,"id",c.id,sizeof(c.id)) ||
            !read_reference(cJSON_GetObjectItemCaseSensitive(o,"a"),&c.a,offsets_required) ||
            !read_reference(cJSON_GetObjectItemCaseSensitive(o,"b"),&c.b,offsets_required) ||
            !number(o,"kind",&kind) || floor(kind)!=kind || kind<0 || kind>LAYOUT_CONSTRAINT_PLANAR_MATE ||
            !number(o,"target",&c.target) || !number(o,"axisX",&x) || !number(o,"axisY",&y) || !number(o,"axisZ",&z)) return false;
        c.kind=(LayoutConstraintKind)kind; c.axis=(Vec3){(float)x,(float)y,(float)z};
        layout->objectStore.constraints[layout->objectStore.constraintCount++]=c;
    }
    /* Loading never silently solves a malformed/stale authored document. */
    return Layout_ValidateConstraints(layout,NULL,0);
}
