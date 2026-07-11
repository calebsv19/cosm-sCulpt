#ifndef CANONICAL_SCENE_EXPORT_MATERIALS_H
#define CANONICAL_SCENE_EXPORT_MATERIALS_H

#include <stdbool.h>

#include "Tools/canonical_scene_export.h"
#include "cjson/cJSON.h"

bool LineDrawingCanonicalScene_IsAllowedMaterialBindingTargetKind(const char* value);
bool LineDrawingCanonicalScene_ValidateMaterialBindingOption(
    const LineDrawingSceneMaterialBindingOption* option);
bool LineDrawingCanonicalScene_AppendDefaultObjectMaterialBinding(cJSON* material_bindings,
                                                                  const char* object_id,
                                                                  const char* material_id,
                                                                  const char* binding_role);
bool LineDrawingCanonicalScene_AppendMaterialBindingOptions(
    cJSON* material_bindings,
    const LineDrawingSceneMaterialBindingOption* options,
    size_t option_count,
    const char* default_material_id);

#endif
