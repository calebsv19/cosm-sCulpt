#pragma once

#include "layout.h"
#include <stdbool.h>

#define LAYOUT_JSON_SCHEMA_VERSION 8
#define LAYOUT_JSON_SCHEMA_VERSION_Z_ANCHOR 3
#define LAYOUT_JSON_SCHEMA_VERSION_HANDLE_AXIS 4
#define LAYOUT_JSON_SCHEMA_VERSION_SCENE_BOUNDS 5
#define LAYOUT_JSON_SCHEMA_VERSION_CONSTRUCTION_PLANE 6
#define LAYOUT_JSON_SCHEMA_VERSION_OBJECT3D 7
#define LAYOUT_JSON_SCHEMA_VERSION_OBJECT3D_RECT_PRISM 8
#define LAYOUT_JSON_SCHEMA_VERSION_MIN_SUPPORTED 0

bool Layout_SaveToFile(const Layout* layout, const char* path);
bool Layout_LoadFromFile(Layout* layout, const char* path);
char* Layout_SaveToString(const Layout* layout);
bool Layout_LoadFromString(Layout* layout, const char* jsonData);
