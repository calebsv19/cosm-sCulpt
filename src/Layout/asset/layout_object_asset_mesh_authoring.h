#pragma once

#include "Layout/layout.h"

#include <stdbool.h>
#include <stddef.h>

bool LayoutObjectAssetMeshAuthoring_Save(const Layout* layout,
                                         const char* path,
                                         char* diagnostics,
                                         size_t diagnostics_size);
bool LayoutObjectAssetMeshAuthoring_Load(Layout* layout,
                                         const char* path,
                                         char* diagnostics,
                                         size_t diagnostics_size);
