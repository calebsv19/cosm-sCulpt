#pragma once

#include "Layout/layout.h"
#include "cjson/cJSON.h"

#include <stdbool.h>

bool LineDrawingCanonicalScene_HasLiveSceneAuthoringRecords(const Layout* layout);

bool LineDrawingCanonicalScene_AppendLiveSceneAuthoringRecords(cJSON* materials,
                                                               cJSON* lights,
                                                               cJSON* cameras,
                                                               cJSON* paths,
                                                               const Layout* layout,
                                                               const char* material_type,
                                                               const char* camera_type);
