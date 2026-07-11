// src/Input/input_keyboard.c
#include "input_keyboard.h"
#include "input_editor_actions.h"
#include "Core/global_state.h"
#include "Core/line_drawing_pane_host.h"
#include "Core/viewport_zoom.h"
#include "Editor/editor.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"
#include "Editor/scene_authoring_path_handles.h"
#include "Layout/layout.h"
#include "Layout/layout_origin.h"
#include "Layout/Grid/grid.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include <SDL2/SDL.h>

static bool Input_CancelObjectFaceAuthoring(GlobalState* state) {
    bool cancelled = false;
    if (!state) return false;
    if (state->editor.objectFaceExtrudeDragging ||
        state->editor.objectFaceExtrudeToolArmed ||
        state->editor.objectFaceExtrudeHasPreview ||
        state->editor.objectFaceSketchToolArmed ||
        state->editor.objectFaceSketchDragging ||
        state->editor.objectFaceSketchHasRectangle) {
        Editor_ObjectFaceExtrudeClear(&state->editor);
        Editor_ObjectFaceSketchClear(&state->editor);
        state->editor.objectAuthoringMode = Editor_ObjectAuthoringIdleMode(&state->editor);
        Global_FlagHitboxesDirty();
        cancelled = true;
    }
    return cancelled;
}

static bool Input_HandleObjectFaceAuthoringOperationShortcut(GlobalState* state,
                                                             SDL_Keycode keycode) {
    ObjectFaceExtrudeMode extrude_mode = OBJECT_FACE_EXTRUDE_MODE_ADD;

    if (!state) return false;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;

    if (keycode == SDLK_EQUALS || keycode == SDLK_PLUS || keycode == SDLK_KP_PLUS) {
        extrude_mode = OBJECT_FACE_EXTRUDE_MODE_ADD;
    } else if (keycode == SDLK_MINUS || keycode == SDLK_KP_MINUS) {
        extrude_mode = OBJECT_FACE_EXTRUDE_MODE_CUT;
    } else {
        return false;
    }

    if (Editor_ObjectFaceExtrudeTrigger(state, extrude_mode)) {
        UIPanel_FocusObjectAuthoringTab(UIPanel_Get());
        return true;
    }
    return false;
}

static bool Input_HandleObjectFaceAuthoringWorkflowShortcut(GlobalState* state,
                                                            SDL_Keycode keycode) {
    if (!state) return false;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;

    if (keycode == SDLK_r) {
        if (Editor_ObjectFaceSketchArmRectangle(state)) {
            UIPanel_FocusObjectAuthoringTab(UIPanel_Get());
            return true;
        }
        return false;
    }

    if (!state->editor.objectFaceSketchHasRectangle) return false;
    if (state->editor.selectedObjectAssetBodyId != state->editor.objectFaceSketchBodyId ||
        state->editor.selectedObjectAssetFace != state->editor.objectFaceSketchFace) {
        return false;
    }

    if (keycode == SDLK_s) {
        if (Editor_ObjectFaceSketchSelect(&state->editor, OBJECT_FACE_SKETCH_HANDLE_BODY)) {
            UIPanel_FocusObjectAuthoringTab(UIPanel_Get());
            Global_FlagHitboxesDirty();
            return true;
        }
        return false;
    }

    if (keycode == SDLK_f) {
        Editor_ObjectFaceExtrudeClear(&state->editor);
        Editor_ObjectFaceSketchDeselect(&state->editor);
        if (state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE) {
            state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_FACE_SELECT;
        }
        UIPanel_FocusObjectAuthoringTab(UIPanel_Get());
        Global_FlagHitboxesDirty();
        return true;
    }

    return false;
}

// 		Continuous movement (arrow keys, zoom, quit)
// ============================================================
static void HandleHeldKeys(AppContext* ctx) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    const SDL_Keymod mods = SDL_GetModState();
    const bool primaryModifierHeld = (mods & (KMOD_CTRL | KMOD_GUI)) != 0;
    float panSpeed = 300.0f * ctx->deltaTime;

    float anchorX = (float)Global_GetScreenWidth() * 0.5f;
    float anchorY = (float)Global_GetScreenHeight() * 0.5f;

    bool gridChanged = false;

    if (LineDrawingPaneHost_GetViewportCenter(&state->paneHost, &anchorX, &anchorY)) {
        // Use the active viewport center rather than the window center.
    }

    if (keys[SDL_SCANCODE_LEFT])   { Grid_pan(grid,  panSpeed,  0); gridChanged = true; }
    if (keys[SDL_SCANCODE_RIGHT])  { Grid_pan(grid, -panSpeed,  0); gridChanged = true; }
    if (keys[SDL_SCANCODE_UP])     { Grid_pan(grid, 0,   panSpeed); gridChanged = true; }
    if (keys[SDL_SCANCODE_DOWN])   { Grid_pan(grid, 0,  -panSpeed); gridChanged = true; }

    if (!primaryModifierHeld) {
        if (keys[SDL_SCANCODE_EQUALS]) {
            gridChanged = LineDrawingViewportZoom_Apply(state, 1.05f, anchorX, anchorY) || gridChanged;
        }
        if (keys[SDL_SCANCODE_MINUS]) {
            gridChanged = LineDrawingViewportZoom_Apply(state, 0.95f, anchorX, anchorY) || gridChanged;
        }
    }

    if (gridChanged) {
        Global_FlagGridChanged();
    }

}

static bool Input_Is3DMode(const GlobalState* state) {
    return state && state->spaceMode == SPACE_MODE_3D;
}

static void Input_SetActivePlane(GlobalState* state, ViewPlane plane) {
    if (!state) return;
    state->activePlane = plane;
    Layout_ConstructionPlane3D_SetFromViewPlane(&state->layout.scene3d.constructionPlane, plane);
}

static void Input_RefreshUILayoutAfterFontStep(void) {
    GlobalState* state = Global_Get();
    if (!state) return;
    Global_SetWindowSize(state->screenWidth, state->screenHeight);
    Global_FlagGridChanged();
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

        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
            if (Input_CancelObjectFaceAuthoring(state)) {
                return;
            }
            ctx->quit = true;
            return;
        }

        if (event->type == SDL_KEYDOWN &&
            !primaryModifier &&
            Input_HandleObjectFaceAuthoringOperationShortcut(state,
                                                             event->key.keysym.sym)) {
            return;
        }
        if (event->type == SDL_KEYDOWN &&
            !primaryModifier &&
            (mods & (KMOD_SHIFT | KMOD_ALT)) == 0 &&
            Input_HandleObjectFaceAuthoringWorkflowShortcut(state,
                                                            event->key.keysym.sym)) {
            return;
        }

        if (event->type == SDL_KEYDOWN && primaryModifier) {
            if (event->key.keysym.sym == SDLK_EQUALS ||
                event->key.keysym.sym == SDLK_PLUS ||
                event->key.keysym.sym == SDLK_KP_PLUS) {
                if (FontManager_AdjustZoomStep(+1)) {
                    printf("[UI] Font zoom step: %d\n", FontManager_GetZoomStep());
                    Input_RefreshUILayoutAfterFontStep();
                }
                return;
            } else if (event->key.keysym.sym == SDLK_MINUS ||
                       event->key.keysym.sym == SDLK_KP_MINUS) {
                if (FontManager_AdjustZoomStep(-1)) {
                    printf("[UI] Font zoom step: %d\n", FontManager_GetZoomStep());
                    Input_RefreshUILayoutAfterFontStep();
                }
                return;
            } else if (event->key.keysym.sym == SDLK_0 ||
                       event->key.keysym.sym == SDLK_KP_0) {
                if (FontManager_SetZoomStep(0)) {
                    printf("[UI] Font zoom step reset: %d\n", FontManager_GetZoomStep());
                    Input_RefreshUILayoutAfterFontStep();
                }
                return;
            } else
            if ((mods & KMOD_SHIFT) && event->key.keysym.sym == SDLK_b) {
                (void)UIPanel_OpenOutputRootFolderDialog();
                return;
            } else if (event->key.keysym.sym == SDLK_b) {
                (void)UIPanel_OpenInputRootFolderDialog();
                return;
            } else if (event->key.keysym.sym == SDLK_i) {
                UIPanel_BeginInputRootDialog();
                return;
            } else if (event->key.keysym.sym == SDLK_o) {
                UIPanel_BeginOutputRootDialog();
                return;
            } else if ((mods & KMOD_SHIFT) && event->key.keysym.sym == SDLK_t) {
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
            (void)InputEditorAction_ToggleSpaceMode(true);
            return;
        }

        if (event->type == SDL_KEYDOWN &&
            event->key.keysym.sym == SDLK_x &&
            (mods & (KMOD_CTRL | KMOD_GUI | KMOD_ALT)) == 0) {
            (void)InputEditorAction_ToggleObjectGizmoMode();
            return;
        }

        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_v) {
            (void)InputEditorAction_ToggleFreeView();
            return;
        }

        if (event->type == SDL_KEYDOWN &&
            event->key.keysym.sym == SDLK_n &&
            (mods & (KMOD_CTRL | KMOD_GUI | KMOD_ALT)) == 0) {
            if (Global_TogglePreviewMode()) {
                printf("[Editor] Preview mode: %s\n",
                       Global_GetPreviewModeLabel(Global_GetPreviewMode()));
            }
            return;
        }

        if (event->type == SDL_KEYDOWN &&
            event->key.keysym.sym == SDLK_h &&
            (mods & (KMOD_CTRL | KMOD_GUI | KMOD_ALT)) == 0) {
            state->editor.sceneBoundsHandlesVisible = !state->editor.sceneBoundsHandlesVisible;
            if (!state->editor.sceneBoundsHandlesVisible) {
                state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
                state->editor.hoveredSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
                state->editor.hoveredSceneBoundsGizmoAxis = -1;
                state->editor.activeSceneBoundsGizmoAxis = -1;
                state->editor.isResizingSceneBounds = false;
            }
            Global_FlagHitboxesDirty();
            printf("[Editor] Scene bounds handles: %s\n",
                   state->editor.sceneBoundsHandlesVisible ? "ON" : "OFF");
            return;
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
            ViewPlane plane = state->activePlane;
            plane.axis = VIEW_PLANE_XY;
            Input_SetActivePlane(state, plane);
            Global_FlagHitboxesDirty();
            printf("[Editor] Active plane: XY (z = %.2f)\n", state->activePlane.offset);
        }
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_2) {
            ViewPlane plane = state->activePlane;
            plane.axis = VIEW_PLANE_YZ;
            Input_SetActivePlane(state, plane);
            Global_FlagHitboxesDirty();
            printf("[Editor] Active plane: YZ (x = %.2f)\n", state->activePlane.offset);
        }
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_3) {
            ViewPlane plane = state->activePlane;
            plane.axis = VIEW_PLANE_XZ;
            Input_SetActivePlane(state, plane);
            Global_FlagHitboxesDirty();
            printf("[Editor] Active plane: XZ (y = %.2f)\n", state->activePlane.offset);
        }
        if (event->type == SDL_KEYDOWN &&
            (event->key.keysym.sym == SDLK_LEFTBRACKET || event->key.keysym.sym == SDLK_RIGHTBRACKET)) {
            float step = state->grid.gridSize;
            if (mods & KMOD_SHIFT) step *= 10.0f;
            if (event->key.keysym.sym == SDLK_LEFTBRACKET) step = -step;
            ViewPlane plane = state->activePlane;
            plane.offset += step;
            Input_SetActivePlane(state, plane);
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
            float centerX = (float)state->screenWidth * 0.5f;
            float centerY = (float)state->screenHeight * 0.5f;
            (void)LineDrawingPaneHost_GetViewportCenter(&state->paneHost, &centerX, &centerY);
            Editor_HistoryCapture(&state->editor, &state->layout);
	        Layout_ShiftOriginToAnchor(
	            &state->layout,
	            &state->grid,
	            i,
	            centerX,
	            centerY
	        );
	        printf("[Editor] Origin shifted to anchor %d\n", i);
	    } else {
	        printf("[Editor] No anchor selected to shift origin.\n");
	    }
	}

	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_p && (mods & KMOD_SHIFT)) {
            (void)UIPanel_CreatePlanePrimitiveFromActiveContext((mods & KMOD_ALT) != 0);
            return;
        }

	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_r && (mods & KMOD_SHIFT)) {
            (void)UIPanel_CreateRectPrismPrimitiveFromActiveContext((mods & KMOD_ALT) != 0);
            return;
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
            if (SceneAuthoringPathHandles_DeleteSelectedControlPoint(state, &state->editor)) {
                return;
            }
            if (state->layout.sceneAuthoring.selected_kind !=
                LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE) {
                if (UIPanel_SceneListDeleteSelectedObject()) {
                    return;
                }
            }
            bool hasWall = state->editor.selectedWallIndex >= 0;
            bool hasAnchor = state->editor.selectedAnchorIndex >= 0;
            bool hasObject = state->editor.selectedObject3DId != 0u;
            if (hasWall || hasAnchor || hasObject) {
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
            if (hasObject) {
                (void)Layout_ObjectStore_Delete(&state->layout.objectStore, state->editor.selectedObject3DId);
                state->editor.selectedObject3DId = 0u;
                state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
                state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
            }
        }
    }
}
