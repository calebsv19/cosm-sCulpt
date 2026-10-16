#pragma once
#include "Layout/layout.h"
#include "Core/space_mode_adapter.h"
typedef struct { bool visible; unsigned tier; size_t triangles; float diameter; Vec3 lo,hi; } LayoutMeshViewEntry;
typedef struct {
    LayoutMeshViewEntry* entries;
    size_t count, culled_objects, culled_assemblies, planned_triangles, budget, scratch_bytes;
    bool budget_overflow;
} LayoutMeshViewPlan;
bool Layout_MeshBoundsVisible(Vec3 lo,Vec3 hi,const SpaceViewContext* view,const Grid* grid,SDL_Rect clip);
unsigned Layout_MeshScreenTier(float diameter,unsigned previous,bool initialized);
const LayoutMeshViewPlan* Layout_MeshViewPlanBuild(const Layout* layout,const SpaceViewContext* view,
    const Grid* grid,SDL_Rect clip,bool moving,bool inspecting,uint32_t selected,size_t budget);
void Layout_MeshViewPlanShutdown(void);
