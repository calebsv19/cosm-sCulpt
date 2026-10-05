#include "Layout/layout_routes.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fail(Layout* layout, const char* text) {
    snprintf(layout->geometryMessage, sizeof(layout->geometryMessage), "%s", text);
    return false;
}
static bool identifier(const char* id, size_t capacity) {
    if (!memchr(id, 0, capacity) || !id[0]) return false;
    for (const unsigned char* p = (const unsigned char*)id; *p; ++p)
        if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.') return false;
    return true;
}
const LayoutPhysicalRoute* Layout_FindRoute(const LayoutObjectStore* store, const char* id) {
    if (!store || !id) return NULL;
    for (size_t i = 0; i < store->route_count && i < LAYOUT_MAX_ROUTES; ++i)
        if (!strcmp(store->routes[i].id, id)) return &store->routes[i];
    return NULL;
}
bool Layout_RouteEntityReferenced(const LayoutObjectStore* store, const char* id) {
    if (!store || !id) return false;
    for (size_t i = 0; i < store->route_count && i < LAYOUT_MAX_ROUTES; ++i)
        if (!strcmp(store->routes[i].source.entity_id, id) ||
            !strcmp(store->routes[i].destination.entity_id, id)) return true;
    return false;
}
bool Layout_RouteEndpoint(const Layout* layout, const LayoutGeometricReference* ref, double point[3]) {
    if (!layout || !ref || !point || !memchr(ref->entity_id, 0, sizeof(ref->entity_id)) ||
        ref->kind != LAYOUT_REFERENCE_ORIGIN || ref->face != OBJECT3D_FACE_NONE) return false;
    LayoutResolvedReference resolved;
    if (Layout_ResolveReference(layout, ref, &resolved) != LAYOUT_MEASUREMENT_OK) return false;
    memcpy(point, resolved.point_meters, sizeof(resolved.point_meters));
    return true;
}
double Layout_RouteLength(const LayoutPhysicalRoute* route) {
    if (!route || route->point_count < 2 || route->point_count > LAYOUT_MAX_ROUTE_POINTS) return NAN;
    double length = 0;
    for (size_t i = 1; i < route->point_count; ++i) {
        double x = route->points_meters[i][0] - route->points_meters[i-1][0];
        double y = route->points_meters[i][1] - route->points_meters[i-1][1];
        double z = route->points_meters[i][2] - route->points_meters[i-1][2];
        length += hypot(hypot(x, y), z);
    }
    return length;
}
bool Layout_RouteEndpointsCurrent(const Layout* layout, const LayoutPhysicalRoute* route) {
    if (!route || route->point_count < 2 || route->point_count > LAYOUT_MAX_ROUTE_POINTS) return false;
    double a[3], b[3];
    if (!Layout_RouteEndpoint(layout, &route->source, a) || !Layout_RouteEndpoint(layout, &route->destination, b)) return false;
    for (int k = 0; k < 3; ++k)
        if (fabs(a[k] - route->points_meters[0][k]) > 1e-6 ||
            fabs(b[k] - route->points_meters[route->point_count-1][k]) > 1e-6) return false;
    return true;
}
bool Layout_ValidateRoutes(const Layout* layout, char* message, size_t capacity) {
    if (!layout) return false;
    const LayoutObjectStore* store = &layout->objectStore;
    const char* reason = NULL;
    if (store->route_count > LAYOUT_MAX_ROUTES) reason = "A scene supports up to 32 physical routes.";
    for (size_t i = 0; !reason && i < store->route_count; ++i) {
        const LayoutPhysicalRoute* route = &store->routes[i];
        double endpoint[3];
        if (!identifier(route->id, sizeof(route->id)) || !Layout_EntityInfoValid(&route->info) ||
            !route->info.label[0] || (strcmp(route->info.entity_type, "Cable") && strcmp(route->info.entity_type, "Pipe")) ||
            route->info.parent_id[0] || route->info.reference || route->info.volume_role || route->info.volume_owner[0])
            reason = "Routes need an ID, name, Cable/Pipe type and unparented design metadata.";
        else if (!Layout_RouteEndpoint(layout, &route->source, endpoint) ||
                 !Layout_RouteEndpoint(layout, &route->destination, endpoint) ||
                 !strcmp(route->source.entity_id, route->destination.entity_id))
            reason = "Choose two different existing plane/prism endpoints with origin references.";
        else if (route->point_count < 2 || route->point_count > LAYOUT_MAX_ROUTE_POINTS)
            reason = "Routes need 2 to 64 ordered points, including endpoints.";
        else for (size_t p = 0; !reason && p < route->point_count; ++p)
            for (int k = 0; k < 3; ++k)
                if (!isfinite(route->points_meters[p][k]) || fabs(route->points_meters[p][k]) > 1e9)
                    reason = "Route coordinates must be finite physical meters within +/-1e9.";
        if (!reason && !isfinite(Layout_RouteLength(route))) reason = "Route length is not finite.";
        if (!reason && Layout_EntityInfo(store, route->id)) reason = "Route identity conflicts with an object or assembly.";
        for (size_t j = 0; !reason && j < i; ++j)
            if (!strcmp(route->id, store->routes[j].id)) reason = "Route IDs must be unique.";
    }
    if (reason && message && capacity) snprintf(message, capacity, "%s", reason);
    return !reason;
}
typedef struct { const LayoutPhysicalRoute* route; const char* remove; } RouteEdit;
static bool edit(Layout* layout, void* context) {
    const RouteEdit* command = context;
    LayoutObjectStore* store = &layout->objectStore;
    if (command->remove) {
        for (size_t i = 0; i < store->route_count; ++i) if (!strcmp(store->routes[i].id, command->remove)) {
            memmove(&store->routes[i], &store->routes[i+1], (store->route_count-i-1)*sizeof(LayoutPhysicalRoute));
            memset(&store->routes[--store->route_count], 0, sizeof(LayoutPhysicalRoute));
            return true;
        }
        return fail(layout, "Route no longer exists.");
    }
    if (!command->route || !memchr(command->route->id, 0, sizeof(command->route->id))) return false;
    LayoutPhysicalRoute next = *command->route;
    if (next.id[0]) {
        LayoutPhysicalRoute* saved = (LayoutPhysicalRoute*)Layout_FindRoute(store, next.id);
        if (!saved) return fail(layout, "Choose an existing route or leave ID empty to create.");
        *saved = next;
    } else {
        if (store->route_count == LAYOUT_MAX_ROUTES) return fail(layout, "A scene supports up to 32 routes.");
        do {
            if (!store->next_route_id) store->next_route_id = 1;
            if (store->next_route_id == UINT32_MAX) return fail(layout, "Route ID space exhausted.");
            snprintf(next.id, sizeof(next.id), "route_%u", store->next_route_id++);
        } while (Layout_FindRoute(store, next.id) || Layout_EntityInfo(store, next.id));
        store->routes[store->route_count++] = next;
    }
    return true;
}
bool Layout_EditRoute(Layout* layout, const LayoutPhysicalRoute* route, const char* remove_id,
    LayoutGeometryBeforePublish history, void* context) {
    if (!layout || (!route && !remove_id)) return false;
    RouteEdit command = {route, remove_id};
    return Layout_RunGeometryEdit(layout, 0, edit, &command, history, context);
}
bool Layout_RefreshRouteEndpoints(Layout* layout, const char* id,
    LayoutGeometryBeforePublish history, void* context) {
    const LayoutPhysicalRoute* saved = layout ? Layout_FindRoute(&layout->objectStore, id) : NULL;
    if (!saved || saved->point_count < 2 || saved->point_count > LAYOUT_MAX_ROUTE_POINTS) return false;
    LayoutPhysicalRoute route = *saved;
    if (!Layout_RouteEndpoint(layout, &route.source, route.points_meters[0]) ||
        !Layout_RouteEndpoint(layout, &route.destination, route.points_meters[route.point_count-1]))
        return fail(layout, "Route endpoints cannot resolve; choose supported objects.");
    return Layout_EditRoute(layout, &route, NULL, history, context);
}
