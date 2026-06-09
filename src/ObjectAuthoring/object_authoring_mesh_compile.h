#pragma once

#include "ObjectAuthoring/object_authoring_document.h"
#include "core_mesh_asset.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    CoreObjectVec3 position;
} ObjectAuthoringRuntimeMeshVertex;

typedef struct {
    size_t a;
    size_t b;
    size_t c;
    char surfaceGroupId[64];
} ObjectAuthoringRuntimeMeshTriangle;

typedef struct {
    char groupId[64];
    size_t triangleStart;
    size_t triangleCount;
} ObjectAuthoringRuntimeMeshSurfaceGroup;

typedef struct {
    CoreMeshAssetRuntimeContract contract;
    size_t vertexCount;
    ObjectAuthoringRuntimeMeshVertex* vertices;
    size_t triangleCount;
    ObjectAuthoringRuntimeMeshTriangle* triangles;
    size_t surfaceGroupCount;
    ObjectAuthoringRuntimeMeshSurfaceGroup* surfaceGroups;
} ObjectAuthoringRuntimeMesh;

void ObjectAuthoringRuntimeMesh_Init(ObjectAuthoringRuntimeMesh* mesh);
void ObjectAuthoringRuntimeMesh_Free(ObjectAuthoringRuntimeMesh* mesh);
bool ObjectAuthoringRuntimeMesh_Validate(const ObjectAuthoringRuntimeMesh* mesh,
                                         char* diagnostics,
                                         size_t diagnostics_size);
bool ObjectAuthoringRuntimeMesh_SaveFile(const ObjectAuthoringRuntimeMesh* mesh,
                                         const char* path,
                                         char* diagnostics,
                                         size_t diagnostics_size);

bool ObjectAuthoring_CompileRuntimeMesh(const ObjectAuthoringDocument* source,
                                        const char* runtime_asset_id,
                                        const char* source_asset_id,
                                        ObjectAuthoringRuntimeMesh* out_runtime,
                                        char* diagnostics,
                                        size_t diagnostics_size);
