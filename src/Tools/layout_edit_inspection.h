#pragma once
#include "Layout/layout_collection_inspection.h"
bool LayoutEditInspection_Handles(const char* operation);
bool LayoutEditInspection_Apply(Layout* layout,const cJSON* operation,cJSON* result);
cJSON* LayoutEditInspection_Report(const Layout* layout,const char* collection);
