#pragma once
#include "Layout/layout.h"
#include "cjson/cJSON.h"
/* Offline typed motion commands; publication belongs to the create-only CLI. */
bool LayoutEditMotion_Handles(const char *operation);
bool LayoutEditMotion_Apply(Layout *layout, const cJSON *command, cJSON *result);
