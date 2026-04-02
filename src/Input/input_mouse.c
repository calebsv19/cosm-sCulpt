// src/Input/input_mouse.c
#include "input_mouse.h"
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

static bool draggingPan = false;
static bool draggingHandle = false;
static bool draggingAnchor = false;
static bool draggingGizmo = false;
static bool draggingSelectionBox = false;
static int draggingAnchorIndex = -1;
static bool anchorDragCaptured = false;
static bool dragPrecise = false;
static int lastMx = 0, lastMy = 0;

static bool HandleFreeViewOrbitMotion(const SDL_MouseMotionEvent* motion) {
    if (!motion) return false;
    if (draggingAnchor || draggingHandle || draggingSelectionBox || draggingGizmo) return false;
    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (!state || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return false;
    if (UIPanel_IsSaveDialogActive() || UIPanel_IsLoadMenuOpen()) return false;

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

static bool BeginGizmoDragSession(GlobalState* state,
                                  EditorState* editor,
                                  int anchorIndex,
                                  GizmoAxisDirection axis,
                                  int mouseX,
                                  int mouseY) {
    if (!state || !editor) return false;
    if (!GizmoAxisDirection_IsValid(axis)) return false;
    if (anchorIndex < 0 || (size_t)anchorIndex >= state->layout.anchorCount) return false;

    const Anchor* anchor = &state->layout.anchors[anchorIndex];
    if (anchor->isDeleted) return false;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return false;

    const float axisWorldLen = fmaxf(state->grid.gridSize, 1e-4f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(axis);
    Vec3 tipWorld = Vec3_Add(anchor->pos, Vec3_Scale(axisWorldVec, axisWorldLen));

    Vec2 anchorScreen = WorldToScreen(SpaceAdapter_ProjectToView(anchor->pos, &viewCtx), &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    float axisPixels = Vec2_Distance(anchorScreen, tipScreen);
    if (axisPixels <= 1e-4f) return false;

    editor->gizmoDrag.active = true;
    editor->gizmoDrag.axis = axis;
    editor->gizmoDrag.anchorIndex = anchorIndex;
    editor->gizmoDrag.mouseStartScreen = (Vec2){ (float)mouseX, (float)mouseY };
    editor->gizmoDrag.primaryStartWorld = anchor->pos;
    editor->gizmoDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    editor->gizmoDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    editor->hoveredGizmoAxis = axis;
    editor->isDraggingAnchor = true;
    editor->isPreciseDrag = editor->gizmoDrag.smooth;

    Editor_BeginAnchorDrag(editor, &state->layout);
    if (editor->dragSnapshotCount == 0) {
        Editor_ResetGizmoDrag(editor);
        editor->isDraggingAnchor = false;
        editor->isPreciseDrag = false;
        return false;
    }

    Editor_HistoryCapture(editor, &state->layout);
    return true;
}

static void UpdateHover(int mx, int my) {
    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;

    if (UIPanel_IsSaveDialogActive() || UIPanel_IsLoadMenuOpen()) {
        editor->hoveredAnchorIndex = -1;
        editor->hoveredWallIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        return;
    }

    Hitbox hit = HitboxSystem_GetHitAt(mx, my);
    if (hit.type == HITBOX_POINT) {
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
    } else if (hit.type == HITBOX_HANDLE) {
        editor->hoveredHandleAnchor = hit.index;
        editor->hoveredHandleComponent = hit.subIndex;
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredGizmoAxis = -1;
    } else if (hit.type == HITBOX_GIZMO_AXIS) {
        editor->hoveredGizmoAxis = hit.subIndex;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
    } else if (hit.type == HITBOX_WALL) {
        editor->hoveredWallIndex = hit.index;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
    } else {
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
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
    if (UIPanel_HandleClick(btn->x, btn->y)) {
        return;  // UI consumed the click — skip layout/editor input
    }
    draggingPan = false;
    draggingHandle = false;
    draggingAnchor = false;
    draggingGizmo = false;
    draggingAnchorIndex = -1;
    anchorDragCaptured = false;
    lastMx = btn->x;
    lastMy = btn->y;
    dragPrecise = (SDL_GetModState() & KMOD_ALT) != 0;

    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    Editor_ResetGizmoDrag(editor);
    bool shiftSelect = (SDL_GetModState() & KMOD_SHIFT) != 0;
    bool startedGizmoDrag = false;

    Global_RebuildHitboxesIfDirty();
    Hitbox hit = HitboxSystem_GetHitAt(btn->x, btn->y);

    bool clickedHandle = (hit.type == HITBOX_HANDLE);
    bool clickedGizmo = (hit.type == HITBOX_GIZMO_AXIS);
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
        Editor_HistoryCapture(editor, &state->layout);
    } else if (hit.type == HITBOX_GIZMO_AXIS) {
        Editor_SelectAnchor(editor, hit.index, false);
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        startedGizmoDrag = BeginGizmoDragSession(state,
                                                 editor,
                                                 hit.index,
                                                 (GizmoAxisDirection)hit.subIndex,
                                                 btn->x,
                                                 btn->y);
    } else if (hit.type == HITBOX_WALL) {
        editor->selectedWallIndex = hit.index;
        Editor_ClearAnchorSelection(editor);
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
        }
    }

    if (clickedHandle) {
        draggingHandle = true;
        draggingPan = false;
    } else if (clickedGizmo) {
        draggingHandle = false;
        draggingPan = false;
        draggingGizmo = startedGizmoDrag;
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

    UpdateHover(btn->x, btn->y);
}


// 		Right click: place wall (snap to grid)
// ============================================================
static void HandleRightMouseDown(SDL_MouseButtonEvent* btn) {
    if (UIPanel_IsSaveDialogActive() || UIPanel_IsLoadMenuOpen()) {
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

static void UpdateAnchorDragPosition(int mx, int my) {
    if (!draggingAnchor || draggingAnchorIndex < 0) return;

    GlobalState* state = Global_Get();
    if (!state) return;

    if ((size_t)draggingAnchorIndex >= state->layout.anchorCount) return;

    if (!anchorDragCaptured) {
        Editor_HistoryCapture(&state->editor, &state->layout);
        anchorDragCaptured = true;
    }

    bool precise = (SDL_GetModState() & KMOD_ALT) != 0;
    dragPrecise = precise;
    state->editor.isPreciseDrag = precise;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Vec3 primaryPos = {0};
    if (!SpaceAdapter_ScreenToWorld(mx, my, &state->grid, &viewCtx, !precise, &primaryPos)) return;

    Editor_UpdateAnchorDrag(&state->editor, &state->layout, primaryPos);
}

static void UpdateHandleDragPosition(int mx, int my) {
    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    if (editor->selectedHandleAnchor < 0 || editor->selectedHandleComponent < 0) return;
    if ((size_t)editor->selectedHandleAnchor >= state->layout.anchorCount) return;

    Anchor* anchor = &state->layout.anchors[editor->selectedHandleAnchor];
    if (anchor->isDeleted || anchor->type != ANCHOR_TYPE_CURVE) return;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Vec3 world3 = {0};
    if (!SpaceAdapter_ScreenToWorld(mx, my, &state->grid, &viewCtx, false, &world3)) return;
    Vec3 deltaWorld = Vec3_Sub(world3, anchor->pos);
    float length = 0.0f;
    float angle = 0.0f;
    anchor->handleAxis = SpaceAdapter_ActivePlaneAxis(&viewCtx);
    Vec3_HandlePolarFromWorldDelta(deltaWorld, anchor->handleAxis, &length, &angle);

    if (editor->selectedHandleComponent == 0) {
        anchor->handleInLength = length;
        anchor->handleInAngleDeg = angle;
        if (anchor->handlesLinked) {
            anchor->handleOutLength = length;
            anchor->handleOutAngleDeg = Angle_NormalizeDeg(angle + 180.0f);
        }
    } else {
        anchor->handleOutLength = length;
        anchor->handleOutAngleDeg = angle;
        if (anchor->handlesLinked) {
            anchor->handleInLength = length;
            anchor->handleInAngleDeg = Angle_NormalizeDeg(angle - 180.0f);
        }
    }

    Global_FlagLayoutChanged();
}

static void UpdateGizmoDragPosition(int mx, int my) {
    if (!draggingGizmo) return;

    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;
    if (!editor->gizmoDrag.active) return;
    if (!GizmoAxisDirection_IsValid(editor->gizmoDrag.axis)) return;
    if (editor->gizmoDrag.anchorIndex < 0 ||
        (size_t)editor->gizmoDrag.anchorIndex >= state->layout.anchorCount) {
        return;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return;

    const float axisWorldLen = fmaxf(state->grid.gridSize, 1e-4f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(editor->gizmoDrag.axis);
    Vec3 tipWorld = Vec3_Add(editor->gizmoDrag.primaryStartWorld, Vec3_Scale(axisWorldVec, axisWorldLen));

    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(editor->gizmoDrag.primaryStartWorld, &viewCtx),
                                     &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    Vec2 axisScreenVector = Vec2_Sub(tipScreen, startScreen);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels > 1e-4f) {
        editor->gizmoDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    }

    Vec2 mouseNow = { (float)mx, (float)my };
    float signedPixels = GizmoDrag_SignedPixelsAlongAxis(editor->gizmoDrag.mouseStartScreen,
                                                         mouseNow,
                                                         axisScreenVector);
    editor->gizmoDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    editor->isPreciseDrag = editor->gizmoDrag.smooth;
    const float step = fmaxf(state->grid.gridSize, 1e-4f);
    float signedWorldDistance = GizmoDrag_ResolveDistance(signedPixels,
                                                          editor->gizmoDrag.worldUnitsPerPixel,
                                                          step,
                                                          editor->gizmoDrag.smooth);
    Vec3 primaryNewPos = GizmoDrag_ApplyAxisDistance(editor->gizmoDrag.primaryStartWorld,
                                                     editor->gizmoDrag.axis,
                                                     signedWorldDistance);
    Editor_UpdateAnchorDrag(editor, &state->layout, primaryNewPos);
}


// 		Handle mouse dragging for panning or handle editing
// ============================================================
static void HandleMouseDrag(SDL_MouseMotionEvent* motion) {
    if (draggingGizmo) {
        UpdateGizmoDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingAnchor) {
        UpdateAnchorDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingHandle) {
        UpdateHandleDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingSelectionBox) {
        GlobalState* state = Global_Get();
        EditorState* editor = &state->editor;
        editor->selectionBoxEnd = ScreenToWorld(motion->x, motion->y, &state->grid);
        return;
    }
    if (!draggingPan) return;

    int dx = motion->x - lastMx;
    int dy = motion->y - lastMy;

    GlobalState* state = Global_Get();
    Grid_pan(&state->grid, -dx, -dy);
    Global_FlagGridChanged();

    lastMx = motion->x;
    lastMy = motion->y;
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
                draggingSelectionBox = false;
                draggingAnchorIndex = -1;
                anchorDragCaptured = false;
                GlobalState* state = Global_Get();
                if (state) {
                    state->editor.isDraggingAnchor = false;
                    state->editor.isPreciseDrag = false;
                    Editor_EndAnchorDrag(&state->editor);
                    Editor_ResetGizmoDrag(&state->editor);
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
