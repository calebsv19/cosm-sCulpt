#include "Layout/layout_routes.h"
#include "Layout/layout_route_design.h"
#include "Layout/layout_saved_views.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool text(const cJSON* object, const char* key, char* output, size_t capacity) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(value) || strlen(value->valuestring) >= capacity) return false;
    snprintf(output, capacity, "%s", value->valuestring);
    return true;
}
static bool vector(const cJSON* array, double point[3]) {
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != 3) return false;
    for (int k = 0; k < 3; ++k) {
        const cJSON* value = cJSON_GetArrayItem(array, k);
        if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble)) return false;
        point[k] = value->valuedouble;
    }
    return true;
}
static cJSON* endpoint_json(const LayoutGeometricReference* reference) {
    cJSON* value = cJSON_CreateObject();
    if (!value) return NULL;
    if (!cJSON_AddStringToObject(value, "entity_id", reference->entity_id) ||
        !cJSON_AddItemToObject(value, "local_offset_m", cJSON_CreateDoubleArray(reference->local_offset_meters, 3))) {
        cJSON_Delete(value); return NULL;
    }
    return value;
}
cJSON* Layout_RouteToJson(const LayoutPhysicalRoute* route) {
    if (!route || route->point_count > LAYOUT_MAX_ROUTE_POINTS) return NULL;
    cJSON* value = Layout_EntityInfoToJson(&route->info);
    if (!value) return NULL;
    cJSON* points = cJSON_AddArrayToObject(value, "points_m");
    if (!points || !cJSON_AddStringToObject(value, "id", route->id) ||
        !cJSON_AddItemToObject(value, "source", endpoint_json(&route->source)) ||
        !cJSON_AddItemToObject(value, "destination", endpoint_json(&route->destination))) {
        cJSON_Delete(value); return NULL;
    }
    for (size_t i = 0; i < route->point_count; ++i) {
        cJSON* point = cJSON_CreateDoubleArray(route->points_meters[i], 3);
        if (!point || !cJSON_AddItemToArray(points, point)) { cJSON_Delete(point); cJSON_Delete(value); return NULL; }
    }
    if (!Layout_RouteDesignWriteJson(route,value)) {cJSON_Delete(value);return NULL;}
    return value;
}
bool Layout_RouteFromJson(const cJSON* value, LayoutPhysicalRoute* route) {
    if (!route || !cJSON_IsObject(value)) return false;
    LayoutPhysicalRoute result = {0};
    const cJSON* a = cJSON_GetObjectItemCaseSensitive(value, "source");
    const cJSON* b = cJSON_GetObjectItemCaseSensitive(value, "destination");
    const cJSON* points = cJSON_GetObjectItemCaseSensitive(value, "points_m");
    if (!text(value, "id", result.id, sizeof(result.id)) || !Layout_EntityInfoFromJson(value, &result.info) ||
        !text(a, "entity_id", result.source.entity_id, sizeof(result.source.entity_id)) ||
        !text(b, "entity_id", result.destination.entity_id, sizeof(result.destination.entity_id)) ||
        !vector(cJSON_GetObjectItemCaseSensitive(a, "local_offset_m"), result.source.local_offset_meters) ||
        !vector(cJSON_GetObjectItemCaseSensitive(b, "local_offset_m"), result.destination.local_offset_meters) ||
        !cJSON_IsArray(points) || cJSON_GetArraySize(points) < 2 || cJSON_GetArraySize(points) > LAYOUT_MAX_ROUTE_POINTS) return false;
    result.point_count = (size_t)cJSON_GetArraySize(points);
    for (size_t i = 0; i < result.point_count; ++i)
        if (!vector(cJSON_GetArrayItem(points, (int)i), result.points_meters[i])) return false;
    if (!Layout_RouteDesignReadJson(&result,value)) return false;
    *route = result;
    return true;
}
bool Layout_RoutesWriteJson(const Layout* layout, cJSON* engineering) {
    if (!Layout_ValidateRoutes(layout, NULL, 0)) return false;
    cJSON* routes = cJSON_AddArrayToObject(engineering, "routes");
    if (!routes || !cJSON_AddNumberToObject(engineering, "nextRouteId",
        layout->objectStore.next_route_id ? layout->objectStore.next_route_id : 1)) return false;
    for (size_t i = 0; i < layout->objectStore.route_count; ++i) {
        cJSON* route = Layout_RouteToJson(&layout->objectStore.routes[i]);
        if (!route || !cJSON_AddItemToArray(routes, route)) { cJSON_Delete(route); return false; }
    }
    return true;
}
bool Layout_RoutesReadJson(Layout* layout, const cJSON* engineering, bool required) {
    const cJSON* routes = cJSON_GetObjectItemCaseSensitive(engineering, "routes");
    const cJSON* next = cJSON_GetObjectItemCaseSensitive(engineering, "nextRouteId");
    if (!routes && !next) return !required;
    if (!cJSON_IsArray(routes) || cJSON_GetArraySize(routes) > LAYOUT_MAX_ROUTES ||
        !cJSON_IsNumber(next) || next->valuedouble < 1 || next->valuedouble > UINT32_MAX ||
        floor(next->valuedouble) != next->valuedouble) return false;
    layout->objectStore.route_count = (size_t)cJSON_GetArraySize(routes);
    layout->objectStore.next_route_id = (uint32_t)next->valuedouble;
    for (size_t i = 0; i < layout->objectStore.route_count; ++i)
        if (!Layout_RouteFromJson(cJSON_GetArrayItem(routes, (int)i), &layout->objectStore.routes[i])) return false;
    return Layout_ValidateRoutes(layout, NULL, 0);
}
cJSON* Layout_RoutesReportJson(const Layout* layout) {
    if (!Layout_ValidateRoutes(layout, NULL, 0)) return NULL;
    cJSON* report = cJSON_CreateObject();
    if (!report) return NULL;
    cJSON* routes = cJSON_AddArrayToObject(report, "routes");
    if (!routes || !cJSON_AddStringToObject(report, "schema", "line_drawing_physical_routes_v1") ||
        !cJSON_AddStringToObject(report, "unit", "m") ||
        !cJSON_AddStringToObject(report, "representation", "world_polyline_centerline_metadata") ||
        !cJSON_AddNumberToObject(report, "count", (double)layout->objectStore.route_count)) {
        cJSON_Delete(report); return NULL;
    }
    for (size_t i = 0; i < layout->objectStore.route_count; ++i) {
        const LayoutPhysicalRoute* route = &layout->objectStore.routes[i];
        cJSON* value = Layout_RouteToJson(route);
        if (!value || !cJSON_AddItemToArray(routes, value)) { cJSON_Delete(value); cJSON_Delete(report); return NULL; }
        if (!cJSON_AddItemToObject(value,"checks",Layout_RouteChecksJson(layout,route))) {cJSON_Delete(report);return NULL;}
        double drop,percent;
        if (Layout_RouteVoltageDrop(route,&drop,&percent)) {
            cJSON_AddNumberToObject(value,"estimated_drop_volts",drop);cJSON_AddNumberToObject(value,"estimated_drop_percent",percent);
            cJSON_AddStringToObject(value,"drop_basis","copper_20C_explicit_return_no_contacts_no_ampacity");
        } else cJSON_AddStringToObject(value,"drop_basis","unknown_or_not_DC");
        double a[3], b[3];
        if (!Layout_RouteEndpoint(layout, &route->source, a) || !Layout_RouteEndpoint(layout, &route->destination, b) ||
            !cJSON_AddNumberToObject(value, "length_m", Layout_RouteLength(route)) ||
            !cJSON_AddStringToObject(value, "endpoint_status", Layout_RouteEndpointsCurrent(layout, route) ? "current" : "stale") ||
            !cJSON_AddItemToObject(value, "resolved_source_m", cJSON_CreateDoubleArray(a, 3)) ||
            !cJSON_AddItemToObject(value, "resolved_destination_m", cJSON_CreateDoubleArray(b, 3))) {
            cJSON_Delete(report); return NULL;
        }
    }
    if (!Layout_SavedViewsWriteJson(layout,report)) {cJSON_Delete(report);return NULL;}
    return report;
}
