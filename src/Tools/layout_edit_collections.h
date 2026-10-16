#pragma once
#include "Layout/layout_json.h"
#include "cjson/cJSON.h"
bool LayoutEditCollections_Handles(const char* operation);
bool LayoutEditCollections_Apply(Layout* layout,const cJSON* command,cJSON* result);

bool LayoutEditCollections_Inspect(Layout* layout,const cJSON* command,cJSON* report);
