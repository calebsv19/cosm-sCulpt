#pragma once

#include "Layout/layout.h"
#include "ObjectAuthoring/object_authoring_document.h"

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
bool LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(
    const Layout* layout,
    const ObjectAuthoringDocument* authoring_document,
    const char* path,
    char* diagnostics,
    size_t diagnostics_size);
bool LayoutObjectAssetMeshAuthoring_LoadWithAuthoring(
    Layout* layout,
    ObjectAuthoringDocument* out_authoring_document,
    bool* out_has_authoring_document,
    const char* path,
    char* diagnostics,
    size_t diagnostics_size);
