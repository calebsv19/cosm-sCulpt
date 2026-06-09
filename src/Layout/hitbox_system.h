#pragma once
#include "Layout/layout.h"
#include "Core/SDLApp/sdl_app_framework.h"
#include "ObjectAuthoring/object_authoring_document.h"
#include <stdbool.h>

typedef enum {
    HITBOX_NONE,
    HITBOX_SCENE_BOUNDS_GIZMO_AXIS,
    HITBOX_GIZMO_AXIS,
    HITBOX_OBJECT3D_GIZMO_AXIS,
    HITBOX_OBJECT_FACE_SKETCH_HANDLE,
    HITBOX_OBJECT_FACE_SKETCH_BODY,
    HITBOX_OBJECT3D_PRISM_HANDLE,
    HITBOX_OBJECT3D_PLANE_CORNER,
    HITBOX_OBJECT3D_PLANE_EDGE,
    HITBOX_OBJECT_TOPOLOGY_VERTEX,
    HITBOX_OBJECT_TOPOLOGY_EDGE,
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

typedef struct {
    bool visible;
    uint32_t bodyId;
    PlaneFrame3 frame;
    Vec2 minUV;
    Vec2 maxUV;
} ObjectFaceSketchHitboxState;

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
                         const ObjectAuthoringDocument* objectTopology,
                         bool objectTopologyEditMode,
                         const ObjectFaceSketchHitboxState* objectFaceSketch,
                         int selectedSceneBoundsHandle,
                         bool sceneBoundsHandlesVisible,
                         bool gizmoEnabled);

// Returns hitbox under screen-space mouse position
Hitbox HitboxSystem_GetHitAt(int mouseX, int mouseY);
Hitbox HitboxSystem_GetHitAtOfType(int mouseX, int mouseY, HitboxType type);
