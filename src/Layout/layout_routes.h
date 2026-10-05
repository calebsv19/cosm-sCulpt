#pragma once
#include "Layout/layout_engineering.h"

const LayoutPhysicalRoute* Layout_FindRoute(const LayoutObjectStore* store, const char* id);
bool Layout_ValidateRoutes(const Layout* layout, char* message, size_t capacity);
bool Layout_RouteEntityReferenced(const LayoutObjectStore* store, const char* id);
/* Endpoints are origin references with physical local offsets. Interior points
 * and captured endpoints stay fixed in world meters until an explicit edit. */
bool Layout_RouteEndpoint(const Layout* layout, const LayoutGeometricReference* reference, double point[3]);
bool Layout_RouteEndpointsCurrent(const Layout* layout, const LayoutPhysicalRoute* route);
double Layout_RouteLength(const LayoutPhysicalRoute* route);
/* Empty ID creates; existing ID updates. All edits use candidate validation and
 * one history reservation. Deleting geometry referenced by a route is refused. */
bool Layout_EditRoute(Layout* layout, const LayoutPhysicalRoute* route, const char* remove_id,
    LayoutGeometryBeforePublish history, void* context);
bool Layout_RefreshRouteEndpoints(Layout* layout, const char* id,
    LayoutGeometryBeforePublish history, void* context);
bool Layout_RoutesWriteJson(const Layout* layout, cJSON* engineering);
bool Layout_RoutesReadJson(Layout* layout, const cJSON* engineering, bool required);
cJSON* Layout_RouteToJson(const LayoutPhysicalRoute* route);
bool Layout_RouteFromJson(const cJSON* json, LayoutPhysicalRoute* route);
/* Structured agent readback and export: meter polylines, length and stale state.
 * This is app-owned engineering metadata, not renderer tube geometry. */
cJSON* Layout_RoutesReportJson(const Layout* layout);
