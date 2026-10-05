#pragma once
#include "Layout/layout_routes.h"
#include "Layout/layout_spatial.h"

bool Layout_RouteDesignValid(const Layout* layout, const LayoutPhysicalRoute* route);
bool Layout_RouteDesignWriteJson(const LayoutPhysicalRoute* route, cJSON* json);
bool Layout_RouteDesignReadJson(LayoutPhysicalRoute* route, const cJSON* json);
/* Copper at 20 C, ideal equal-area conductors; explicit return length is required.
 * Does not assess ampacity, protection, connectors, temperature or installation. */
bool Layout_RouteVoltageDrop(const LayoutPhysicalRoute* route, double* volts, double* percent);
double Layout_AWGArea(int awg);
const Object3D* Layout_FindCorridor(const LayoutObjectStore* store, const char* id);
bool Layout_MarkCorridor(Layout* layout, uint32_t object_id, LayoutGeometryBeforePublish history, void* context);
typedef struct {
    char route_id[64], target_id[64], code[32], message[160];
    size_t segment; /* 1-based; zero = entire route */
    LayoutSpatialSeverity severity;
    bool approximate;
} LayoutRouteCheck;
/* Filters never affect checks. Full count is returned even when output is truncated.
 * Corridor coverage uses the union of named oriented boxes; motion is conservative. */
size_t Layout_CheckRoute(const Layout* layout, const LayoutPhysicalRoute* route, LayoutRouteCheck* results, size_t capacity);
cJSON* Layout_RouteChecksJson(const Layout* layout, const LayoutPhysicalRoute* route);
