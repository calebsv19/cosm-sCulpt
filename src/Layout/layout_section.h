#pragma once
#include "Layout/layout_constraints.h"

/* View-only clipping, independent of construction planes and saved geometry. */
typedef enum { LAYOUT_SECTION_OFF, LAYOUT_SECTION_EXACT, LAYOUT_SECTION_CUTAWAY } LayoutSectionMode;
typedef struct {
    LayoutSectionMode mode;
    int axis; /* 0 X (across), 1 Y (along), 2 Z (height). */
    double position_meters;
    double step_meters;
    bool flipped; /* Cutaway keeps the positive half instead of the negative. */
} LayoutSectionView;
typedef struct {
    Vec3 a, b, c;
    bool cap;
} LayoutSurfaceTriangle;
#define LAYOUT_SURFACE_MAX_TRIANGLES 64

/* Convex native surfaces in world coordinates. Exact sections contain only the
 * intersection polygon; clipping caps are display geometry, never saved meshes. */
size_t Layout_BuildNativeSurface(const Object3D* object, const LayoutSectionView* section,
                                 double meters_per_world,
                                 LayoutSurfaceTriangle out[LAYOUT_SURFACE_MAX_TRIANGLES]);
size_t Layout_ClipSectionPolygon(const Vec3* input, size_t count, int axis, float position, bool flipped,
                                 Vec3 output[12]);
bool Layout_SectionRange(const Layout* layout, int axis, double* minimum, double* maximum);
int Layout_PanelThicknessAxis(const Object3D* object);
/* Keep -1 negative face, 0 center or +1 positive face in the prism's local axis.
 * Only explicit Panel prisms with unit scale are editable through this contract. */
bool Layout_SetPanelThickness(Layout* layout, uint32_t object_id, double meters, int keep_face,
                              LayoutGeometryBeforePublish history, void* context);
