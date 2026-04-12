#pragma once

#include "Core/global_state.h"

typedef struct {
    bool active;
    uint32_t objectId;
    PlaneResizeHandleKind handle;
    bool historyCaptured;
} ObjectResizeDragState;

typedef struct {
    bool active;
    uint32_t objectId;
    RectPrismResizeHandleKind handle;
    RectPrismAxisDirection axisDirection;
    Vec2 mouseStartScreen;
    Vec3 handleStartWorld;
    float worldUnitsPerPixel;
    bool historyCaptured;
} ObjectGizmoDragState;

typedef struct {
    bool active;
    uint32_t objectId;
    GizmoAxisDirection axis;
    Vec2 mouseStartScreen;
    Vec3 centerStartWorld;
    float worldUnitsPerPixel;
    bool smooth;
    bool historyCaptured;
} ObjectTranslateDragState;

typedef struct {
    bool active;
    uint32_t objectId;
    GizmoAxisDirection axis;
    Vec2 mouseStartScreen;
    Vec3 centerStartWorld;
    float degreesPerPixel;
    bool smooth;
    bool historyCaptured;
    Object3D baselineObject;
} ObjectRotateDragState;

extern bool draggingPan;
extern bool draggingHandle;
extern bool draggingAnchor;
extern bool draggingGizmo;
extern bool draggingObjectResize;
extern bool draggingObjectGizmo;
extern bool draggingObjectTranslate;
extern bool draggingObjectRotate;
extern bool draggingSelectionBox;
extern int draggingAnchorIndex;
extern bool anchorDragCaptured;
extern bool dragPrecise;
extern int lastMx;
extern int lastMy;

extern ObjectResizeDragState objectResizeDrag;
extern ObjectGizmoDragState objectGizmoDrag;
extern ObjectTranslateDragState objectTranslateDrag;
extern ObjectRotateDragState objectRotateDrag;
