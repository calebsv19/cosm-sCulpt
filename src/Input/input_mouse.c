// src/Input/input_mouse.c
#include "input_mouse.h"
#include "Core/global_state.h"
#include "Editor/editor.h"

#include "UI/input_ui_panel.h"

#include "Layout/Grid/grid.h"
#include "Layout/hitbox_system.h"  // ← if using hitboxes

#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

static bool dragging = false;
static int lastMx = 0, lastMy = 0;



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
    dragging = true;
    lastMx = btn->x;
    lastMy = btn->y;

    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;

    Global_RebuildHitboxesIfDirty();
    Hitbox hit = HitboxSystem_GetHitAt(btn->x, btn->y);

    // Priority: anchor selection overrides wall
    if (hit.type == HITBOX_POINT) {
        editor->selectedAnchorIndex = hit.index;
        editor->selectedWallIndex = -1;
    } else if (hit.type == HITBOX_WALL) {
        editor->selectedWallIndex = hit.index;
        editor->selectedAnchorIndex = -1;
    } else {
        editor->selectedWallIndex = -1;
        editor->selectedAnchorIndex = -1;
    }
}


// 		Right click: place wall (snap to grid)
// ============================================================
static void HandleRightMouseDown(SDL_MouseButtonEvent* btn) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    EditorState* editor = &state->editor;

    Vec2 world = ScreenToSnappedWorld(btn->x, btn->y, grid);

    Editor_ClickAt(editor, world);
}


// 		Handle mouse dragging for panning
// ============================================================
static void HandleMouseDrag(SDL_MouseMotionEvent* motion) {
    if (!dragging) return;

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
            if (event->button.button == SDL_BUTTON_LEFT)
                dragging = false;
            break;

        case SDL_MOUSEMOTION:
            HandleMouseDrag(&event->motion);
            break;
    }
}
