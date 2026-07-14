#pragma once

#include "Layout/scene/layout_scene_authoring.h"
#include "cjson/cJSON.h"

/* Maps canonical scene_authoring_v1 top-level records into editable app state. */
bool LineDrawingSceneAuthoringImport_ParseCanonical(
    const cJSON* root,
    LineDrawingSceneAuthoringState* out_authoring,
    bool* out_has_records);
