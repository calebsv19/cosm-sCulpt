#include "Core/space_mode_adapter.h"
#include "Core/global_state.h"

static SpaceViewContext SpaceAdapter_DefaultContext(void) {
    SpaceViewContext ctx;
    ctx.plane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    ctx.camera = (FreeViewCamera){
        .enabled = false,
        .yawDeg = 35.0f,
        .pitchDeg = 20.0f,
        .target = {0.0f, 0.0f, 0.0f}
    };
    return ctx;
}

bool SpaceAdapter_Is3DMode(const GlobalState* state) {
    if (!state) return true;
    return state->spaceMode == SPACE_MODE_3D;
}

SpaceViewContext SpaceAdapter_BuildViewContext(const GlobalState* state) {
    SpaceViewContext ctx = SpaceAdapter_DefaultContext();
    if (!state) return ctx;

    ctx.plane = state->activePlane;
    ctx.camera = state->freeViewCamera;

    if (!SpaceAdapter_Is3DMode(state)) {
        ctx.plane.axis = VIEW_PLANE_XY;
        ctx.plane.offset = 0.0f;
        ctx.camera.enabled = false;
    }

    return ctx;
}

bool SpaceAdapter_IsFreeViewEnabled(const SpaceViewContext* ctx) {
    return ctx && ctx->camera.enabled;
}

Vec2 SpaceAdapter_ProjectToView(Vec3 world, const SpaceViewContext* ctx) {
    if (!ctx) return (Vec2){ world.x, world.y };
    return Vec3_ProjectToView(world, ctx->plane, &ctx->camera);
}

bool SpaceAdapter_ScreenToWorld(int screenX,
                                int screenY,
                                const Grid* grid,
                                const SpaceViewContext* ctx,
                                bool snapToGrid,
                                Vec3* outWorld) {
    if (!grid || !ctx || !outWorld) return false;
    return ScreenToPlaneWorld(screenX,
                              screenY,
                              grid,
                              ctx->plane,
                              &ctx->camera,
                              snapToGrid,
                              outWorld);
}

ViewPlaneAxis SpaceAdapter_ActivePlaneAxis(const SpaceViewContext* ctx) {
    if (!ctx) return VIEW_PLANE_XY;
    return ctx->plane.axis;
}

float SpaceAdapter_ActivePlaneOffset(const SpaceViewContext* ctx) {
    if (!ctx) return 0.0f;
    return ctx->plane.offset;
}

const FreeViewCamera* SpaceAdapter_Camera(const SpaceViewContext* ctx) {
    if (!ctx) return NULL;
    return &ctx->camera;
}
