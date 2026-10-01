#include "Layout/layout_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

bool Layout_MotionWriteJson(const Layout* l, cJSON* engineering) {
    cJSON* array=cJSON_AddArrayToObject(engineering,"motionEnvelopes");
    if (!array) return false;
    for (size_t i=0;i<l->objectStore.motion_envelope_count;++i) {
        const LayoutMotionEnvelope* e=&l->objectStore.motion_envelopes[i];cJSON* o=cJSON_CreateObject();
        if (!o) return false;
        cJSON_AddItemToArray(array,o);
        if (!cJSON_AddNumberToObject(o,"objectId",e->object_id) || !cJSON_AddStringToObject(o,"ruleId",e->rule_id) ||
            !cJSON_AddNumberToObject(o,"samples",e->samples) || !cJSON_AddNumberToObject(o,"paddingMeters",e->padding_meters) ||
            !cJSON_AddStringToObject(o,"inputDigest",e->input_digest) || !cJSON_AddStringToObject(o,"boundsDigest",e->bounds_digest)) return false;
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
bool Layout_MotionReadJson(Layout* l, const cJSON* engineering, bool required) {
    const cJSON* array=cJSON_GetObjectItemCaseSensitive(engineering,"motionEnvelopes");
    if (!array) return !required;
    int count=cJSON_GetArraySize(array);
    if (!cJSON_IsArray(array) || count>LAYOUT_MAX_MOTION_ENVELOPES || (!required && count)) return false;
    l->objectStore.motion_envelope_count=(size_t)count;
    for (size_t i=0;i<(size_t)count;++i) {
        const cJSON* o=cJSON_GetArrayItem(array,(int)i);LayoutMotionEnvelope* e=&l->objectStore.motion_envelopes[i];double id,samples;
        if (!number(o,"objectId",&id) || id<1 || id>UINT32_MAX || floor(id)!=id || !number(o,"samples",&samples) || samples<2 || samples>129 || floor(samples)!=samples ||
            !number(o,"paddingMeters",&e->padding_meters) || !string(o,"ruleId",e->rule_id,64) || !string(o,"inputDigest",e->input_digest,17) || !string(o,"boundsDigest",e->bounds_digest,17)) return false;
        e->object_id=(uint32_t)id;e->samples=(uint32_t)samples;
    }
    return Layout_ValidateEngineering(l,NULL,0);
}
