#ifndef LINE_DRAWING_AGENT_SCENE_MATERIAL_FLOW_H
#define LINE_DRAWING_AGENT_SCENE_MATERIAL_FLOW_H

#include <stdbool.h>

#include "cjson/cJSON.h"

bool AgentSceneMaterialFlow_ApplyObjectPrompts(cJSON* authoring, const cJSON* request);

#endif
