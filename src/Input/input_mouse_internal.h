#pragma once

#include "Core/global_state.h"
#include "Layout/hitbox_system.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    POINTER_PANE_TOP = 0,
    POINTER_PANE_LEFT = 1,
    POINTER_PANE_RIGHT = 2,
    POINTER_PANE_CENTER = 3,
    POINTER_PANE_OUTSIDE = 4
} PointerPaneLane;

LineDrawingPaneHost* ResolvePaneHostMutable(void);
PointerPaneLane ResolvePointerPaneLane(int x, int y);
void ClearHoverState(EditorState* editor);
bool InputMouse_ObjectModeEnabled(void);
bool InputMouse_ObjectEditTopologyModeActive(const GlobalState* state);
bool InputMouse_IsObjectFaceAuthoringModal(const EditorState* editor);
bool InputMouse_IsObjectFaceSketchDrawActive(const EditorState* editor);
void ClearObjectAuthoringSelection(EditorState* editor);
void SyncObjectFaceSketchTarget(EditorState* editor);
Object3DFaceKind ResolveObjectAuthoringFaceAtPointer(const GlobalState* state,
                                                     const Object3D* object,
                                                     int mouse_x,
                                                     int mouse_y);
Hitbox ResolveViewportObjectBodyHit(const GlobalState* state, int mx, int my, Hitbox base_hit);
Object3DFaceKind ResolveObjectAuthoringFaceForSelection(const GlobalState* state,
                                                        const Object3D* object,
                                                        int mouse_x,
                                                        int mouse_y);
void UpdateHover(int mx, int my);
