#include "Layout/layout_furniture.h"
#include "Layout/layout_json.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool string(const cJSON* item, const char* key, char* dst, size_t n) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(item, key);
    if (!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring) >= n) return false;
    snprintf(dst, n, "%s", v->valuestring); return true;
}
static bool integer(const cJSON* item, const char* key, int* dst, int lo, int hi) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(item, key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < lo || v->valuedouble > hi || floor(v->valuedouble) != v->valuedouble) return false;
    *dst = v->valueint; return true;
}
bool Layout_FurnitureContactsWriteJson(const Layout* l, cJSON* e) {
    if (!cJSON_AddNumberToObject(e, "nextFurnitureContactId", l->objectStore.next_furniture_contact_id)) return false;
    cJSON* array = cJSON_AddArrayToObject(e, "furnitureContacts");
    if (!array) return false;
    for (size_t i = 0; i < l->objectStore.furniture_contact_count; ++i) {
        const LayoutFurnitureContact* c = &l->objectStore.furniture_contacts[i]; cJSON* item = cJSON_CreateObject();
        if (!item) return false;
        cJSON_AddItemToArray(array, item);
        if (!cJSON_AddStringToObject(item, "id", c->id) || !cJSON_AddStringToObject(item, "driver", c->driver) ||
            !cJSON_AddStringToObject(item, "follower", c->follower) || !cJSON_AddNumberToObject(item, "driverEnd", c->driver_end) ||
            !cJSON_AddNumberToObject(item, "followerEnd", c->follower_end) || !cJSON_AddNumberToObject(item, "behavior", c->behavior) ||
            !cJSON_AddNumberToObject(item, "gapMeters", c->gap_m) || !cJSON_AddBoolToObject(item, "enabled", c->enabled)) return false;
    }
    return true;
}
bool Layout_FurnitureContactsReadJson(Layout* l, const cJSON* e, int schema) {
    LayoutObjectStore* s = &l->objectStore;
    memset(s->furniture_contacts, 0, sizeof(s->furniture_contacts)); s->furniture_contact_count = 0; s->next_furniture_contact_id = 1;
    const cJSON* array = cJSON_GetObjectItemCaseSensitive(e, "furnitureContacts");
    if (!array) return schema < LAYOUT_JSON_SCHEMA_VERSION_FURNITURE_CONTACTS;
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) > LAYOUT_MAX_FURNITURE_CONTACTS ||
        (schema < LAYOUT_JSON_SCHEMA_VERSION_FURNITURE_CONTACTS && cJSON_GetArraySize(array))) return false;
    const cJSON* next = cJSON_GetObjectItemCaseSensitive(e, "nextFurnitureContactId");
    if (!cJSON_IsNumber(next) || !isfinite(next->valuedouble) || next->valuedouble < 1 || next->valuedouble > UINT32_MAX || floor(next->valuedouble) != next->valuedouble) return false;
    s->next_furniture_contact_id = (uint32_t)next->valuedouble;
    s->furniture_contact_count = (size_t)cJSON_GetArraySize(array);
    for (size_t i = 0; i < s->furniture_contact_count; ++i) {
        const cJSON* item = cJSON_GetArrayItem(array, (int)i); LayoutFurnitureContact* c = &s->furniture_contacts[i]; int behavior;
        const cJSON* gap = cJSON_GetObjectItemCaseSensitive(item, "gapMeters");
        const cJSON* enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
        if (!string(item, "id", c->id, sizeof(c->id)) || !string(item, "driver", c->driver, sizeof(c->driver)) ||
            !string(item, "follower", c->follower, sizeof(c->follower)) || !integer(item, "driverEnd", &c->driver_end, -1, 1) ||
            !integer(item, "followerEnd", &c->follower_end, -1, 1) || !integer(item, "behavior", &behavior, 0, 1) ||
            !cJSON_IsNumber(gap) || !cJSON_IsBool(enabled)) return false;
        c->behavior = (LayoutFurnitureFollow)behavior; c->gap_m = gap->valuedouble; c->enabled = cJSON_IsTrue(enabled);
    }
    return Layout_ValidateFurnitureContacts(l, true, NULL, 0);
}
