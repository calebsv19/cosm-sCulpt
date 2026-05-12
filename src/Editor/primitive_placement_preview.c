#include "Editor/primitive_placement_preview.h"

#include "Core/space_mode_adapter.h"
#include "Layout/layout.h"

#include <math.h>

static PlaneFrame3 PlacementPreview_FrameFromConstructionPlane(const GlobalState* state) {
    PlaneFrame3 frame = {0};
    if (!state) return frame;

    const ConstructionPlane3D* cp = &state->layout.scene3d.constructionPlane;
    if (!Layout_ConstructionPlane3D_IsValid(cp)) {
        Plane3 fallbackPlane = Plane3_FromViewPlane((ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f });
        return PlaneFrame3_FromPlane(fallbackPlane, (Vec3){ 0.0f, 0.0f, 0.0f });
    }

    if (cp->mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME) {
        return cp->customFrame;
    }

    Plane3 plane = Plane3_FromViewPlane(cp->axisAligned);
    Vec3 origin = { 0.0f, 0.0f, 0.0f };
    switch (cp->axisAligned.axis) {
        case VIEW_PLANE_YZ: origin.x = cp->axisAligned.offset; break;
        case VIEW_PLANE_XZ: origin.y = cp->axisAligned.offset; break;
        case VIEW_PLANE_XY:
        default: origin.z = cp->axisAligned.offset; break;
    }
    frame = PlaneFrame3_FromPlane(plane, origin);
    frame.origin = origin;
    return frame;
}

static Vec3 PlacementPreview_ResolveSpawnOrigin(const GlobalState* state,
                                                const SpaceViewContext* viewCtx,
                                                PlaneFrame3 frame) {
    Vec3 origin = frame.origin;
    if (!state || !viewCtx) return origin;

    Vec3 candidate = origin;
    const int cx = state->screenWidth / 2;
    const int cy = state->screenHeight / 2;
    if (!SpaceAdapter_ScreenToWorld(cx, cy, &state->grid, viewCtx, false, &candidate)) {
        candidate = viewCtx->camera.target;
    }

    if (state->layout.scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED) {
        switch (state->layout.scene3d.constructionPlane.axisAligned.axis) {
            case VIEW_PLANE_YZ:
                candidate.x = state->layout.scene3d.constructionPlane.axisAligned.offset;
                break;
            case VIEW_PLANE_XZ:
                candidate.y = state->layout.scene3d.constructionPlane.axisAligned.offset;
                break;
            case VIEW_PLANE_XY:
            default:
                candidate.z = state->layout.scene3d.constructionPlane.axisAligned.offset;
                break;
        }
        return candidate;
    }

    Plane3 plane = Plane3_FromPointNormal(frame.origin, frame.normal);
    return Plane3_ProjectPoint(plane, candidate);
}

bool Editor_PrimitivePlacementPreview_Build(const GlobalState* state,
                                            PrimitivePlacementPreviewKind kind,
                                            PrimitivePlacementPreview* outPreview) {
    if (!outPreview) return false;
    *outPreview = (PrimitivePlacementPreview){0};
    if (!state || state->spaceMode != SPACE_MODE_3D) return false;
    if (kind != PRIMITIVE_PLACEMENT_PREVIEW_PLANE &&
        kind != PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM) {
        return false;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    PlaneFrame3 frame = PlacementPreview_FrameFromConstructionPlane(state);
    frame.origin = PlacementPreview_ResolveSpawnOrigin(state, &viewCtx, frame);

    const float defaultSize = fmaxf(state->grid.gridSize * 4.0f, 1.0f);
    outPreview->kind = kind;
    outPreview->frame = frame;
    outPreview->width = defaultSize;
    outPreview->height = defaultSize;
    outPreview->depth = (kind == PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM) ? defaultSize : 0.0f;
    return true;
}

bool Editor_PrimitivePlacementPreview_ComputeCorners(const PrimitivePlacementPreview* preview,
                                                     Vec3 outCorners[8],
                                                     size_t* outCornerCount) {
    if (outCornerCount) *outCornerCount = 0u;
    if (!preview || !outCorners) return false;

    const Vec3 center = preview->frame.origin;
    const Vec3 axisU = Vec3_Normalize(preview->frame.axisU);
    const Vec3 axisV = Vec3_Normalize(preview->frame.axisV);
    const Vec3 axisN = Vec3_Normalize(preview->frame.normal);
    if (Vec3_Length(axisU) <= 1e-5f ||
        Vec3_Length(axisV) <= 1e-5f ||
        Vec3_Length(axisN) <= 1e-5f ||
        preview->width <= 0.0f ||
        preview->height <= 0.0f) {
        return false;
    }

    const float halfW = preview->width * 0.5f;
    const float halfH = preview->height * 0.5f;
    const Vec3 u = Vec3_Scale(axisU, halfW);
    const Vec3 v = Vec3_Scale(axisV, halfH);

    if (preview->kind == PRIMITIVE_PLACEMENT_PREVIEW_PLANE) {
        outCorners[0] = Vec3_Add(center, Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)));
        outCorners[1] = Vec3_Add(center, Vec3_Add(u, Vec3_Scale(v, -1.0f)));
        outCorners[2] = Vec3_Add(center, Vec3_Add(u, v));
        outCorners[3] = Vec3_Add(center, Vec3_Add(Vec3_Scale(u, -1.0f), v));
        if (outCornerCount) *outCornerCount = 4u;
        return true;
    }

    if (preview->kind != PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM || preview->depth < 0.0f) {
        return false;
    }

    const Vec3 n = Vec3_Scale(axisN, preview->depth * 0.5f);
    outCorners[0] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)), Vec3_Scale(n, -1.0f)));
    outCorners[1] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, Vec3_Scale(v, -1.0f)), Vec3_Scale(n, -1.0f)));
    outCorners[2] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, v), Vec3_Scale(n, -1.0f)));
    outCorners[3] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), v), Vec3_Scale(n, -1.0f)));
    outCorners[4] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)), n));
    outCorners[5] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, Vec3_Scale(v, -1.0f)), n));
    outCorners[6] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, v), n));
    outCorners[7] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), v), n));
    if (outCornerCount) *outCornerCount = 8u;
    return true;
}
