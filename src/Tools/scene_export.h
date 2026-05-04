#pragma once

#include "Layout/layout.h"
#include "Tools/shape_export.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LineDrawingSceneExportPaths {
    char scene_id[64];
    char scene_dir[SHAPE_EXPORT_PATH_MAX];
    char authoring_path[SHAPE_EXPORT_PATH_MAX];
    char runtime_path[SHAPE_EXPORT_PATH_MAX];
} LineDrawingSceneExportPaths;

bool LineDrawingSceneExport_ExportLayoutToOutputRoot(const Layout* layout,
                                                     const char* layout_path_hint,
                                                     const char* output_root,
                                                     LineDrawingSceneExportPaths* out_paths,
                                                     char* diagnostics,
                                                     size_t diagnostics_size);

#ifdef __cplusplus
}
#endif
