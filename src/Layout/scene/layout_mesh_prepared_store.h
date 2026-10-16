#pragma once
#include "core_mesh_preview.h"
#define LAYOUT_PREVIEW_LOD_COUNT 4u
extern const size_t Layout_MeshPreviewTriangleLimits[LAYOUT_PREVIEW_LOD_COUNT];
/* Private disposable app cache; sources remain authoritative. */
bool Layout_MeshPreparedStoreKey(const char* source, char path[1024]);
bool Layout_MeshPreparedStoreLoad(const char* path, CoreMeshPreviewLodMesh levels[4]);
bool Layout_MeshPreparedStoreSave(const char* path, const CoreMeshPreviewLodMesh levels[4]);
