// src/Input/input_mouse.c
#include "input_mouse.h"
#include "Core/global_state.h"
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
static bool draggingSelectionBox = false;
static int draggingAnchorIndex = -1;
static bool anchorDragCaptured = false;
static bool dragPrecise = false;
static int lastMx = 0, lastMy = 0;

static void UpdateHover(int mx, int my) {
    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;

    if (UIPanel_IsSaveDialogActive() || UIPanel_IsLoadMenuOpen()) {
        editor->hoveredAnchorIndex = -1;
        editor->hoveredWallIndex = -1;
        return;
    }

    Hitbox hit = HitboxSystem_GetHitAt(mx, my);
    if (hit.type == HITBOX_POINT) {
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
    } else if (hit.type == HITBOX_HANDLE) {
        editor->hoveredHandleAnchor = hit.index;
        editor->hoveredHandleComponent = hit.subIndex;
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
    } else if (hit.type == HITBOX_WALL) {
        editor->hoveredWallIndex = hit.index;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
    } else {
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
    }
}


// 		Scroll to zoom in/out
// ============================================================
static void HandleMouseWheel(AppContext* ctx, SDL_MouseWheelEvent* wheel) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;

    int w, h;
    SDL_GetRendererOutputSize(ctx->renderer, &w, &h);

    float factor = (wheel->y > 0) ? 1.1f : 0.9f;
    Grid_zoom(grid, factor, w / 2.0f, h / 2.0f);
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
    draggingAnchorIndex = -1;
    anchorDragCaptured = false;
    lastMx = btn->x;
    lastMy = btn->y;
    dragPrecise = (SDL_GetModState() & KMOD_ALT) != 0;

    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    bool shiftSelect = (SDL_GetModState() & KMOD_SHIFT) != 0;

    Global_RebuildHitboxesIfDirty();
    Hitbox hit = HitboxSystem_GetHitAt(btn->x, btn->y);

    bool clickedHandle = (hit.type == HITBOX_HANDLE);
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

    Vec2 world = ScreenToSnappedWorld(btn->x, btn->y, grid);

    Editor_ClickAt(editor, world);
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
    Vec2 snapped = ScreenToSnappedWorld(mx, my, &state->grid);
    Vec2 world = ScreenToWorld(mx, my, &state->grid);
    Vec2 primaryPos = precise ? world : snapped;

    Editor_UpdateAnchorDrag(&state->editor, &state->layout, primaryPos);
}

static void UpdateHandleDragPosition(int mx, int my) {
    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    if (editor->selectedHandleAnchor < 0 || editor->selectedHandleComponent < 0) return;
    if ((size_t)editor->selectedHandleAnchor >= state->layout.anchorCount) return;

    Anchor* anchor = &state->layout.anchors[editor->selectedHandleAnchor];
    if (anchor->isDeleted || anchor->type != ANCHOR_TYPE_CURVE) return;

    Vec2 world = ScreenToWorld(mx, my, &state->grid);
    Vec2 delta = Vec2_Sub(world, anchor->pos);
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
    float angle = RadToDeg(atan2f(delta.y, delta.x));

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


// 		Handle mouse dragging for panning or handle editing
// ============================================================
static void HandleMouseDrag(SDL_MouseMotionEvent* motion) {
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
                draggingSelectionBox = false;
                draggingAnchorIndex = -1;
                anchorDragCaptured = false;
                GlobalState* state = Global_Get();
                if (state) {
                    state->editor.isDraggingAnchor = false;
                    state->editor.isPreciseDrag = false;
                    Editor_EndAnchorDrag(&state->editor);
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
            HandleMouseDrag(&event->motion);
            UpdateHover(event->motion.x, event->motion.y);
            break;
    }
}
