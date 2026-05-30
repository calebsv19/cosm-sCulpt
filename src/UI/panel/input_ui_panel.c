#include "UI/input_ui_panel.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"

#include "Core/global_state.h"
#include "Core/viewport_zoom.h"

#include "Layout/layout_origin.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout_json.h"

#include "Editor/editor.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"

enum {
    UI_FILE_BROWSER_BUTTON_DOUBLE_CLICK_MS = 350
};

static bool UIPanel_IsFileBrowserModeButton(int button_id) {
    return button_id == UI_BTN_LOAD_JSON || button_id == UI_BTN_LOAD_SCENE;
}

static bool UIPanel_HandleFileBrowserModeButtonClick(UIPanelState* ui, int button_id) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    Uint32 now = SDL_GetTicks();
    bool is_double_click = false;
    if (!ui || !UIPanel_IsFileBrowserModeButton(button_id)) return false;

    is_double_click = (ui->loadMenu.lastModeButtonId == button_id) &&
                      (now - ui->loadMenu.lastModeButtonClickTicks <= UI_FILE_BROWSER_BUTTON_DOUBLE_CLICK_MS);
    ui->loadMenu.lastModeButtonId = button_id;
    ui->loadMenu.lastModeButtonClickTicks = now;

    if (object_mode) {
        if (button_id == UI_BTN_LOAD_JSON) {
            if (is_double_click) {
                return UIPanel_OpenObjectAssetFolderDialog();
            }
            UIPanel_ActivateObjectAssetBrowser();
            return true;
        }
        return UIPanel_NewObjectAssetDocument();
    }

    if (button_id == UI_BTN_LOAD_JSON) {
        if (is_double_click) {
            return UIPanel_OpenJsonFolderDialog();
        }
        UIPanel_ActivateJsonBrowser();
        return true;
    }

    if (is_double_click) {
        return UIPanel_OpenSceneFolderDialog();
    }
    UIPanel_ActivateSceneBrowser();
    return true;
}

static void UIPanel_ObjectAuthoringSetFaceSelect(GlobalState* state) {
    EditorState* editor = NULL;
    if (!state) return;
    editor = &state->editor;
    Editor_ObjectFaceExtrudeClear(editor);
    Editor_ObjectFaceSketchDeselect(editor);
    editor->objectAuthoringMode =
        editor->selectedObjectAssetFace != OBJECT3D_FACE_NONE
            ? OBJECT_AUTHORING_MODE_FACE_SELECT
            : Editor_ObjectAuthoringIdleMode(editor);
}

static void UIPanel_ObjectAuthoringSelectCommittedSketch(GlobalState* state) {
    EditorState* editor = NULL;
    if (!state) return;
    editor = &state->editor;
    Editor_ObjectFaceExtrudeClear(editor);
    if (Editor_ObjectFaceSketchHasCommittedRectangle(editor)) {
        (void)Editor_ObjectFaceSketchSelect(editor, OBJECT_FACE_SKETCH_HANDLE_BODY);
    }
}

static void UIPanel_ObjectAuthoringClearSketch(GlobalState* state) {
    EditorState* editor = NULL;
    if (!state) return;
    editor = &state->editor;
    Editor_ObjectFaceExtrudeClear(editor);
    Editor_ObjectFaceSketchClear(editor);
    editor->objectAuthoringMode =
        editor->selectedObjectAssetFace != OBJECT3D_FACE_NONE
            ? OBJECT_AUTHORING_MODE_FACE_SELECT
            : Editor_ObjectAuthoringIdleMode(editor);
}

bool UIPanel_HandleClick(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    Grid* grid = &state->grid;
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const bool has_face_target =
        object_mode &&
        state->editor.selectedObjectAssetBodyId != 0u &&
        state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE;
    const bool has_committed_sketch =
        object_mode && state->editor.objectFaceSketchHasRectangle;
    const bool sketch_active =
        object_mode &&
        (state->editor.objectFaceSketchToolArmed ||
         state->editor.objectFaceSketchDragging ||
         state->editor.objectFaceSketchHasRectangle);

    if (UIPanel_IsSaveDialogActive() ||
        UIPanel_IsRootDialogActive() ||
        UIPanel_IsPrismDimensionDialogActive() ||
        UIPanel_IsSceneBoundsDialogActive() ||
        UIPanel_IsConstructionPlaneDialogActive() ||
        UIPanel_IsObjectTransformDialogActive()) {
        return true;
    }

    if (UIPanel_IsLoadMenuOpen() && UIPanel_HandleLoadMenuClick(mouseX, mouseY)) {
        return true;
    }

    if (UIPanel_HandleTabClick(ui, mouseX, mouseY)) {
        ui->loadMenu.open = false;
        UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
        return true;
    }

    if (UIPanel_HandleSceneListClick(mouseX, mouseY)) {
        ui->loadMenu.open = false;
        return true;
    }

    for (int i = 0; i < ui->count; ++i) {
        UIButton* btn = &ui->buttons[i];
        SDL_Rect r = btn->bounds;

        if (r.w <= 0 || r.h <= 0) continue;
        if (!UIPanel_ShouldShowGroup(ui, btn->group)) continue;

        if (mouseX >= r.x && mouseX <= r.x + r.w &&
            mouseY >= r.y && mouseY <= r.y + r.h) {

            btn->pressed = true;
            if (!UIPanel_IsFileBrowserModeButton(btn->id)) {
                ui->loadMenu.lastModeButtonId = -1;
                ui->loadMenu.lastModeButtonClickTicks = 0u;
            }

            switch (btn->id) {
		    // ─── LEFT PANEL ACTIONS ─────────────────────
                case UI_BTN_SAVE_JSON: { // Save JSON
                ui->loadMenu.open = false;
                UIPanel_BeginSaveDialog();
                break;
	}

	case UI_BTN_LOAD_JSON: { // Load JSON
                (void)UIPanel_HandleFileBrowserModeButtonClick(ui, btn->id);
			break;
		}

            case UI_BTN_LOAD_SCENE: { // Load Scene
                (void)UIPanel_HandleFileBrowserModeButtonClick(ui, btn->id);
                break;
            }

	case UI_BTN_EXPORT_SHAPE: { // Export Shape
                ui->loadMenu.open = false;
                UIPanel_ExportShape();
                break;
            }
                case UI_BTN_EXPORT_SCENE: { // Export Scene
                    ui->loadMenu.open = false;
                    if (!object_mode) {
                        UIPanel_ExportScene();
                    }
                    break;
                }
                case UI_BTN_FILE_BROWSER_USE_ACTIVE: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_FocusFileBrowserOnActiveSession();
                    break;
                }
                case UI_BTN_FILE_BROWSER_CLEAR_REMEMBERED: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_ClearRememberedFileBrowserEntry();
                    break;
                }
                case UI_BTN_SCENE_CLEAR_SELECTION: {
                    ui->loadMenu.open = false;
                    UIPanel_SceneListClearSelection();
                    break;
                }
                case UI_BTN_SCENE_DELETE_SELECTED: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_SceneListDeleteSelectedObject();
                    break;
                }

                case UI_BTN_INPUT_ROOT_EDIT: { // Edit input root
                    ui->loadMenu.open = false;
                    if (object_mode) {
                        UIPanel_BeginObjectAssetRootDialog();
                    } else {
                        UIPanel_BeginInputRootDialog();
                    }
                    break;
                }
                case UI_BTN_INPUT_ROOT_FOLDER: { // Pick input root via folder chooser
                    ui->loadMenu.open = false;
                    if (object_mode) {
                        UIPanel_OpenObjectAssetFolderDialog();
                    } else {
                        UIPanel_OpenInputRootFolderDialog();
                    }
                    break;
                }
                case UI_BTN_OUTPUT_ROOT_EDIT: { // Edit output root
                    ui->loadMenu.open = false;
                    UIPanel_BeginOutputRootDialog();
                    break;
                }
                case UI_BTN_OUTPUT_ROOT_FOLDER: { // Pick output root via folder chooser
                    ui->loadMenu.open = false;
                    UIPanel_OpenOutputRootFolderDialog();
                    break;
                }


		// ───RIGHT PANEL ACTIONS  ─────────────────────
                case UI_BTN_RESET_ORIGIN: { // Reset Origin
                    ui->loadMenu.open = false;
                    int sel = editor->selectedAnchorIndex;
                    if (sel >= 0) {
                        float centerX = (float)state->screenWidth * 0.5f;
                        float centerY = (float)state->screenHeight * 0.5f;
                        (void)LineDrawingPaneHost_GetViewportCenter(&state->paneHost, &centerX, &centerY);
                        Editor_HistoryCapture(editor, &state->layout);
                        Layout_ShiftOriginToAnchor(&state->layout, grid, sel, centerX, centerY);
                    }
                    break;
                }
                case UI_BTN_ZOOM_IN: { // Zoom In
                    ui->loadMenu.open = false;
                    float centerX = (float)state->screenWidth * 0.5f;
                    float centerY = (float)state->screenHeight * 0.5f;
                    (void)LineDrawingPaneHost_GetViewportCenter(&state->paneHost, &centerX, &centerY);
                    if (LineDrawingViewportZoom_Apply(state, 1.1f, centerX, centerY)) {
                        Global_FlagGridChanged();
                    }
                    break;
                }
                case UI_BTN_ZOOM_OUT: { // Zoom Out
                    ui->loadMenu.open = false;
                    float centerX = (float)state->screenWidth * 0.5f;
                    float centerY = (float)state->screenHeight * 0.5f;
                    (void)LineDrawingPaneHost_GetViewportCenter(&state->paneHost, &centerX, &centerY);
                    if (LineDrawingViewportZoom_Apply(state, 0.9f, centerX, centerY)) {
                        Global_FlagGridChanged();
                    }
                    break;
                }
                case UI_BTN_TOGGLE_DELETE: { // Toggle Delete Mode
                    ui->loadMenu.open = false;
                    if (editor->deleteMode == DELETE_MODE_SAFE)
                        editor->deleteMode = DELETE_MODE_AUTO_PRUNE;
                    else
                        editor->deleteMode = DELETE_MODE_SAFE;
                    break;
                }
                case UI_BTN_PIN_ANCHOR: { // Pin Anchor
                    ui->loadMenu.open = false;
                    int sel = editor->selectedAnchorIndex;
                    if (sel >= 0 && sel < (int)state->layout.anchorCount) {
                        Editor_HistoryCapture(editor, &state->layout);
                        Anchor* a = &state->layout.anchors[sel];
                        a->isPersistent = !a->isPersistent;
                        Global_FlagLayoutChanged();
                    }
                    break;
                }
                case UI_BTN_LINK_HANDLES: { // Toggle handle linking
                    ui->loadMenu.open = false;
                    int sel = editor->selectedAnchorIndex;
                    if (sel >= 0 && sel < (int)state->layout.anchorCount) {
                        Anchor* a = &state->layout.anchors[sel];
                        if (a->type == ANCHOR_TYPE_CURVE) {
                            Editor_HistoryCapture(editor, &state->layout);
                            bool target = !a->handlesLinked;
                            Layout_SetHandlesLinked(&state->layout, sel, target);
                        }
                    }
                    break;
                }
                case UI_BTN_CREATE_PLANE: { // Add plane primitive
                    ui->loadMenu.open = false;
                    if (object_mode) {
                        if (has_face_target || has_committed_sketch) {
                            (void)Editor_ObjectFaceSketchArmRectangle(state);
                            UIPanel_FocusObjectAuthoringTab(ui);
                        } else {
                            (void)UIPanel_CreatePlanePrimitiveFromActiveContext(false);
                        }
                    } else {
                        (void)UIPanel_CreatePlanePrimitiveFromActiveContext(false);
                    }
                    break;
                }
                case UI_BTN_CREATE_RECT_PRISM: { // Add rectangular prism primitive
                    ui->loadMenu.open = false;
                    if (!object_mode) {
                        (void)UIPanel_CreateRectPrismPrimitiveFromActiveContext(false);
                    }
                    break;
                }
                case UI_BTN_OBJECT_FACE_SELECT: {
                    ui->loadMenu.open = false;
                    if (object_mode) {
                        UIPanel_ObjectAuthoringSetFaceSelect(state);
                        UIPanel_FocusObjectAuthoringTab(ui);
                    }
                    break;
                }
                case UI_BTN_OBJECT_SKETCH_SELECT: {
                    ui->loadMenu.open = false;
                    if (object_mode && has_committed_sketch) {
                        UIPanel_ObjectAuthoringSelectCommittedSketch(state);
                        UIPanel_FocusObjectAuthoringTab(ui);
                    }
                    break;
                }
                case UI_BTN_OBJECT_SKETCH_CLEAR: {
                    ui->loadMenu.open = false;
                    if (object_mode && sketch_active) {
                        UIPanel_ObjectAuthoringClearSketch(state);
                        UIPanel_FocusObjectAuthoringTab(ui);
                    }
                    break;
                }
                case UI_BTN_EXTRUDE_ADD:
                case UI_BTN_EXTRUDE_CUT: {
                    ui->loadMenu.open = false;
                    if (Editor_ObjectFaceExtrudeTrigger(
                        state,
                        btn->id == UI_BTN_EXTRUDE_ADD
                            ? OBJECT_FACE_EXTRUDE_MODE_ADD
                            : OBJECT_FACE_EXTRUDE_MODE_CUT)) {
                        UIPanel_FocusObjectAuthoringTab(ui);
                    }
                    break;
                }
                case UI_BTN_EDIT_PRISM_WIDTH: { // Edit selected prism width
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginPrismWidthDialog();
                    break;
                }
                case UI_BTN_EDIT_PRISM_HEIGHT: { // Edit selected prism height
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginPrismHeightDialog();
                    break;
                }
                case UI_BTN_EDIT_PRISM_DEPTH: { // Edit selected prism depth
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginPrismDepthDialog();
                    break;
                }
                case UI_BTN_CYCLE_DISPLAY_UNITS: { // Cycle display units
                    ui->loadMenu.open = false;
                    UIPanel_CycleDisplayUnit();
                    break;
                }
                case UI_BTN_OBJECT_CLEAR_SELECTION: {
                    ui->loadMenu.open = false;
                    UIPanel_SceneListClearSelection();
                    break;
                }
                case UI_BTN_OBJECT_DELETE_SELECTED: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_SceneListDeleteSelectedObject();
                    break;
                }
                case UI_BTN_TOGGLE_OBJECT_GIZMO_MODE: { // Toggle object gizmo move/rotate mode
                    ui->loadMenu.open = false;
                    (void)UIPanel_ToggleObjectGizmoRotateMode();
                    break;
                }
                case UI_BTN_EDIT_OBJECT_POSITION: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginObjectPositionDialog();
                    break;
                }
                case UI_BTN_EDIT_OBJECT_ROTATION_X: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginObjectRotationXDialog();
                    break;
                }
                case UI_BTN_EDIT_OBJECT_ROTATION_Y: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginObjectRotationYDialog();
                    break;
                }
                case UI_BTN_EDIT_OBJECT_ROTATION_Z: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginObjectRotationZDialog();
                    break;
                }
                case UI_BTN_TOGGLE_SCENE_BOUNDS: { // Toggle scene bounds enabled
                    ui->loadMenu.open = false;
                    (void)UIPanel_ToggleSceneBoundsEnabled();
                    break;
                }
                case UI_BTN_TOGGLE_SCENE_BOUNDS_CLAMP: { // Toggle scene bounds clamp-on-edit
                    ui->loadMenu.open = false;
                    (void)UIPanel_ToggleSceneBoundsClampOnEdit();
                    break;
                }
                case UI_BTN_EDIT_SCENE_BOUNDS_MIN: { // Edit scene bounds min vector
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginSceneBoundsMinDialog();
                    break;
                }
                case UI_BTN_EDIT_SCENE_BOUNDS_MAX: { // Edit scene bounds max vector
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginSceneBoundsMaxDialog();
                    break;
                }
                case UI_BTN_FIT_SCENE_BOUNDS_TO_OBJECT: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_FitSceneBoundsToSelectedObject();
                    break;
                }
                case UI_BTN_SET_CONSTRUCTION_PLANE_XY: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_SetConstructionPlaneAxis(VIEW_PLANE_XY);
                    break;
                }
                case UI_BTN_SET_CONSTRUCTION_PLANE_YZ: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_SetConstructionPlaneAxis(VIEW_PLANE_YZ);
                    break;
                }
                case UI_BTN_SET_CONSTRUCTION_PLANE_XZ: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_SetConstructionPlaneAxis(VIEW_PLANE_XZ);
                    break;
                }
                case UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_NEG: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_AdjustConstructionPlaneOffset(-state->grid.gridSize);
                    break;
                }
                case UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_POS: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_AdjustConstructionPlaneOffset(state->grid.gridSize);
                    break;
                }
                case UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET: {
                    ui->loadMenu.open = false;
                    (void)UIPanel_BeginConstructionPlaneOffsetDialog();
                    break;
                }
                case UI_BTN_TOGGLE_SPACE_MODE: { // Toggle 2D/3D mode
                    ui->loadMenu.open = false;
                    if (Global_ToggleSpaceMode(true)) {
                        SDL_Log("[UI] Space mode: %s", Global_GetSpaceModeLabel(state->spaceMode));
                    }
                    break;
                }
            }

            return true;
        }
    }

    return false;
}
