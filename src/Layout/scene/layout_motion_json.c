#include "Layout/layout_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool add_vector(cJSON* o, const char* key, const double* values, int count) {
    cJSON* a=cJSON_CreateDoubleArray(values,count);if (!a) return false;
    if (cJSON_AddItemToObject(o,key,a)) return true;
    cJSON_Delete(a);return false;
}
bool Layout_MotionWriteJson(const Layout* l, cJSON* engineering) {
    cJSON* array=cJSON_AddArrayToObject(engineering,"motionEnvelopes");if (!array) return false;
    for (size_t i=0;i<l->objectStore.motion_envelope_count;++i) {
        const LayoutMotionEnvelope* e=&l->objectStore.motion_envelopes[i];cJSON* o=cJSON_CreateObject();
        if (!o) return false;cJSON_AddItemToArray(array,o);
        if (!cJSON_AddNumberToObject(o,"objectId",e->object_id) || !cJSON_AddStringToObject(o,"ruleId",e->rule_id) ||
            !cJSON_AddNumberToObject(o,"samples",e->samples) || !cJSON_AddNumberToObject(o,"paddingMeters",e->padding_meters) ||
            !cJSON_AddStringToObject(o,"inputDigest",e->input_digest) || !cJSON_AddStringToObject(o,"boundsDigest",e->bounds_digest) ||
            !cJSON_AddStringToObject(o,"assemblyId",e->assembly_id)) return false;
        cJSON* members=cJSON_AddArrayToObject(o,"members");if (!members) return false;
        for (size_t j=0;j<e->member_count;++j) {
            const LayoutMotionMember* m=&e->members[j];cJSON* n=cJSON_CreateObject();if (!n) return false;cJSON_AddItemToArray(members,n);
            double basis[9];for (int k=0;k<9;++k) basis[k]=m->local_basis[k/3][k%3];
            if (!cJSON_AddStringToObject(n,"entityId",m->entity_id) || !cJSON_AddNumberToObject(n,"kind",m->kind) ||
                !add_vector(n,"sizeMeters",m->size_meters,3) || !add_vector(n,"localOriginMeters",m->local_origin,3) || !add_vector(n,"localBasis",basis,9)) return false;
        }
    }
    return true;
}
static bool string(const cJSON* o, const char* key, char* value, size_t capacity) {
    const cJSON* n=cJSON_GetObjectItemCaseSensitive(o,key);
    if (!cJSON_IsString(n) || !n->valuestring || strlen(n->valuestring)>=capacity) return false;
    snprintf(value,capacity,"%s",n->valuestring);return true;
}
static bool number(const cJSON* o, const char* key, double* value) {
    const cJSON* n=cJSON_GetObjectItemCaseSensitive(o,key);
    if (!cJSON_IsNumber(n) || !isfinite(n->valuedouble)) return false;
    *value=n->valuedouble;return true;
}
static bool vector(const cJSON* o, const char* key, double* values, int count) {
    const cJSON* a=cJSON_GetObjectItemCaseSensitive(o,key);if (!cJSON_IsArray(a) || cJSON_GetArraySize(a)!=count) return false;
    for (int i=0;i<count;++i) {const cJSON* n=cJSON_GetArrayItem(a,i);if (!cJSON_IsNumber(n) || !isfinite(n->valuedouble)) return false;values[i]=n->valuedouble;}
    return true;
}
bool Layout_MotionReadJson(Layout* l, const cJSON* engineering, int schema) {
    const cJSON* array=cJSON_GetObjectItemCaseSensitive(engineering,"motionEnvelopes");
    if (!array) return schema<18;
    int count=cJSON_GetArraySize(array);
    if (!cJSON_IsArray(array) || count>LAYOUT_MAX_MOTION_ENVELOPES || (schema<18 && count)) return false;
    l->objectStore.motion_envelope_count=(size_t)count;
    for (size_t i=0;i<(size_t)count;++i) {
        const cJSON* o=cJSON_GetArrayItem(array,(int)i);LayoutMotionEnvelope* e=&l->objectStore.motion_envelopes[i];double id,samples;
        if (!number(o,"objectId",&id) || id<1 || id>UINT32_MAX || floor(id)!=id || !number(o,"samples",&samples) || samples<2 || samples>129 || floor(samples)!=samples ||
            !number(o,"paddingMeters",&e->padding_meters) || !string(o,"ruleId",e->rule_id,64) || !string(o,"inputDigest",e->input_digest,17) || !string(o,"boundsDigest",e->bounds_digest,17)) return false;
        e->object_id=(uint32_t)id;e->samples=(uint32_t)samples;
        const cJSON* members=cJSON_GetObjectItemCaseSensitive(o,"members");
        const cJSON* assembly=cJSON_GetObjectItemCaseSensitive(o,"assemblyId");
        if (schema<19) {
            if ((assembly && (!cJSON_IsString(assembly) || !assembly->valuestring || assembly->valuestring[0])) ||
                (members && (!cJSON_IsArray(members) || cJSON_GetArraySize(members)))) return false;
            continue;
        }
        if (!string(o,"assemblyId",e->assembly_id,64) || !cJSON_IsArray(members) || cJSON_GetArraySize(members)>LAYOUT_MAX_MOTION_MEMBERS) return false;
        e->member_count=(size_t)cJSON_GetArraySize(members);
        for (size_t j=0;j<e->member_count;++j) {
            LayoutMotionMember* m=&e->members[j];const cJSON* n=cJSON_GetArrayItem(members,(int)j);double kind,basis[9];
            if (!string(n,"entityId",m->entity_id,64) || !number(n,"kind",&kind) || floor(kind)!=kind || kind<1 || kind>2 ||
                !vector(n,"sizeMeters",m->size_meters,3) || !vector(n,"localOriginMeters",m->local_origin,3) || !vector(n,"localBasis",basis,9)) return false;
            m->kind=(Object3DKind)kind;for (int k=0;k<9;++k) m->local_basis[k/3][k%3]=basis[k];
        }
    }
    return Layout_ValidateEngineering(l,NULL,0);
}
