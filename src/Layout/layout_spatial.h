#pragma once
#include "Layout/layout_engineering.h"

typedef struct {
    uint32_t object_id; /* zero creates a box; existing volume updates in its current rigid frame */
    char name[96], owner[64];
    LayoutVolumeRole role;
    double center_meters[3], size_meters[3];
} LayoutVolumeEdit;
bool Layout_EditVolume(Layout* layout, const LayoutVolumeEdit* edit, bool remove,
    LayoutGeometryBeforePublish history, void* context);
const LayoutSpatialRule* Layout_FindSpatialRule(const LayoutObjectStore* store, const char* id);
bool Layout_EditSpatialRule(Layout* layout, const LayoutSpatialRule* rule, const char* remove,
    LayoutGeometryBeforePublish history, void* context);
bool Layout_ValidateSpatialRecords(const Layout* layout, char* message, size_t capacity);
bool Layout_SpatialEntityReferenced(const LayoutObjectStore* store, const char* id);
bool Layout_SpatialWriteJson(const Layout* layout, cJSON* engineering);
bool Layout_SpatialReadJson(Layout* layout, const cJSON* engineering, bool required);

typedef enum { LAYOUT_SPATIAL_PASS, LAYOUT_SPATIAL_ERROR, LAYOUT_SPATIAL_WARNING } LayoutSpatialSeverity;
typedef struct {
    char rule_id[64], source[64], target[64];
    LayoutSpatialSeverity severity;
    bool approximate, overlap, measurable;
    double distance_meters, required_meters;
    char message[160];
} LayoutSpatialResult;
/* Read-only. Returns full count even with a smaller output buffer. Includes automatic
 * reserved-space checks; reference objects and declared owners are exempt only there. */
size_t Layout_CheckSpatial(const Layout* layout, LayoutSpatialResult* results, size_t capacity);
/* Caller owns the JSON report. Bounded to 4096 records; total/truncated are explicit. */
cJSON* Layout_SpatialReportJson(const Layout* layout);
/* Oriented primitive boxes/panels; meshes use a conservative world bounds proxy.
 * Contact counts as intersection. No mesh triangles or swept motion are tested. */
bool Layout_SpatialDistance(const Layout* layout, const Object3D* a, const Object3D* b,
    double* meters, bool* intersects, bool* approximate);
