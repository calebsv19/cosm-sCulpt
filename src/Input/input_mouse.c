// src/Input/input_mouse.c
#include "input_mouse.h"
#include "Input/input_mouse_drag.h"
#include "Input/input_mouse_drag_shared.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Editor/editor.h"

#include "UI/input_ui_panel.h"
#include "UI/ui_panel.h"

#include "Layout/Grid/grid.h"
#include "Layout/hitbox_system.h"  // ← if using hitboxes

#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

typedef enum {
    POINTER_PANE_TOP = 0,
    POINTER_PANE_LEFT = 1,
    POINTER_PANE_RIGHT = 2,
    POINTER_PANE_CENTER = 3,
    POINTER_PANE_OUTSIDE = 4
} PointerPaneLane;

static SDL_Rect PaneRectToSDLRect(CorePaneRect rect) {
    SDL_Rect out = {0, 0, 0, 0};
    int x0 = (int)floorf(rect.x);
    int y0 = (int)floorf(rect.y);
    int x1 = (int)ceilf(rect.x + rect.width);
    int y1 = (int)ceilf(rect.y + rect.height);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    out.x = x0;
    out.y = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    return out;
}

static bool ResolvePaneRect(LineDrawingPaneRole role, SDL_Rect* out_rect) {
    const LineDrawingPaneHost* pane_host = NULL;
    CorePaneRect pane_rect = {0};
    if (!out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};

    pane_host = Global_GetPaneHostConst();
    if (!pane_host || !pane_host->initialized) return false;
    if (!LineDrawingPaneHost_GetRectForRole(pane_host, role, &pane_rect)) return false;

    *out_rect = PaneRectToSDLRect(pane_rect);
    return out_rect->w > 0 && out_rect->h > 0;
}

static PointerPaneLane ResolvePointerPaneLane(int x, int y) {
    SDL_Point point = { x, y };
    SDL_Rect top = {0, 0, 0, 0};
    SDL_Rect left = {0, 0, 0, 0};
    SDL_Rect right = {0, 0, 0, 0};
    SDL_Rect center = {0, 0, 0, 0};
    bool any_resolved = false;

    if (ResolvePaneRect(LINE_DRAWING_PANE_ROLE_TOP_BAR, &top)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &top)) return POINTER_PANE_TOP;
    }
    if (ResolvePaneRect(LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS, &left)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &left)) return POINTER_PANE_LEFT;
    }
    if (ResolvePaneRect(LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &right)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &right)) return POINTER_PANE_RIGHT;
    }
    if (ResolvePaneRect(LINE_DRAWING_PANE_ROLE_CENTER_CANVAS, &center)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &center)) return POINTER_PANE_CENTER;
    }

    if (!any_resolved) {
        // Legacy fallback when pane host is unavailable.
        return POINTER_PANE_CENTER;
    }
    return POINTER_PANE_OUTSIDE;
}

static void ClearHoverState(EditorState* editor) {
    if (!editor) return;
    editor->hoveredAnchorIndex = -1;
    editor->hoveredWallIndex = -1;
    editor->hoveredHandleAnchor = -1;
    editor->hoveredHandleComponent = -1;
    editor->hoveredGizmoAxis = -1;
    editor->hoveredObject3DGizmoAxis = -1;
    editor->hoveredObject3DId = 0u;
    editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
}

static bool HandleFreeViewOrbitMotion(const SDL_MouseMotionEvent* motion) {
    if (!motion) return false;
    if (draggingAnchor || draggingHandle || draggingSelectionBox || draggingGizmo ||
        draggingObjectResize || draggingObjectGizmo ||
        draggingObjectTranslate || draggingObjectRotate) return false;
    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (!state || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return false;
    if (UIPanel_IsCapturingKeyboard() || UIPanel_IsLoadMenuOpen()) return false;
    if (ResolvePointerPaneLane(motion->x, motion->y) != POINTER_PANE_CENTER) return false;

    SDL_Keymod mods = SDL_GetModState();
    if ((mods & KMOD_ALT) == 0) return false;

    // Orbit around current layout centroid while Option/Alt is held.
    bool hasAnchors = false;
    Vec3 center = Layout_ComputeCentroid(&state->layout, &hasAnchors);
    if (hasAnchors) {
        state->freeViewCamera.target = center;
        viewCtx.camera.target = center;
    }

    const float orbitSensitivity = 0.35f;
    state->freeViewCamera.yawDeg += (float)motion->xrel * orbitSensitivity;
    state->freeViewCamera.pitchDeg -= (float)motion->yrel * orbitSensitivity;
    FreeView_NormalizeOrbitAngles(&state->freeViewCamera);
    Global_FlagHitboxesDirty();
    return true;
}

static void UpdateHover(int mx, int my) {
    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;

    if (UIPanel_IsCapturingKeyboard() || UIPanel_IsLoadMenuOpen()) {
        ClearHoverState(editor);
        return;
    }
    if (ResolvePointerPaneLane(mx, my) != POINTER_PANE_CENTER) {
        ClearHoverState(editor);
        return;
    }

    Hitbox hit = HitboxSystem_GetHitAt(mx, my);
    if (hit.type == HITBOX_POINT) {
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_HANDLE) {
        editor->hoveredHandleAnchor = hit.index;
        editor->hoveredHandleComponent = hit.subIndex;
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_GIZMO_AXIS) {
        editor->hoveredGizmoAxis = hit.subIndex;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_OBJECT3D_GIZMO_AXIS) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObject3DGizmoAxis = hit.subIndex;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
    } else if (hit.type == HITBOX_WALL) {
        editor->hoveredWallIndex = hit.index;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_OBJECT3D_PRISM_HANDLE) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObject3DPrismHandle = hit.subIndex;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
    } else if (hit.type == HITBOX_OBJECT3D_PLANE_CORNER ||
               hit.type == HITBOX_OBJECT3D_PLANE_EDGE) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObject3DResizeHandle = hit.subIndex;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
    } else if (hit.type == HITBOX_OBJECT3D) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
    } else {
        ClearHoverState(editor);
    }
}


// 		Scroll to zoom in/out
// ============================================================
static void HandleMouseWheel(AppContext* ctx, SDL_MouseWheelEvent* wheel) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    int mx = 0;
    int my = 0;
    if (ctx && ctx->window) {
        SDL_GetMouseState(&mx, &my);
    } else {
        mx = Global_GetScreenWidth() / 2;
        my = Global_GetScreenHeight() / 2;
    }

    if (ResolvePointerPaneLane(mx, my) != POINTER_PANE_CENTER) return;

    float delta = (wheel->preciseY != 0.0f) ? wheel->preciseY : (float)wheel->y;
    if (wheel->direction == SDL_MOUSEWHEEL_FLIPPED) {
        delta = -delta;
    }
    if (fabsf(delta) <= 0.0001f) return;

    // Exponential zoom for smoother high-precision wheel/trackpad input.
    float factor = powf(1.08f, delta);
    Grid_zoom(grid, factor, (float)mx, (float)my);
    Global_FlagGridChanged();
}


//        Left click: select point (priority) or wall
// ============================================================
static void HandleLeftMouseDown(SDL_MouseButtonEvent* btn) {
    PointerPaneLane pane_lane = POINTER_PANE_OUTSIDE;
    if (UIPanel_IsSaveDialogActive() ||
        UIPanel_IsRootDialogActive() ||
        UIPanel_IsPrismDimensionDialogActive() ||
        UIPanel_IsSceneBoundsDialogActive() ||
        UIPanel_IsLoadMenuOpen()) {
        (void)UIPanel_HandleClick(btn->x, btn->y);
        return;
    }

    pane_lane = ResolvePointerPaneLane(btn->x, btn->y);
    if (pane_lane == POINTER_PANE_TOP) {
        return;
    }
    if (pane_lane == POINTER_PANE_LEFT || pane_lane == POINTER_PANE_RIGHT) {
        (void)UIPanel_HandleClick(btn->x, btn->y);
        return;
    }
    if (pane_lane == POINTER_PANE_OUTSIDE) {
        return;
    }

    draggingPan = false;
    draggingHandle = false;
    draggingAnchor = false;
    draggingGizmo = false;
    draggingObjectResize = false;
    draggingObjectGizmo = false;
    draggingObjectTranslate = false;
    draggingObjectRotate = false;
    draggingAnchorIndex = -1;
    anchorDragCaptured = false;
    lastMx = btn->x;
    lastMy = btn->y;
    dragPrecise = (SDL_GetModState() & KMOD_ALT) != 0;

    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    Editor_ResetGizmoDrag(editor);
    ResetObjectResizeDrag(editor);
    ResetObjectGizmoDrag(editor);
    ResetObjectTranslateDrag(editor);
    ResetObjectRotateDrag(editor);
    bool shiftSelect = (SDL_GetModState() & KMOD_SHIFT) != 0;
    bool startedGizmoDrag = false;
    bool startedObjectResize = false;
    bool startedObjectGizmoDrag = false;
    bool startedObjectTranslateDrag = false;
    bool startedObjectRotateDrag = false;

    Global_RebuildHitboxesIfDirty();
    Hitbox hit = HitboxSystem_GetHitAt(btn->x, btn->y);

    bool clickedHandle = (hit.type == HITBOX_HANDLE);
    bool clickedGizmo = (hit.type == HITBOX_GIZMO_AXIS);
    bool clickedObjectGizmo = (hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
    bool clickedPrismHandle = (hit.type == HITBOX_OBJECT3D_PRISM_HANDLE);
    bool clickedObjectResize = (hit.type == HITBOX_OBJECT3D_PLANE_CORNER ||
                                hit.type == HITBOX_OBJECT3D_PLANE_EDGE);
    bool doubleClick = (!shiftSelect && btn->clicks >= 2);

    // Priority: anchor selection overrides wall
    if (hit.type == HITBOX_POINT) {
        bool alreadySelected = Editor_IsAnchorSelected(editor, hit.index);
        if (doubleClick) {
            Editor_SelectAnchor(editor, hit.index, false);
        } else if (!alreadySelected) {
            Editor_SelectAnchor(editor, hit.index, shiftSelect);
        } else {
            editor->selectedAnchorIndex = hit.index;
        }
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        editor->selectedObject3DId = 0u;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        if (!doubleClick) {
            draggingAnchor = true;
            draggingAnchorIndex = hit.index;
            editor->isDraggingAnchor = true;
            editor->isPreciseDrag = dragPrecise;
            Editor_BeginAnchorDrag(editor, &state->layout);
        } else {
            draggingAnchor = false;
            editor->isDraggingAnchor = false;
        }
    } else if (hit.type == HITBOX_HANDLE) {
        Editor_SelectAnchor(editor, hit.index, false);
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = hit.index;
        editor->selectedHandleComponent = hit.subIndex;
        editor->selectedObject3DId = 0u;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        Editor_HistoryCapture(editor, &state->layout);
    } else if (hit.type == HITBOX_GIZMO_AXIS) {
        Editor_SelectAnchor(editor, hit.index, false);
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        editor->selectedObject3DId = 0u;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        startedGizmoDrag = BeginGizmoDragSession(state,
                                                 editor,
                                                 hit.index,
                                                 (GizmoAxisDirection)hit.subIndex,
                                                 btn->x,
                                                 btn->y);
    } else if (hit.type == HITBOX_OBJECT3D_PLANE_CORNER ||
               hit.type == HITBOX_OBJECT3D_PLANE_EDGE) {
        Editor_ClearAnchorSelection(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObject3DResizeHandle = hit.subIndex;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, (uint32_t)hit.index);
        SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
        const bool freeView = (state->spaceMode == SPACE_MODE_3D) &&
                              SpaceAdapter_IsFreeViewEnabled(&viewCtx);
        if (object && object->kind == OBJECT3D_KIND_RECT_PRISM && freeView) {
            startedObjectResize = false;
        } else {
            startedObjectResize = BeginObjectResizeDragSession(state,
                                                               editor,
                                                               (uint32_t)hit.index,
                                                               (PlaneResizeHandleKind)hit.subIndex);
        }
    } else if (hit.type == HITBOX_OBJECT3D_PRISM_HANDLE) {
        Editor_ClearAnchorSelection(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = hit.subIndex;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
    } else if (hit.type == HITBOX_OBJECT3D_GIZMO_AXIS) {
        const int selectedHandle = editor->selectedObject3DPrismHandle;
        Editor_ClearAnchorSelection(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = selectedHandle;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        if (editor->selectedObject3DPrismHandle != RECT_PRISM_RESIZE_HANDLE_NONE) {
            startedObjectGizmoDrag = BeginObjectGizmoDragSession(state,
                                                                 editor,
                                                                 (uint32_t)hit.index,
                                                                 (RectPrismResizeHandleKind)editor->selectedObject3DPrismHandle,
                                                                 (RectPrismAxisDirection)hit.subIndex,
                                                                 btn->x,
                                                                 btn->y);
        } else {
            if (editor->object3DRotateMode) {
                startedObjectRotateDrag = BeginObjectRotateDragSession(state,
                                                                       editor,
                                                                       (uint32_t)hit.index,
                                                                       (GizmoAxisDirection)hit.subIndex,
                                                                       btn->x,
                                                                       btn->y);
            } else {
                startedObjectTranslateDrag = BeginObjectTranslateDragSession(state,
                                                                             editor,
                                                                             (uint32_t)hit.index,
                                                                             (GizmoAxisDirection)hit.subIndex,
                                                                             btn->x,
                                                                             btn->y);
            }
        }
    } else if (hit.type == HITBOX_WALL) {
        editor->selectedWallIndex = hit.index;
        Editor_ClearAnchorSelection(editor);
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        editor->selectedObject3DId = 0u;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_OBJECT3D) {
        Editor_ClearAnchorSelection(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
    } else {
        if (shiftSelect) {
            editor->selectionBoxActive = true;
            editor->selectionBoxAdditive = true;
            draggingSelectionBox = true;
            editor->selectionBoxStart = ScreenToWorld(btn->x, btn->y, &state->grid);
            editor->selectionBoxEnd = editor->selectionBoxStart;
            lastMx = btn->x;
            lastMy = btn->y;
        } else {
            editor->selectedWallIndex = -1;
            Editor_ClearAnchorSelection(editor);
            editor->selectedHandleAnchor = -1;
            editor->selectedHandleComponent = -1;
            editor->selectedObject3DId = 0u;
            editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
            editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        }
    }

    if (clickedHandle) {
        draggingHandle = true;
        draggingPan = false;
    } else if (clickedGizmo) {
        draggingHandle = false;
        draggingPan = false;
        draggingGizmo = startedGizmoDrag;
    } else if (clickedObjectGizmo) {
        draggingHandle = false;
        draggingPan = false;
        draggingObjectGizmo = startedObjectGizmoDrag;
        draggingObjectTranslate = startedObjectTranslateDrag;
        draggingObjectRotate = startedObjectRotateDrag;
    } else if (clickedObjectResize) {
        draggingHandle = false;
        draggingPan = false;
        draggingObjectResize = startedObjectResize;
    } else if (clickedPrismHandle) {
        draggingHandle = false;
        draggingPan = false;
        draggingObjectResize = false;
    } else if (draggingAnchor) {
        draggingHandle = false;
        draggingPan = false;
    } else if (draggingSelectionBox) {
        draggingHandle = false;
        draggingPan = false;
        editor->selectionBoxActive = true;
    } else {
        draggingHandle = false;
        draggingPan = true;
    }

    Global_FlagHitboxesDirty();
    UpdateHover(btn->x, btn->y);
}


// 		Right click: place wall (snap to grid)
// ============================================================
static void HandleRightMouseDown(SDL_MouseButtonEvent* btn) {
    if (UIPanel_IsCapturingKeyboard() || UIPanel_IsLoadMenuOpen()) {
        return;
    }
    if (ResolvePointerPaneLane(btn->x, btn->y) != POINTER_PANE_CENTER) {
        return;
    }
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    EditorState* editor = &state->editor;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Vec3 world3 = {0};
    if (!SpaceAdapter_ScreenToWorld(btn->x, btn->y, grid, &viewCtx, true, &world3)) return;
    Editor_ClickAt(editor, world3);
}

// 		Public interface
// ============================================================
void Input_MouseHandle(AppContext *ctx, SDL_Event* event) {
    switch (event->type) {
        case SDL_MOUSEWHEEL:
            HandleMouseWheel(ctx, &event->wheel);
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT)
                HandleLeftMouseDown(&event->button);
            else if (event->button.button == SDL_BUTTON_RIGHT)
                HandleRightMouseDown(&event->button);
            break;

        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                draggingPan = false;
                draggingHandle = false;
                draggingAnchor = false;
                draggingGizmo = false;
                draggingObjectResize = false;
                draggingObjectGizmo = false;
                draggingObjectTranslate = false;
                draggingObjectRotate = false;
                draggingSelectionBox = false;
                draggingAnchorIndex = -1;
                anchorDragCaptured = false;
                GlobalState* state = Global_Get();
                if (state) {
                    state->editor.isDraggingAnchor = false;
                    state->editor.isResizingObject3D = false;
                    state->editor.isRotatingObject3D = false;
                    state->editor.isPreciseDrag = false;
                    Editor_EndAnchorDrag(&state->editor);
                    Editor_ResetGizmoDrag(&state->editor);
                    ResetObjectResizeDrag(&state->editor);
                    ResetObjectGizmoDrag(&state->editor);
                    ResetObjectTranslateDrag(&state->editor);
                    ResetObjectRotateDrag(&state->editor);
                    if (state->editor.selectionBoxActive) {
                        Vec2 start = state->editor.selectionBoxStart;
                        Vec2 end = state->editor.selectionBoxEnd;
                        Vec2 min = { fminf(start.x, end.x), fminf(start.y, end.y) };
                        Vec2 max = { fmaxf(start.x, end.x), fmaxf(start.y, end.y) };
                        Editor_SelectAnchorsInBox(&state->editor,
                                                  &state->layout,
                                                  min,
                                                  max,
                                                  state->editor.selectionBoxAdditive);
                        state->editor.selectionBoxActive = false;
                        state->editor.selectionBoxAdditive = false;
                    }
                }
            }
            break;

        case SDL_MOUSEMOTION:
            if (HandleFreeViewOrbitMotion(&event->motion)) {
                UpdateHover(event->motion.x, event->motion.y);
                break;
            }
            HandleMouseDrag(&event->motion);
            UpdateHover(event->motion.x, event->motion.y);
            break;
    }
}
