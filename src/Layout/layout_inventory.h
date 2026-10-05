#pragma once
#include "Layout/layout_engineering.h"

/* Stable-ID inventory uses existing metadata, not a second authoritative registry.
 * Dimensions are read-only observations; the patch changes only named metadata
 * and explicit voltage/current assumptions. No downstream load propagation. */
cJSON* Layout_InventoryJson(const Layout* layout);
bool Layout_EditInventory(Layout* layout, const cJSON* items,
    LayoutGeometryBeforePublish history, void* context);
