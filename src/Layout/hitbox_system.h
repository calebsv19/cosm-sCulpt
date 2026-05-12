#pragma once
#include "Layout/layout.h"
#include "Core/SDLApp/sdl_app_framework.h"
#include <stdbool.h>

typedef enum {
    HITBOX_NONE,
    HITBOX_SCENE_BOUNDS_GIZMO_AXIS,
    HITBOX_GIZMO_AXIS,
    HITBOX_OBJECT3D_GIZMO_AXIS,
    HITBOX_OBJECT3D_PRISM_HANDLE,
    HITBOX_OBJECT3D_PLANE_CORNER,
    HITBOX_OBJECT3D_PLANE_EDGE,
    HITBOX_SCENE_BOUNDS_HANDLE,
    HITBOX_WALL,
    HITBOX_POINT,
    HITBOX_HANDLE,
    HITBOX_OBJECT3D
} HitboxType;

typedef struct {
    HitboxType type;
    int index;      // Wall/point/anchor index (type-dependent)
    int subIndex;   // Handles: 0=in,1=out. Gizmos: axis direction enum. Otherwise -1
    SDL_Rect bounds;
    float depthDistance;
} Hitbox;

// Call once per frame after layout update
void HitboxSystem_Rebuild(const Layout* layout,
                         float scale,
                         float offsetX,
                         float offsetY,
                         ViewPlane plane,
                         const FreeViewCamera* camera,
                         int selectedAnchorIndex,
                         uint32_t selectedObject3DId,
                         int selectedObject3DResizeHandle,
                         int selectedObject3DPrismHandle,
                         int selectedSceneBoundsHandle,
                         bool sceneBoundsHandlesVisible,
                         bool gizmoEnabled);

// Returns hitbox under screen-space mouse position
Hitbox HitboxSystem_GetHitAt(int mouseX, int mouseY);
