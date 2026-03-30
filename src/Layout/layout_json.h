#pragma once

#include "layout.h"
#include <stdbool.h>

#define LAYOUT_JSON_SCHEMA_VERSION 4
#define LAYOUT_JSON_SCHEMA_VERSION_Z_ANCHOR 3
#define LAYOUT_JSON_SCHEMA_VERSION_HANDLE_AXIS 4
#define LAYOUT_JSON_SCHEMA_VERSION_MIN_SUPPORTED 0

bool Layout_SaveToFile(const Layout* layout, const char* path);
bool Layout_LoadFromFile(Layout* layout, const char* path);
char* Layout_SaveToString(const Layout* layout);
bool Layout_LoadFromString(Layout* layout, const char* jsonData);
