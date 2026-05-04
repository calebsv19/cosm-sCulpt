#include "UI/input_ui_panel.h"
#include "UI/ui_panel.h"

#include "Core/global_state.h"

#include "Layout/layout_origin.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout_json.h"

#include "Editor/editor.h"


bool UIPanel_HandleClick(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    Grid* grid = &state->grid;

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

    for (int i = 0; i < ui->count; ++i) {
        UIButton* btn = &ui->buttons[i];
        SDL_Rect r = btn->bounds;

        if (mouseX >= r.x && mouseX <= r.x + r.w &&
            mouseY >= r.y && mouseY <= r.y + r.h) {

            btn->pressed = true;

            switch (btn->id) {
		    // ─── LEFT PANEL ACTIONS ─────────────────────
                case UI_BTN_SAVE_JSON: { // Save JSON
                ui->loadMenu.open = false;
                UIPanel_BeginSaveDialog();
                break;
	}

	case UI_BTN_LOAD_JSON: { // Load JSON
                UIPanel_ToggleLoadMenu();
			break;
		}

	case UI_BTN_EXPORT_SHAPE: { // Export Shape
                ui->loadMenu.open = false;
                UIPanel_ExportShape();
                break;
            }
                case UI_BTN_EXPORT_SCENE: { // Export Scene
                    ui->loadMenu.open = false;
                    UIPanel_ExportScene();
                    break;
                }

                case UI_BTN_INPUT_ROOT_EDIT: { // Edit input root
                    ui->loadMenu.open = false;
                    UIPanel_BeginInputRootDialog();
                    break;
                }
                case UI_BTN_INPUT_ROOT_FOLDER: { // Pick input root via folder chooser
                    ui->loadMenu.open = false;
                    UIPanel_OpenInputRootFolderDialog();
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
                        Editor_HistoryCapture(editor, &state->layout);
                        Layout_ShiftOriginToAnchor(&state->layout, grid, sel, state->screenWidth, state->screenHeight);
                    }
                    break;
                }
                case UI_BTN_ZOOM_IN: { // Zoom In
                    ui->loadMenu.open = false;
                    int w = state->screenWidth;
                    int h = state->screenHeight;
                    Grid_zoom(grid, 1.1f, w / 2.0f, h / 2.0f);
                    Global_FlagGridChanged();
                    break;
                }
                case UI_BTN_ZOOM_OUT: { // Zoom Out
                    ui->loadMenu.open = false;
                    int w = state->screenWidth;
                    int h = state->screenHeight;
                    Grid_zoom(grid, 0.9f, w / 2.0f, h / 2.0f);
                    Global_FlagGridChanged();
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
                    (void)UIPanel_CreatePlanePrimitiveFromActiveContext(false);
                    break;
                }
                case UI_BTN_CREATE_RECT_PRISM: { // Add rectangular prism primitive
                    ui->loadMenu.open = false;
                    (void)UIPanel_CreateRectPrismPrimitiveFromActiveContext(false);
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
