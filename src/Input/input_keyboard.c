// src/Input/input_keyboard.c
#include "input_keyboard.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/layout.h"
#include "Layout/layout_origin.h"
#include "Layout/Grid/grid.h"
#include "UI/ui_panel.h"
#include "UI/shared_theme_font_adapter.h"
#include <SDL2/SDL.h>

// 		Continuous movement (arrow keys, zoom, quit)
// ============================================================
static void HandleHeldKeys(AppContext* ctx) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float panSpeed = 300.0f * ctx->deltaTime;

    int w = Global_GetScreenWidth();
    int h = Global_GetScreenHeight();

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

static bool Input_Is3DMode(const GlobalState* state) {
    return state && state->spaceMode == SPACE_MODE_3D;
}

// 		Public keyboard input dispatcher
// ============================================================
void Input_KeyboardHandle(AppContext* ctx, SDL_Event* event) {
    if (UIPanel_HandleKeyEvent(event)) {
        return;
    }

    if (UIPanel_IsCapturingKeyboard()) {
        return;
    }

    GlobalState* state = Global_Get();

    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        SDL_Keymod mods = SDL_GetModState();
        bool primaryModifier = (mods & (KMOD_CTRL | KMOD_GUI)) != 0;

        if (event->type == SDL_KEYDOWN && primaryModifier) {
            if ((mods & KMOD_SHIFT) && event->key.keysym.sym == SDLK_t) {
                line_drawing3d_shared_theme_cycle_next();
                line_drawing3d_shared_theme_save_persisted();
                return;
            } else if ((mods & KMOD_SHIFT) && event->key.keysym.sym == SDLK_y) {
                line_drawing3d_shared_theme_cycle_prev();
                line_drawing3d_shared_theme_save_persisted();
                return;
            } else if (event->key.keysym.sym == SDLK_z) {
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

        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_m) {
            if (Global_ToggleSpaceMode(true)) {
                printf("[Editor] Space mode: %s\n", Global_GetSpaceModeLabel(state->spaceMode));
            }
            return;
        }

        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_v) {
            if (!Input_Is3DMode(state)) {
                printf("[Editor] View toggle requires SPACE_MODE_3D.\n");
                return;
            }
            state->freeViewCamera.enabled = !state->freeViewCamera.enabled;
            if (state->freeViewCamera.enabled) {
                bool hasAnchors = false;
                Vec3 center = Layout_ComputeCentroid(&state->layout, &hasAnchors);
                if (hasAnchors) {
                    state->freeViewCamera.target = center;
                }
            }
            Global_FlagHitboxesDirty();
            printf("[Editor] View mode: %s\n", state->freeViewCamera.enabled ? "FREE_VIEW" : "PLANE_VIEW");
        }

        if (event->type == SDL_KEYDOWN && Input_Is3DMode(state) && state->freeViewCamera.enabled) {
            bool consumedCameraControl = false;
            float angleStep = 6.0f;
            float moveStep = state->grid.gridSize;
            if (event->key.keysym.sym == SDLK_q) {
                state->freeViewCamera.yawDeg -= angleStep;
                consumedCameraControl = true;
            }
            if (event->key.keysym.sym == SDLK_e) {
                state->freeViewCamera.yawDeg += angleStep;
                consumedCameraControl = true;
            }
            if (event->key.keysym.sym == SDLK_t) {
                state->freeViewCamera.pitchDeg += angleStep;
                consumedCameraControl = true;
            }
            if (event->key.keysym.sym == SDLK_g) {
                state->freeViewCamera.pitchDeg -= angleStep;
                consumedCameraControl = true;
            }
            FreeView_NormalizeOrbitAngles(&state->freeViewCamera);

            Vec3 right = FreeView_Right(&state->freeViewCamera);
            Vec3 up = FreeView_Up(&state->freeViewCamera);
            if (event->key.keysym.sym == SDLK_j) {
                state->freeViewCamera.target = Vec3_Sub(state->freeViewCamera.target, Vec3_Scale(right, moveStep));
                consumedCameraControl = true;
            }
            if (event->key.keysym.sym == SDLK_l) {
                state->freeViewCamera.target = Vec3_Add(state->freeViewCamera.target, Vec3_Scale(right, moveStep));
                consumedCameraControl = true;
            }
            if (event->key.keysym.sym == SDLK_i) {
                state->freeViewCamera.target = Vec3_Add(state->freeViewCamera.target, Vec3_Scale(up, moveStep));
                consumedCameraControl = true;
            }
            if (event->key.keysym.sym == SDLK_k) {
                state->freeViewCamera.target = Vec3_Sub(state->freeViewCamera.target, Vec3_Scale(up, moveStep));
                consumedCameraControl = true;
            }
            if (consumedCameraControl) {
                Global_FlagHitboxesDirty();
                return;
            }
        }

        if (event->type == SDL_KEYDOWN &&
            (event->key.keysym.sym == SDLK_1 ||
             event->key.keysym.sym == SDLK_2 ||
             event->key.keysym.sym == SDLK_3 ||
             event->key.keysym.sym == SDLK_LEFTBRACKET ||
             event->key.keysym.sym == SDLK_RIGHTBRACKET) &&
            !Input_Is3DMode(state)) {
            printf("[Editor] Plane controls require SPACE_MODE_3D.\n");
            return;
        }

        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_1) {
            state->activePlane.axis = VIEW_PLANE_XY;
            Global_FlagHitboxesDirty();
            printf("[Editor] Active plane: XY (z = %.2f)\n", state->activePlane.offset);
        }
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_2) {
            state->activePlane.axis = VIEW_PLANE_YZ;
            Global_FlagHitboxesDirty();
            printf("[Editor] Active plane: YZ (x = %.2f)\n", state->activePlane.offset);
        }
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_3) {
            state->activePlane.axis = VIEW_PLANE_XZ;
            Global_FlagHitboxesDirty();
            printf("[Editor] Active plane: XZ (y = %.2f)\n", state->activePlane.offset);
        }
        if (event->type == SDL_KEYDOWN &&
            (event->key.keysym.sym == SDLK_LEFTBRACKET || event->key.keysym.sym == SDLK_RIGHTBRACKET)) {
            float step = state->grid.gridSize;
            if (mods & KMOD_SHIFT) step *= 10.0f;
            if (event->key.keysym.sym == SDLK_LEFTBRACKET) step = -step;
            state->activePlane.offset += step;
            Global_FlagHitboxesDirty();
            switch (state->activePlane.axis) {
                case VIEW_PLANE_XY:
                    printf("[Editor] Active plane offset: z = %.2f\n", state->activePlane.offset);
                    break;
                case VIEW_PLANE_YZ:
                    printf("[Editor] Active plane offset: x = %.2f\n", state->activePlane.offset);
                    break;
                case VIEW_PLANE_XZ:
                    printf("[Editor] Active plane offset: y = %.2f\n", state->activePlane.offset);
                    break;
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

        if (event->type == SDL_KEYDOWN &&
            event->key.keysym.sym == SDLK_c &&
            (mods & KMOD_SHIFT)) {
            if (Global_ToggleCenterCrosshair()) {
                printf("[Editor] View center crosshair: %s\n",
                       Global_IsCenterCrosshairEnabled() ? "ON" : "OFF");
            }
            return;
        }

        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_c) {
            int sel = state->editor.selectedAnchorIndex;
            if (sel >= 0 && sel < (int)state->layout.anchorCount) {
                Anchor* anchor = &state->layout.anchors[sel];
                AnchorType target = (anchor->type == ANCHOR_TYPE_CURVE)
                    ? ANCHOR_TYPE_CORNER
                    : ANCHOR_TYPE_CURVE;
                if (target == ANCHOR_TYPE_CURVE &&
                    !Layout_CanAnchorBecomeCurve(&state->layout, sel)) {
                    printf("[Editor] Anchor %d needs exactly 2 connections to become curved.\n", sel);
                } else {
                    Editor_HistoryCapture(&state->editor, &state->layout);
                    if (Layout_SetAnchorType(&state->layout, sel, target)) {
                        printf("[Editor] Anchor %d type: %s\n",
                               sel,
                               target == ANCHOR_TYPE_CURVE ? "CURVE" : "CORNER");
                    }
                }
            }
        }

        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_l) {
            int sel = state->editor.selectedAnchorIndex;
            if (sel >= 0 && sel < (int)state->layout.anchorCount) {
                Anchor* anchor = &state->layout.anchors[sel];
                if (anchor->type != ANCHOR_TYPE_CURVE) {
                    printf("[Editor] Anchor %d must be a curve to toggle handle linking.\n", sel);
                } else {
                    Editor_HistoryCapture(&state->editor, &state->layout);
                    bool target = !anchor->handlesLinked;
                    if (Layout_SetHandlesLinked(&state->layout, sel, target)) {
                        printf("[Editor] Anchor %d handles: %s\n",
                               sel, target ? "LINKED" : "UNLINKED");
                    }
                }
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
