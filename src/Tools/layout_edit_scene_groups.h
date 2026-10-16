#pragma once
#include "Layout/layout_json.h"
#include "cjson/cJSON.h"
bool LayoutEditSceneGroups_Handles(const char* operation);
bool LayoutEditSceneGroups_Apply(Layout* layout,const cJSON* row,cJSON* result);
