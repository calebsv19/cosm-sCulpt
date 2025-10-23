// src/Input/input_keyboard.c
#include "input_keyboard.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/layout.h"
#include "Layout/layout_origin.h"
#include "Layout/Grid/grid.h"
#include <SDL2/SDL.h>

// 		Continuous movement (arrow keys, zoom, quit)
// ============================================================
static void HandleHeldKeys(AppContext* ctx) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float panSpeed = 300.0f * ctx->deltaTime;

    int w, h;
    SDL_GetRendererOutputSize(ctx->renderer, &w, &h);

    bool gridChanged = false;

    if (keys[SDL_SCANCODE_LEFT])   { Grid_pan(grid,  panSpeed,  0); gridChanged = true; }
    if (keys[SDL_SCANCODE_RIGHT])  { Grid_pan(grid, -panSpeed,  0); gridChanged = true; }
    if (keys[SDL_SCANCODE_UP])     { Grid_pan(grid, 0,   panSpeed); gridChanged = true; }
    if (keys[SDL_SCANCODE_DOWN])   { Grid_pan(grid, 0,  -panSpeed); gridChanged = true; }

    if (keys[SDL_SCANCODE_EQUALS]) { Grid_zoom(grid, 1.05f, w / 2.0f, h / 2.0f); gridChanged = true; }
    if (keys[SDL_SCANCODE_MINUS])  { Grid_zoom(grid, 0.95f, w / 2.0f, h / 2.0f); gridChanged = true; }

    if (gridChanged) {
        Global_FlagGridChanged();
    }

    if (keys[SDL_SCANCODE_ESCAPE]) ctx->quit = true;
}


// 		Public keyboard input dispatcher
// ============================================================
void Input_KeyboardHandle(AppContext* ctx, SDL_Event* event) {
    GlobalState* state = Global_Get();

    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        SDL_Keymod mods = SDL_GetModState();
        bool primaryModifier = (mods & (KMOD_CTRL | KMOD_GUI)) != 0;

        if (event->type == SDL_KEYDOWN && primaryModifier) {
            if (event->key.keysym.sym == SDLK_z) {
                if (mods & KMOD_SHIFT) {
                    if (Editor_Redo(&state->editor, &state->layout)) {
                        return;
                    }
                } else {
                    if (Editor_Undo(&state->editor, &state->layout)) {
                        return;
                    }
                }
            } else if (event->key.keysym.sym == SDLK_y) {
                if (Editor_Redo(&state->editor, &state->layout)) {
                    return;
                }
            }
        }

        HandleHeldKeys(ctx);

        // Shift key held for wall snapping
        if (event->key.keysym.sym == SDLK_LSHIFT || event->key.keysym.sym == SDLK_RSHIFT) {
            bool held = (event->type == SDL_KEYDOWN);
            Editor_SetShiftHeld(&state->editor, held);
        }

        // Toggle delete mode (D)
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_d) {
            if (state->editor.deleteMode == DELETE_MODE_SAFE) {
                state->editor.deleteMode = DELETE_MODE_AUTO_PRUNE;
                printf("[Editor] Delete mode: AUTO_PRUNE\n");
            } else {
                state->editor.deleteMode = DELETE_MODE_SAFE;
                printf("[Editor] Delete mode: SAFE\n");
            }
        }


	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_o) {
	    int i = state->editor.selectedAnchorIndex;
	    if (i >= 0 && i < (int)state->layout.anchorCount) {
            Editor_HistoryCapture(&state->editor, &state->layout);
	        Layout_ShiftOriginToAnchor(
	            &state->layout,
	            &state->grid,
	            i,
	            state->screenWidth,
	            state->screenHeight
	        );
	        printf("[Editor] Origin shifted to anchor %d\n", i);
	    } else {
	        printf("[Editor] No anchor selected to shift origin.\n");
	    }
	}

	// Toggle pinning of selected anchor
	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_p) {
	    int i = state->editor.selectedAnchorIndex;
	    if (i >= 0 && i < (int)state->layout.anchorCount) {
	        Editor_HistoryCapture(&state->editor, &state->layout);
	        Anchor* a = &state->layout.anchors[i];
	        a->isPersistent = !a->isPersistent;
	        printf("[Editor] Anchor %d pin state: %s\n", i, a->isPersistent ? "PINNED" : "UNPINNED");
            Global_FlagLayoutChanged();
	    }
	}

        // Delete wall or anchor
        if (event->type == SDL_KEYDOWN &&
           (event->key.keysym.sym == SDLK_DELETE || event->key.keysym.sym == SDLK_BACKSPACE)) {
            bool hasWall = state->editor.selectedWallIndex >= 0;
            bool hasAnchor = state->editor.selectedAnchorIndex >= 0;
            if (hasWall || hasAnchor) {
                Editor_HistoryCapture(&state->editor, &state->layout);
            }

            if (hasWall) {
                Layout_RemoveWall(&state->layout, state->editor.selectedWallIndex);
                state->editor.selectedWallIndex = -1;
            }
            if (hasAnchor) {
                Layout_RemoveAnchor(&state->layout, state->editor.selectedAnchorIndex);
                state->editor.selectedAnchorIndex = -1;
            }
        }
    }
}
