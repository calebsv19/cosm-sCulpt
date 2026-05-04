#pragma once

#include "Layout/layout.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LineDrawingSceneAuthoringOptions {
    const char* material_id;
    const char* material_type;
    const char* light_id;
    const char* light_type;
    const char* camera_id;
    const char* camera_type;
} LineDrawingSceneAuthoringOptions;

bool LineDrawingCanonicalScene_ComputeFramingBounds(const Layout* layout, SceneBounds3D* out_bounds);

bool LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(
    const Layout* layout,
    const char* sceneId,
    const char* outputPath,
    const LineDrawingSceneAuthoringOptions* options);

char* LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(
    const Layout* layout,
    const char* sceneId,
    const LineDrawingSceneAuthoringOptions* options);

bool LineDrawingCanonicalScene_ExportLayoutToFile(const Layout* layout,
                                                  const char* sceneId,
                                                  const char* outputPath);

char* LineDrawingCanonicalScene_ExportLayoutToString(const Layout* layout,
                                                     const char* sceneId);

#ifdef __cplusplus
}
#endif
