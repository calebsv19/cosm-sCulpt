#include "Core/space_mode_adapter.h"
#include "Core/global_state.h"

static SpaceViewContext SpaceAdapter_DefaultContext(void) {
    SpaceViewContext ctx = {0};
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
    ViewPlane resolvedPlane = ctx.plane;
    if (!state) return ctx;

    ctx.inspection = state->inspectionView && SpaceAdapter_Is3DMode(state) &&
        state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE;
    ctx.camera = state->freeViewCamera;
    if (Layout_ConstructionPlane3D_IsValid(&state->layout.scene3d.constructionPlane)) {
        resolvedPlane = Layout_ConstructionPlane3D_ToViewPlane(&state->layout.scene3d.constructionPlane);
    } else {
        resolvedPlane = state->activePlane;
    }
    /* Section orientation is a view override; the authored construction plane stays intact. */
    if (state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE &&
        state->sectionView.mode != LAYOUT_SECTION_OFF) {
        resolvedPlane.axis = state->sectionView.axis == 0 ? VIEW_PLANE_YZ :
            state->sectionView.axis == 1 ? VIEW_PLANE_XZ : VIEW_PLANE_XY;
        if (state->sectionView.mode == LAYOUT_SECTION_EXACT) ctx.camera.enabled = false;
    }
    ctx.plane.axis = resolvedPlane.axis;
    ctx.plane.offset = resolvedPlane.offset;

    if (!SpaceAdapter_Is3DMode(state)) {
        ctx.plane.axis = VIEW_PLANE_XY;
        ctx.plane.offset = 0.0f;
        ctx.camera.enabled = false;
    }

    if (state->cameraView.active && SpaceAdapter_Is3DMode(state) &&
        state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE) {
        const CameraViewSession* c = &state->cameraView;
        CorePaneRect rect = {0,0,(float)state->screenWidth,(float)state->screenHeight};
        if(c->viewport[2]>0 && c->viewport[3]>0)
            rect=(CorePaneRect){c->viewport[0],c->viewport[1],c->viewport[2],c->viewport[3]};
        float ppu = state->grid.gridSize * state->grid.scale;
        if (ppu > 0 && rect.width > 0 && rect.height > 0) {
            PerspectiveView* p = &ctx.perspective;
            p->enabled = true;
            p->eye = c->eye;
            /* Inline basis keeps headless scene producers independent of UI/session code. */
            FreeViewCamera basis = {.enabled=true,.yawDeg=c->yaw,.pitchDeg=c->pitch};
            p->forward=FreeView_Forward(&basis);
            Vec3 r=FreeView_Right(&basis), u=FreeView_Up(&basis);
            float roll=DegToRad(c->roll);
            p->right=Vec3_Add(Vec3_Scale(r,cosf(roll)),Vec3_Scale(u,-sinf(roll)));
            p->up=Vec3_Add(Vec3_Scale(u,cosf(roll)),Vec3_Scale(r,sinf(roll)));
            p->tan_half_fov=tanf(DegToRad(c->fov)*.5f);
            p->focal=rect.height*.5f/(p->tan_half_fov*ppu);
            p->aspect=rect.width/rect.height;
            p->center=(Vec2){state->grid.offsetX+(rect.x+rect.width*.5f)/ppu,
                            state->grid.offsetY+(rect.y+rect.height*.5f)/ppu};
            double meters = state->layout.metersPerWorldUnit > 0 ? state->layout.metersPerWorldUnit : 1;
            p->near_clip=(float)(c->near_clip/meters);
            p->far_clip=(float)(c->far_clip/meters);
            ctx.camera=basis;
            ctx.camera.target=c->eye;
        }
    }
    return ctx;
}

bool SpaceAdapter_IsFreeViewEnabled(const SpaceViewContext* ctx) {
    return ctx && ctx->camera.enabled;
}

Vec2 SpaceAdapter_ProjectToView(Vec3 world, const SpaceViewContext* ctx) {
    if (!ctx) return (Vec2){ world.x, world.y };
    if (ctx->perspective.enabled) return PerspectiveView_Project(&ctx->perspective, world);
    return Vec3_ProjectToView(world, ctx->plane, &ctx->camera);
}

bool SpaceAdapter_ScreenToWorld(int screenX,
                                int screenY,
                                const Grid* grid,
                                const SpaceViewContext* ctx,
                                bool snapToGrid,
                                Vec3* outWorld) {
    if (!grid || !ctx || !outWorld) return false;
    if (ctx->perspective.enabled) {
        Vec3 point;
        if (!Ray3_IntersectPlane(PerspectiveView_Ray(&ctx->perspective,
            ScreenToWorld(screenX,screenY,grid)), Plane3_FromViewPlane(ctx->plane),NULL,&point)) return false;
        if (snapToGrid) {
            Vec2 uv=Vec2_Snap(Vec3_ProjectToPlane(point,ctx->plane.axis),grid->gridSize);
            point=Vec3_FromPlaneCoords(uv,ctx->plane.axis,ctx->plane.offset);
        }
        *outWorld=point;
        return true;
    }
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
