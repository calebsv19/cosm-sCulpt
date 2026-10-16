#pragma once
#include "Layout/layout_engineering.h"

typedef struct {
    const char *id, *label, *texture;
    unsigned rgb;
    double roughness, reflectivity;
} LayoutSurfaceMaterial;
/* Render finish is independent of construction material and physical properties.
 * Empty means default. Unknown explicit finishes are rejected, not guessed. */
const LayoutSurfaceMaterial *Layout_SurfaceMaterialFind(const char *id);
const char *Layout_SurfaceMaterialId(const LayoutEntityInfo *info);
const char *Layout_SurfaceMaterialNext(const LayoutEntityInfo *info);
bool Layout_SurfaceMaterialValid(const LayoutEntityInfo *info);
bool Layout_SetSurfaceMaterial(Layout *layout, const char *object_id, const char *preset,
    LayoutGeometryBeforePublish history, void *context);

/* Resolves a preset or explicit solid finish into caller-owned storage. */
bool Layout_SurfaceMaterialResolve(const LayoutEntityInfo *info, LayoutSurfaceMaterial *out);
