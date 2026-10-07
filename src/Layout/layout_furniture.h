#pragma once
#include "Layout/layout_engineering.h"

const LayoutFurnitureUnit* Layout_FindFurnitureUnit(const LayoutObjectStore* store, const char* assembly_id);
const LayoutFurnitureUnit* Layout_FurnitureForObject(const LayoutObjectStore* store, uint32_t object_id);
/* Recipes use the assembly's rigid frame. Size order: depth, run length, height.
 * Outside wall face and floor stay fixed; keep_length is -1 rear, 0 center, +1 front.
 * Applies all parts in one validated/history transaction, preserving part IDs. */
bool Layout_EditFurnitureUnit(Layout* layout, const LayoutFurnitureUnit* unit, int keep_length,
                              LayoutGeometryBeforePublish history, void* context);
bool Layout_ValidateFurniture(const Layout* layout, char* message, size_t capacity);
bool Layout_FurniturePartGeometry(const Layout* layout, const LayoutFurnitureUnit* unit,
                                  size_t part_index, Object3D* result);
/* Candidate helpers require geometryEditActive; publication remains transactional. */
bool Layout_FurnitureRegenerateCandidate(Layout* layout, const LayoutFurnitureUnit* unit);
bool Layout_ValidateFurnitureContacts(const Layout* layout, bool require_contact, char* message, size_t capacity);
bool Layout_SolveFurnitureContacts(Layout* layout, const char* protected_unit);
bool Layout_EditFurnitureContact(Layout* layout, const LayoutFurnitureContact* contact, const char* remove_id,
                                  LayoutGeometryBeforePublish history, void* context);
bool Layout_FurnitureContactsWriteJson(const Layout* layout, cJSON* engineering);
bool Layout_FurnitureContactsReadJson(Layout* layout, const cJSON* engineering, int schema_version);
bool Layout_FurnitureWriteJson(const Layout* layout, cJSON* engineering);
bool Layout_FurnitureReadJson(Layout* layout, const cJSON* engineering, int schema_version);
