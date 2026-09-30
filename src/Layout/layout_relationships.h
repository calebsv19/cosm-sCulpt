#pragma once
#include "Layout/layout_engineering.h"

/* Directed semantic edges are independent of transform parenting and constraints.
 * Source is the part attached to, supported by, or contained by the target. */
const char* Layout_RelationshipType(LayoutRelationshipKind kind);
const LayoutRelationship* Layout_FindRelationship(const LayoutObjectStore* store, const char* id);
bool Layout_ValidateRelationships(const Layout* layout, char* message, size_t capacity);
/* Empty id creates a stable ID; existing id updates the edge. Remove is by ID.
 * All operations participate in the candidate/history/publish transaction. */
bool Layout_EditRelationship(Layout* layout, const LayoutRelationship* relationship,
    const char* remove_id, LayoutGeometryBeforePublish history, void* context);
/* NULL/empty entity matches all edges. kind=-1 matches all types.
 * direction: 0 either endpoint, 1 outgoing, 2 incoming. Returns full match count. */
size_t Layout_QueryRelationships(const LayoutObjectStore* store, const char* entity,
    int kind, int direction, char (*ids)[64], size_t capacity);
/* Caller owns JSON arrays; incident edges retain their original direction. */
cJSON* Layout_RelationshipsToJson(const LayoutObjectStore* store, const char* entity);
bool Layout_RelationshipsWriteJson(const Layout* layout, cJSON* engineering);
bool Layout_RelationshipsReadJson(Layout* layout, const cJSON* engineering, bool required);
