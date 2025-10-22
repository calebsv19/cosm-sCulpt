#pragma once

#include "layout.h"
#include <stdbool.h>

bool Layout_SaveToFile(const Layout* layout, const char* path);
bool Layout_LoadFromFile(Layout* layout, const char* path);

