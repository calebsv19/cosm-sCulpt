#pragma once
#include "Layout/layout_constraints.h"

const LayoutAssembly* Layout_FindAssembly(const LayoutObjectStore* store, const char* id);
const LayoutEntityInfo* Layout_EntityInfo(const LayoutObjectStore* store, const char* id);
const char* Layout_EntityType(const LayoutEntityInfo* info);
bool Layout_IsDescendant(const LayoutObjectStore* store, const char* parent, const char* assembly);
bool Layout_ValidateEngineering(const Layout* layout, char* message, size_t capacity);
bool Layout_HasEngineeringData(const Layout* layout);
bool Layout_SetEntityInfo(Layout* layout, const char* id, const LayoutEntityInfo* info,
    LayoutGeometryBeforePublish history, void* context);
/* Empty id creates a monotonic stable ID. Existing id updates name/properties/parent,
 * preserving world pose. Delete requires an empty assembly. */
bool Layout_EditAssembly(Layout* layout, const LayoutAssembly* assembly, const char* remove_id,
    LayoutGeometryBeforePublish history, void* context);
/* Rigid world translation (meters) and world-axis XYZ rotations (degrees), about
 * the assembly origin, each angle within [-180, 180]. Moves nested frames and geometry atomically, preserving IDs. */
bool Layout_MoveAssembly(Layout* layout, const char* id, const double translation_m[3],
    Vec3 rotation_deg, LayoutGeometryBeforePublish history, void* context);
bool Layout_EntityLocalFrame(const Layout* layout, const char* id, PlaneFrame3* frame);
bool Layout_QueryMatches(const LayoutObjectStore* store, const char* id, const LayoutEntityQuery* query);
bool Layout_ObjectShown(const LayoutObjectStore* store, const Object3D* object);
size_t Layout_QueryEntities(const LayoutObjectStore* store, const LayoutEntityQuery* query,
    char (*ids)[64], size_t capacity);
bool Layout_EngineeringWriteJson(const Layout* layout, cJSON* root);
bool Layout_EngineeringReadJson(Layout* layout, const cJSON* root, bool required);

/* Caller owns the returned JSON object. */
cJSON* Layout_EntityInfoToJson(const LayoutEntityInfo* info);
bool Layout_EntityInfoFromJson(const cJSON* json, LayoutEntityInfo* info);
bool Layout_EntityInfoValid(const LayoutEntityInfo* info);
