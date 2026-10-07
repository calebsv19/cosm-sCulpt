#include "Layout/layout_furniture.h"
#include "Layout/layout_json.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool string(const cJSON* o, const char* key, char* dst, size_t n) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring) >= n) return false;
    snprintf(dst, n, "%s", v->valuestring); return true;
}
static bool array(const cJSON* o, const char* key, double* dst, int count) {
    const cJSON* a = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsArray(a) || cJSON_GetArraySize(a) != count) return false;
    for (int k = 0; k < count; ++k) {
        const cJSON* v = cJSON_GetArrayItem(a, k);
        if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble)) return false;
        dst[k] = v->valuedouble;
    }
    return true;
}
static bool add_array(cJSON* o, const char* key, const double* values, int n) {
    cJSON* a = cJSON_CreateDoubleArray(values, n);
    if (!a) return false;
    if (!cJSON_AddItemToObject(o, key, a)) { cJSON_Delete(a); return false; }
    return true;
}
bool Layout_FurnitureWriteJson(const Layout* layout, cJSON* engineering) {
    if (!Layout_ValidateFurniture(layout, NULL, 0)) return false;
    cJSON* units = cJSON_AddArrayToObject(engineering, "furnitureUnits");
    if (!units) return false;
    for (size_t i = 0; i < layout->objectStore.furniture_count; ++i) {
        const LayoutFurnitureUnit* u = &layout->objectStore.furniture[i];
        cJSON* item = cJSON_CreateObject();
        if (!item) return false;
        cJSON_AddItemToArray(units, item);
        if (!cJSON_AddStringToObject(item, "assembly", u->assembly_id) ||
            !cJSON_AddBoolToObject(item, "sinkOpening", u->sink_opening) ||
            !cJSON_AddNumberToObject(item, "backSign", u->back_sign) ||
            !add_array(item, "centerMeters", u->center_m, 3) || !add_array(item, "sizeMeters", u->size_m, 3) ||
            !add_array(item, "thicknessMeters", u->thickness_m, 4) ||
            !add_array(item, "overhangMeters", u->overhang_m, 4) ||
            !cJSON_AddNumberToObject(item, "shelfFraction", u->shelf_fraction)) return false;
        cJSON* parts = cJSON_AddArrayToObject(item, "parts");
        if (!parts) return false;
        for (size_t j = 0; j < u->part_count; ++j) {
            const LayoutFurniturePart* p = &u->parts[j]; cJSON* part = cJSON_CreateObject();
            if (!part) return false;
            cJSON_AddItemToArray(parts, part);
            if (!cJSON_AddStringToObject(part, "entity", p->entity_id) ||
                !cJSON_AddNumberToObject(part, "role", p->role) ||
                !add_array(part, "offsetMeters", p->offset_m, 3) ||
                !add_array(part, "spanMeters", p->span_m, 3)) return false;
        }
    }
    return Layout_FurnitureContactsWriteJson(layout, engineering);
}
bool Layout_FurnitureReadJson(Layout* layout, const cJSON* engineering, int schema) {
    const cJSON* units = cJSON_GetObjectItemCaseSensitive(engineering, "furnitureUnits");
    if (!units) return schema < LAYOUT_JSON_SCHEMA_VERSION_FURNITURE && Layout_FurnitureContactsReadJson(layout, engineering, schema);
    if (!cJSON_IsArray(units) || cJSON_GetArraySize(units) > LAYOUT_MAX_FURNITURE_UNITS ||
        (schema < LAYOUT_JSON_SCHEMA_VERSION_FURNITURE && cJSON_GetArraySize(units))) return false;
    LayoutObjectStore* s = &layout->objectStore;
    memset(s->furniture, 0, sizeof(s->furniture));
    s->furniture_count = (size_t)cJSON_GetArraySize(units);
    for (size_t i = 0; i < s->furniture_count; ++i) {
        const cJSON* item = cJSON_GetArrayItem(units, (int)i);
        const cJSON* sign = cJSON_GetObjectItemCaseSensitive(item, "backSign");
        const cJSON* shelf = cJSON_GetObjectItemCaseSensitive(item, "shelfFraction");
        const cJSON* parts = cJSON_GetObjectItemCaseSensitive(item, "parts");
        LayoutFurnitureUnit* u = &s->furniture[i];
        if (!string(item, "assembly", u->assembly_id, sizeof(u->assembly_id)) ||
            !cJSON_IsNumber(sign) || (sign->valuedouble != -1 && sign->valuedouble != 1) ||
            !array(item, "centerMeters", u->center_m, 3) || !array(item, "sizeMeters", u->size_m, 3) ||
            !array(item, "thicknessMeters", u->thickness_m, 4) || !array(item, "overhangMeters", u->overhang_m, 4) ||
            !cJSON_IsNumber(shelf) || !isfinite(shelf->valuedouble) || !cJSON_IsArray(parts) ||
            cJSON_GetArraySize(parts) > LAYOUT_MAX_FURNITURE_PARTS) return false;
        const cJSON* sink = cJSON_GetObjectItemCaseSensitive(item, "sinkOpening");
        if (sink && (!cJSON_IsBool(sink) || (schema < LAYOUT_JSON_SCHEMA_VERSION_PANEL_OPENINGS && cJSON_IsTrue(sink)))) return false;
        u->sink_opening = cJSON_IsTrue(sink);
        u->back_sign = sign->valueint; u->shelf_fraction = shelf->valuedouble;
        u->part_count = (size_t)cJSON_GetArraySize(parts);
        for (size_t j = 0; j < u->part_count; ++j) {
            const cJSON* part = cJSON_GetArrayItem(parts, (int)j);
            const cJSON* role = cJSON_GetObjectItemCaseSensitive(part, "role");
            LayoutFurniturePart* p = &u->parts[j];
            if (!string(part, "entity", p->entity_id, sizeof(p->entity_id)) || !cJSON_IsNumber(role) ||
                role->valuedouble < LAYOUT_FURNITURE_BACK || role->valuedouble > LAYOUT_FURNITURE_CUSHION ||
                (schema < LAYOUT_JSON_SCHEMA_VERSION_FURNITURE_CONTACTS && role->valuedouble == LAYOUT_FURNITURE_RUN_RAIL) ||
                (schema < LAYOUT_JSON_SCHEMA_VERSION_PANEL_OPENINGS && role->valuedouble == LAYOUT_FURNITURE_CUSHION) ||
                floor(role->valuedouble) != role->valuedouble ||
                !array(part, "offsetMeters", p->offset_m, 3) || !array(part, "spanMeters", p->span_m, 3)) return false;
            p->role = (LayoutFurnitureRole)role->valueint;
        }
    }
    return Layout_FurnitureContactsReadJson(layout, engineering, schema) && Layout_ValidateFurniture(layout, NULL, 0);
}
