#include "Layout/layout_routes.h"
#include "Layout/layout_route_design.h"
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
    for (size_t i=0;i<store->route_count;++i) for (size_t j=0;j<store->routes[i].corridor_count;++j)
        if (!strcmp(store->routes[i].corridor_ids[j],id)) return true;
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
        if (!reason && !Layout_RouteDesignValid(layout,route)) reason="Invalid corridor membership or electrical inputs; use finite nonnegative values.";
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

typedef struct { const char* id; size_t point; const char* junction; } RouteSplit;
static bool section_property(LayoutPhysicalRoute* r, const char* origin) {
    LayoutProperty* p = NULL;
    for (size_t i = 0; i < r->info.property_count; ++i)
        if (!strcmp(r->info.properties[i].key, "section_of")) p = &r->info.properties[i];
    if (p) return true; /* Preserve the first section's family through repeated splits. */
    if (r->info.property_count == LAYOUT_MAX_PROPERTIES) return false;
    p = &r->info.properties[r->info.property_count++];
    memset(p, 0, sizeof(*p));
    snprintf(p->key, sizeof(p->key), "section_of");
    snprintf(p->text, sizeof(p->text), "%s", origin);
    return true;
}
static bool split(Layout* l, void* context) {
    const RouteSplit* cmd = context;
    LayoutObjectStore* store = &l->objectStore;
    const LayoutPhysicalRoute* saved = Layout_FindRoute(store, cmd->id);
    if (!saved || !cmd->point || cmd->point + 1 >= saved->point_count)
        return fail(l, "Choose an interior point on a saved route.");
    if (store->route_count == LAYOUT_MAX_ROUTES)
        return fail(l, "Route capacity reached; split needs one additional section.");
    if (!Layout_RouteEndpointsCurrent(l, saved)) return fail(l, "Refresh stale endpoints before splitting.");
    LayoutPhysicalRoute a = *saved, b = *saved;
    if (!section_property(&a, saved->id) || !section_property(&b, saved->id))
        return fail(l, "Free a metadata slot for section_of before splitting.");
    LayoutGeometricReference ref = {0};
    if (cmd->junction && cmd->junction[0]) {
        const LayoutEntityInfo* info = Layout_EntityInfo(store, cmd->junction);
        if (!info || (strcmp(info->entity_type, "Connector") && strcmp(info->entity_type, "PowerBus")))
            return fail(l, "A shared junction must be an existing Connector or PowerBus.");
        snprintf(ref.entity_id, sizeof(ref.entity_id), "%s", cmd->junction);
    } else {
        double scale = Layout_WorldScale(l);
        RectPrismPrimitiveCreateParams p = {.width=(float)(.016/scale), .height=(float)(.016/scale),
            .depth=(float)(.016/scale), .useExplicitFrame=true,
            .explicitFrame={.origin={(float)(a.points_meters[cmd->point][0]/scale),
                (float)(a.points_meters[cmd->point][1]/scale), (float)(a.points_meters[cmd->point][2]/scale)},
                .axisU={1,0,0}, .axisV={0,1,0}, .normal={0,0,1}}};
        uint32_t object_id = 0; bool adjusted = false;
        if (!Layout_CreateRectPrismPrimitive(l, &p, &object_id, &adjusted) || adjusted)
            return fail(l, "Junction position conflicts with bounds or geometry precision.");
        Object3D* o = Layout_ObjectStore_Find(store, object_id);
        snprintf(o->info.entity_type, sizeof(o->info.entity_type), "Connector");
        snprintf(o->info.label, sizeof(o->info.label), "Junction / %.64s", a.id);
        LayoutProperty* domain=&o->info.properties[o->info.property_count++];
        snprintf(domain->key,sizeof(domain->key),"power_domain");
        snprintf(domain->text,sizeof(domain->text),"%s",a.electrical.power_domain);
        for(size_t i=0;i<a.info.property_count;++i)if(!strcmp(a.info.properties[i].key,"bus") && a.info.properties[i].text[0])
            o->info.properties[o->info.property_count++]=a.info.properties[i];
        snprintf(ref.entity_id, sizeof(ref.entity_id), "%s", o->coreMeta.object_id);
    }
    double resolved[3];
    if (!Layout_RouteEndpoint(l, &ref, resolved)) return fail(l, "Junction cannot resolve.");
    for (int k = 0; k < 3; ++k)
        if (fabs(resolved[k] - a.points_meters[cmd->point][k]) > 1e-6)
            return fail(l, "Selected junction must match this point within 0.001 mm.");
    a.destination = ref; a.point_count = cmd->point + 1;
    memset(a.points_meters + a.point_count, 0, (LAYOUT_MAX_ROUTE_POINTS-a.point_count)*sizeof(a.points_meters[0]));
    b.source = ref; b.point_count -= cmd->point;
    memmove(b.points_meters, b.points_meters + cmd->point, b.point_count*sizeof(b.points_meters[0]));
    memset(b.points_meters + b.point_count, 0, (LAYOUT_MAX_ROUTE_POINTS-b.point_count)*sizeof(b.points_meters[0]));
    double length = Layout_RouteLength(saved), first = Layout_RouteLength(&a);
    if (first <= 1e-9 || length-first <= 1e-9) return fail(l, "Split must leave two nonzero sections.");
    a.electrical.return_meters = saved->electrical.return_meters * first / length;
    b.electrical.return_meters = saved->electrical.return_meters - a.electrical.return_meters;
    b.electrical.allowance_meters = 0;
    a.maximum_length_meters = b.maximum_length_meters = 0;
    snprintf(a.info.label, sizeof(a.info.label), "%.80s / A", saved->info.label);
    snprintf(b.info.label, sizeof(b.info.label), "%.80s / B", saved->info.label);
    b.id[0] = 0;
    RouteEdit first_edit = {&a, NULL}, second_edit = {&b, NULL};
    return edit(l, &first_edit) && edit(l, &second_edit);
}
bool Layout_SplitRoute(Layout* l, const char* id, size_t point, const char* junction,
    LayoutGeometryBeforePublish history, void* context) {
    if (!l || !id || (junction && strlen(junction) >= 64)) return false;
    RouteSplit cmd = {id, point, junction};
    return Layout_RunGeometryEdit(l, 0, split, &cmd, history, context);
}
cJSON* Layout_RouteConnectionsJson(const Layout* l) {
    if (!Layout_ValidateRoutes(l, NULL, 0)) return NULL;
    cJSON* nodes = cJSON_CreateArray();
    if (!nodes) return NULL;
    const LayoutObjectStore* store = &l->objectStore;
    for (size_t i = 0; i < store->count; ++i) {
        const Object3D* o = &store->items[i];
        if (o->isDeleted) continue;
        cJSON* node = cJSON_CreateObject();
        cJSON* incidents = cJSON_AddArrayToObject(node, "sections");
        size_t count = 0; bool mismatch = false, offset_mismatch = false, stale = false;
        const LayoutPhysicalRoute* baseline = NULL; LayoutGeometricReference first = {0};
        bool junction = !strcmp(o->info.entity_type, "Connector") || !strcmp(o->info.entity_type, "PowerBus");
        for (size_t j = 0; j < store->route_count; ++j) {
            const LayoutPhysicalRoute* r = &store->routes[j];
            const LayoutGeometricReference* ref = !strcmp(r->source.entity_id, o->coreMeta.object_id) ? &r->source :
                !strcmp(r->destination.entity_id, o->coreMeta.object_id) ? &r->destination : NULL;
            if (!ref) continue;
            cJSON* entry = cJSON_CreateObject();
            cJSON_AddStringToObject(entry, "route_id", r->id);
            cJSON_AddStringToObject(entry, "end", ref == &r->source ? "source" : "destination");
            cJSON_AddStringToObject(entry, "domain", r->electrical.power_domain);
            cJSON_AddStringToObject(entry, "circuit", r->electrical.circuit);
            cJSON_AddItemToArray(incidents, entry);
            stale |= !Layout_RouteEndpointsCurrent(l, r);
            if (!baseline) { baseline = r; first = *ref; }
            else {
                const LayoutRouteElectrical* a = &baseline->electrical; const LayoutRouteElectrical* b = &r->electrical;
                mismatch |= (a->voltage_class && b->voltage_class && a->voltage_class != b->voltage_class) ||
                    (a->power_domain[0] && b->power_domain[0] && strcmp(a->power_domain, b->power_domain)) ||
                    (a->circuit[0] && b->circuit[0] && strcmp(a->circuit,b->circuit)) ||
                    (a->nominal_volts > 0 && b->nominal_volts > 0 && fabs(a->nominal_volts-b->nominal_volts)>1e-6);
                for (int k = 0; k < 3; ++k) offset_mismatch |= fabs(first.local_offset_meters[k]-ref->local_offset_meters[k])>1e-6;
            }
            ++count;
        }
        if (!count) { cJSON_Delete(node); continue; }
        cJSON_AddStringToObject(node, "entity_id", o->coreMeta.object_id);
        cJSON_AddStringToObject(node, "name", o->info.label[0] ? o->info.label : o->coreMeta.object_id);
        cJSON_AddBoolToObject(node, "passive_junction", junction);
        cJSON_AddNumberToObject(node, "degree", (double)count);
        cJSON_AddBoolToObject(node, "domain_conflict", junction && mismatch);
        cJSON_AddBoolToObject(node, "port_offsets_differ", offset_mismatch);
        cJSON_AddBoolToObject(node, "stale_sections", stale);
        cJSON_AddItemToArray(nodes, node);
    }
    return nodes;
}
