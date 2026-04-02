// src/Editor/space_gizmo_drag.h
#pragma once

#include "Math/math_util.h"
#include <stdbool.h>

// Six-direction axis handles used by the free-view 3D translate gizmo.
typedef enum {
    GIZMO_AXIS_DIR_POS_X = 0,
    GIZMO_AXIS_DIR_NEG_X = 1,
    GIZMO_AXIS_DIR_POS_Y = 2,
    GIZMO_AXIS_DIR_NEG_Y = 3,
    GIZMO_AXIS_DIR_POS_Z = 4,
    GIZMO_AXIS_DIR_NEG_Z = 5
} GizmoAxisDirection;

// Runtime session state for one active axis-drag interaction.
typedef struct {
    bool active;
    GizmoAxisDirection axis;
    int anchorIndex;
    Vec2 mouseStartScreen;
    Vec3 primaryStartWorld;
    float worldUnitsPerPixel;
    bool smooth;
} GizmoAxisDragSession;

bool GizmoAxisDirection_IsValid(GizmoAxisDirection axis);
Vec3 GizmoAxisDirection_WorldVector(GizmoAxisDirection axis);

float GizmoDrag_SignedPixelsAlongAxis(Vec2 mouseStartScreen,
                                      Vec2 mouseNowScreen,
                                      Vec2 axisScreenVector);
float GizmoDrag_DistanceWorldFromPixels(float signedPixels, float worldUnitsPerPixel);
float GizmoDrag_QuantizeDistance(float signedWorldDistance, float step);
float GizmoDrag_ResolveDistance(float signedPixels,
                                float worldUnitsPerPixel,
                                float step,
                                bool smooth);
Vec3 GizmoDrag_ApplyAxisDistance(Vec3 startWorld,
                                 GizmoAxisDirection axis,
                                 float signedWorldDistance);
