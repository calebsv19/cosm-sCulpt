#pragma once

#include "Math/math_util.h"
#include "Layout/Grid/grid.h"

typedef struct GlobalState GlobalState;

typedef struct {
    ViewPlane plane;
    FreeViewCamera camera;
} SpaceViewContext;

SpaceViewContext SpaceAdapter_BuildViewContext(const GlobalState* state);
bool SpaceAdapter_Is3DMode(const GlobalState* state);
bool SpaceAdapter_IsFreeViewEnabled(const SpaceViewContext* ctx);
Vec2 SpaceAdapter_ProjectToView(Vec3 world, const SpaceViewContext* ctx);
bool SpaceAdapter_ScreenToWorld(int screenX,
                                int screenY,
                                const Grid* grid,
                                const SpaceViewContext* ctx,
                                bool snapToGrid,
                                Vec3* outWorld);
ViewPlaneAxis SpaceAdapter_ActivePlaneAxis(const SpaceViewContext* ctx);
float SpaceAdapter_ActivePlaneOffset(const SpaceViewContext* ctx);
const FreeViewCamera* SpaceAdapter_Camera(const SpaceViewContext* ctx);
