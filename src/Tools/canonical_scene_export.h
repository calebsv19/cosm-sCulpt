#pragma once

#include "Layout/layout.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LineDrawingSceneMaterialBindingOption {
    const char* binding_id;
    const char* target_kind;
    const char* object_id;
    const char* face_id;
    const char* face_role;
    const char* surface_group_id;
    const char* material_id;
    const char* binding_role;
} LineDrawingSceneMaterialBindingOption;

typedef struct LineDrawingSceneAuthoringOptions {
    const char* material_id;
    const char* material_type;
    const char* light_id;
    const char* light_type;
    const char* camera_id;
    const char* camera_type;
    const char* camera_path_id;
    const char* camera_path_label;
    const char* light_path_id;
    const char* light_path_label;
    const char* preview_mode;
    const LineDrawingSceneMaterialBindingOption* material_bindings;
    size_t material_binding_count;
    double world_scale;
    const char* unit_system;
    const char* conversion_policy;
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
