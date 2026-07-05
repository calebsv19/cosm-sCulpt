#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LineDrawingSceneProjectManifestObject {
    const char* object_id;
    const char* display_name;
    const char* kind;
    const char* mesh_asset_id;
    const char* source_asset_id;
    const char* source_mesh_sidecar_path;
    size_t vertex_count;
    size_t triangle_count;
    bool has_physics_extension;
    bool has_ray_tracing_extension;
} LineDrawingSceneProjectManifestObject;

typedef struct LineDrawingSceneProjectExportOptions {
    const char* project_name;
    const char* created_by;
    const char* timestamp_utc;
    const char* authoring_scene;
    const char* runtime_scene;
    const LineDrawingSceneProjectManifestObject* objects;
    size_t object_count;
} LineDrawingSceneProjectExportOptions;

bool LineDrawingSceneProjectExport_WriteProjectFiles(const char* project_root,
                                                     const LineDrawingSceneProjectExportOptions* options,
                                                     char* diagnostics,
                                                     size_t diagnostics_size);

#ifdef __cplusplus
}
#endif
