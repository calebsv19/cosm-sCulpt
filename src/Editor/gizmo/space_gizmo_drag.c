// src/Editor/space_gizmo_drag.c
#include "Editor/space_gizmo_drag.h"
#include <math.h>

bool GizmoAxisDirection_IsValid(GizmoAxisDirection axis) {
    return axis >= GIZMO_AXIS_DIR_POS_X && axis <= GIZMO_AXIS_DIR_NEG_Z;
}

Vec3 GizmoAxisDirection_WorldVector(GizmoAxisDirection axis) {
    switch (axis) {
        case GIZMO_AXIS_DIR_NEG_X: return (Vec3){ -1.0f, 0.0f, 0.0f };
        case GIZMO_AXIS_DIR_POS_Y: return (Vec3){ 0.0f, 1.0f, 0.0f };
        case GIZMO_AXIS_DIR_NEG_Y: return (Vec3){ 0.0f, -1.0f, 0.0f };
        case GIZMO_AXIS_DIR_POS_Z: return (Vec3){ 0.0f, 0.0f, 1.0f };
        case GIZMO_AXIS_DIR_NEG_Z: return (Vec3){ 0.0f, 0.0f, -1.0f };
        case GIZMO_AXIS_DIR_POS_X:
        default:
            return (Vec3){ 1.0f, 0.0f, 0.0f };
    }
}

float GizmoDrag_SignedPixelsAlongAxis(Vec2 mouseStartScreen,
                                      Vec2 mouseNowScreen,
                                      Vec2 axisScreenVector) {
    const float axisLen = sqrtf(axisScreenVector.x * axisScreenVector.x +
                                axisScreenVector.y * axisScreenVector.y);
    if (axisLen <= 1e-6f) return 0.0f;

    const Vec2 axisUnit = {
        .x = axisScreenVector.x / axisLen,
        .y = axisScreenVector.y / axisLen
    };
    const Vec2 mouseDelta = {
        .x = mouseNowScreen.x - mouseStartScreen.x,
        .y = mouseNowScreen.y - mouseStartScreen.y
    };

    return mouseDelta.x * axisUnit.x + mouseDelta.y * axisUnit.y;
}

float GizmoDrag_DistanceWorldFromPixels(float signedPixels, float worldUnitsPerPixel) {
    if (!isfinite(signedPixels) || !isfinite(worldUnitsPerPixel)) return 0.0f;
    if (worldUnitsPerPixel <= 1e-6f) return 0.0f;
    return signedPixels * worldUnitsPerPixel;
}

float GizmoDrag_QuantizeDistance(float signedWorldDistance, float step) {
    if (!isfinite(signedWorldDistance)) return 0.0f;
    if (!isfinite(step) || step <= 1e-6f) return signedWorldDistance;
    return roundf(signedWorldDistance / step) * step;
}

float GizmoDrag_ResolveDistance(float signedPixels,
                                float worldUnitsPerPixel,
                                float step,
                                bool smooth) {
    const float rawWorldDistance = GizmoDrag_DistanceWorldFromPixels(signedPixels,
                                                                     worldUnitsPerPixel);
    if (smooth) return rawWorldDistance;
    return GizmoDrag_QuantizeDistance(rawWorldDistance, step);
}

Vec3 GizmoDrag_ApplyAxisDistance(Vec3 startWorld,
                                 GizmoAxisDirection axis,
                                 float signedWorldDistance) {
    if (!GizmoAxisDirection_IsValid(axis)) return startWorld;
    if (!isfinite(signedWorldDistance)) return startWorld;
    const Vec3 axisDir = GizmoAxisDirection_WorldVector(axis);
    return Vec3_Add(startWorld, Vec3_Scale(axisDir, signedWorldDistance));
}
