#include "UI/ui_panel_right_controls.h"
#include "Core/global_state.h"
#include "UI/ui_panel_scene_authoring_inspector.h"

typedef struct UIPanelRightControlRowSpec {
    int row_key;
    int columns;
    int column_index;
} UIPanelRightControlRowSpec;

static bool UIPanel_RightControlButtonVisible(int button_id) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const bool scene_authoring_selected =
        !object_mode && UIPanel_SceneAuthoringInspectorHasSelection();
    const GlobalState* state = Global_Get();
    const LineDrawingSceneAuthoringSelectionKind authoring_kind =
        scene_authoring_selected && state
            ? state->layout.sceneAuthoring.selected_kind
            : LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE;
    switch (button_id) {
        case UI_BTN_SCENE_AUTHORING_EDIT_MODE:
            return authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
                   authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH;
        case UI_BTN_SCENE_AUTHORING_LIGHT_ENABLED:
        case UI_BTN_SCENE_AUTHORING_LIGHT_KIND:
        case UI_BTN_SCENE_AUTHORING_LIGHT_PATH:
            return authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT;
        case UI_BTN_SCENE_AUTHORING_PATH_KIND:
            return authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH;
        case UI_BTN_SCENE_AUTHORING_MATERIAL_COLOR:
            return authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL;
        case UI_BTN_OBJECT_CLEAR_SELECTION:
        case UI_BTN_OBJECT_DELETE_SELECTED:
        case UI_BTN_EDIT_PRISM_WIDTH:
        case UI_BTN_EDIT_PRISM_HEIGHT:
        case UI_BTN_EDIT_PRISM_DEPTH:
        case UI_BTN_CYCLE_DISPLAY_UNITS:
        case UI_BTN_TOGGLE_OBJECT_GIZMO_MODE:
        case UI_BTN_EDIT_OBJECT_POSITION:
        case UI_BTN_EDIT_OBJECT_ROTATION_X:
        case UI_BTN_EDIT_OBJECT_ROTATION_Y:
        case UI_BTN_EDIT_OBJECT_ROTATION_Z:
            return !scene_authoring_selected;
        case UI_BTN_CREATE_RECT_PRISM:
            return !object_mode;
        case UI_BTN_PLACE_MESH_INSTANCE:
            return !object_mode;
        case UI_BTN_CREATE_LIGHT:
        case UI_BTN_CREATE_CAMERA_PATH:
        case UI_BTN_CREATE_MATERIAL:
            return !object_mode;
        case UI_BTN_CREATE_PLANE:
            return true;
        case UI_BTN_OBJECT_FACE_SELECT:
        case UI_BTN_OBJECT_SKETCH_SELECT:
        case UI_BTN_OBJECT_SKETCH_CLEAR:
        case UI_BTN_EXTRUDE_ADD:
        case UI_BTN_EXTRUDE_CUT:
        case UI_BTN_EXTRUDE_DEPTH_DEC:
        case UI_BTN_EXTRUDE_DEPTH_INC:
        case UI_BTN_OBJECT_EDIT_BODY_MODE:
        case UI_BTN_OBJECT_EDIT_FACE_MODE:
        case UI_BTN_OBJECT_EDIT_EDGE_MODE:
        case UI_BTN_OBJECT_EDIT_VERTEX_MODE:
            return object_mode;
        default:
            return true;
    }
}

static bool UIPanel_RightControlRowSpecForButton(int button_id, UIPanelRightControlRowSpec* out_spec) {
    UIPanelRightControlRowSpec spec = {0, 0, 0};
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    if (!UIPanel_RightControlButtonVisible(button_id)) return false;
    switch (button_id) {
        case UI_BTN_RESET_ORIGIN: spec = (UIPanelRightControlRowSpec){ 1, 3, 0 }; break;
        case UI_BTN_ZOOM_IN: spec = (UIPanelRightControlRowSpec){ 1, 3, 1 }; break;
        case UI_BTN_ZOOM_OUT: spec = (UIPanelRightControlRowSpec){ 1, 3, 2 }; break;

        case UI_BTN_TOGGLE_DELETE: spec = (UIPanelRightControlRowSpec){ 2, 3, 0 }; break;
        case UI_BTN_PIN_ANCHOR: spec = (UIPanelRightControlRowSpec){ 2, 3, 1 }; break;
        case UI_BTN_LINK_HANDLES: spec = (UIPanelRightControlRowSpec){ 2, 3, 2 }; break;
        case UI_BTN_TOGGLE_SPACE_MODE: spec = (UIPanelRightControlRowSpec){ 3, 1, 0 }; break;

        case UI_BTN_CREATE_PLANE: spec = object_mode
            ? (UIPanelRightControlRowSpec){ 4, 2, 1 }
            : (UIPanelRightControlRowSpec){ 4, 2, 0 };
            break;
        case UI_BTN_CREATE_RECT_PRISM: spec = (UIPanelRightControlRowSpec){ 4, 2, 1 }; break;
        case UI_BTN_PLACE_MESH_INSTANCE: spec = (UIPanelRightControlRowSpec){ 5, 1, 0 }; break;
        case UI_BTN_CREATE_LIGHT: spec = (UIPanelRightControlRowSpec){ 1, 3, 0 }; break;
        case UI_BTN_CREATE_CAMERA_PATH: spec = (UIPanelRightControlRowSpec){ 1, 3, 1 }; break;
        case UI_BTN_CREATE_MATERIAL: spec = (UIPanelRightControlRowSpec){ 1, 3, 2 }; break;
        case UI_BTN_OBJECT_FACE_SELECT: spec = (UIPanelRightControlRowSpec){ 4, 2, 0 }; break;
        case UI_BTN_OBJECT_SKETCH_SELECT: spec = (UIPanelRightControlRowSpec){ 5, 2, 0 }; break;
        case UI_BTN_OBJECT_SKETCH_CLEAR: spec = (UIPanelRightControlRowSpec){ 5, 2, 1 }; break;
        case UI_BTN_EXTRUDE_ADD: spec = (UIPanelRightControlRowSpec){ 12, 2, 0 }; break;
        case UI_BTN_EXTRUDE_CUT: spec = (UIPanelRightControlRowSpec){ 12, 2, 1 }; break;
        case UI_BTN_EXTRUDE_DEPTH_DEC: spec = (UIPanelRightControlRowSpec){ 13, 2, 0 }; break;
        case UI_BTN_EXTRUDE_DEPTH_INC: spec = (UIPanelRightControlRowSpec){ 13, 2, 1 }; break;

        case UI_BTN_SET_CONSTRUCTION_PLANE_XY: spec = (UIPanelRightControlRowSpec){ 5, 3, 0 }; break;
        case UI_BTN_SET_CONSTRUCTION_PLANE_YZ: spec = (UIPanelRightControlRowSpec){ 5, 3, 1 }; break;
        case UI_BTN_SET_CONSTRUCTION_PLANE_XZ: spec = (UIPanelRightControlRowSpec){ 5, 3, 2 }; break;
        case UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_NEG: spec = (UIPanelRightControlRowSpec){ 6, 3, 0 }; break;
        case UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_POS: spec = (UIPanelRightControlRowSpec){ 6, 3, 1 }; break;
        case UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET: spec = (UIPanelRightControlRowSpec){ 6, 3, 2 }; break;

        case UI_BTN_OBJECT_CLEAR_SELECTION: spec = (UIPanelRightControlRowSpec){ 7, 2, 0 }; break;
        case UI_BTN_OBJECT_DELETE_SELECTED: spec = (UIPanelRightControlRowSpec){ 7, 2, 1 }; break;
        case UI_BTN_SCENE_AUTHORING_EDIT_MODE: spec = (UIPanelRightControlRowSpec){ 7, 2, 0 }; break;
        case UI_BTN_SCENE_AUTHORING_LIGHT_ENABLED: spec = (UIPanelRightControlRowSpec){ 7, 2, 1 }; break;
        case UI_BTN_EDIT_PRISM_WIDTH: spec = (UIPanelRightControlRowSpec){ 8, 4, 0 }; break;
        case UI_BTN_EDIT_PRISM_HEIGHT: spec = (UIPanelRightControlRowSpec){ 8, 4, 1 }; break;
        case UI_BTN_EDIT_PRISM_DEPTH: spec = (UIPanelRightControlRowSpec){ 8, 4, 2 }; break;
        case UI_BTN_CYCLE_DISPLAY_UNITS: spec = (UIPanelRightControlRowSpec){ 8, 4, 3 }; break;
        case UI_BTN_SCENE_AUTHORING_LIGHT_KIND: spec = (UIPanelRightControlRowSpec){ 8, 1, 0 }; break;
        case UI_BTN_SCENE_AUTHORING_PATH_KIND:
        case UI_BTN_SCENE_AUTHORING_MATERIAL_COLOR:
            spec = (UIPanelRightControlRowSpec){ 8, 1, 0 };
            break;
        case UI_BTN_SCENE_AUTHORING_LIGHT_PATH: spec = (UIPanelRightControlRowSpec){ 9, 1, 0 }; break;
        case UI_BTN_TOGGLE_OBJECT_GIZMO_MODE: spec = (UIPanelRightControlRowSpec){ 9, 1, 0 }; break;
        case UI_BTN_EDIT_OBJECT_POSITION: spec = (UIPanelRightControlRowSpec){ 10, 1, 0 }; break;
        case UI_BTN_EDIT_OBJECT_ROTATION_X: spec = (UIPanelRightControlRowSpec){ 11, 3, 0 }; break;
        case UI_BTN_EDIT_OBJECT_ROTATION_Y: spec = (UIPanelRightControlRowSpec){ 11, 3, 1 }; break;
        case UI_BTN_EDIT_OBJECT_ROTATION_Z: spec = (UIPanelRightControlRowSpec){ 11, 3, 2 }; break;
        case UI_BTN_OBJECT_EDIT_BODY_MODE: spec = (UIPanelRightControlRowSpec){ 14, 4, 0 }; break;
        case UI_BTN_OBJECT_EDIT_FACE_MODE: spec = (UIPanelRightControlRowSpec){ 14, 4, 1 }; break;
        case UI_BTN_OBJECT_EDIT_EDGE_MODE: spec = (UIPanelRightControlRowSpec){ 14, 4, 2 }; break;
        case UI_BTN_OBJECT_EDIT_VERTEX_MODE: spec = (UIPanelRightControlRowSpec){ 14, 4, 3 }; break;
        default: return false;
    }
    if (out_spec) *out_spec = spec;
    return true;
}

static int UIPanel_RightControlsRowCount(UIPanelGroup group) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const bool scene_authoring_selected =
        !object_mode && UIPanel_SceneAuthoringInspectorHasSelection();
    const GlobalState* state = Global_Get();
    const LineDrawingSceneAuthoringSelectionKind authoring_kind =
        scene_authoring_selected && state
            ? state->layout.sceneAuthoring.selected_kind
            : LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE;
    if (scene_authoring_selected) {
        switch (group) {
            case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS:
                return authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
                       authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH
                           ? 1
                           : 0;
            case UI_PANEL_GROUP_RIGHT_PRISM:
                return authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
                       authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH ||
                       authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL
                           ? 1
                           : 0;
            case UI_PANEL_GROUP_RIGHT_GIZMO:
                return authoring_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ? 1 : 0;
            case UI_PANEL_GROUP_RIGHT_TRANSFORM:
                return 0;
            default:
                break;
        }
    }
    switch (group) {
        case UI_PANEL_GROUP_RIGHT_VIEW: return 1;
        case UI_PANEL_GROUP_RIGHT_MODES: return 2;
        case UI_PANEL_GROUP_RIGHT_PRIMITIVES: return object_mode ? 2 : 2;
        case UI_PANEL_GROUP_RIGHT_OPERATIONS: return object_mode ? 2 : 1;
        case UI_PANEL_GROUP_RIGHT_CONSTRUCTION: return 2;
        case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS: return 1;
        case UI_PANEL_GROUP_RIGHT_PRISM: return 1;
        case UI_PANEL_GROUP_RIGHT_GIZMO: return 1;
        case UI_PANEL_GROUP_RIGHT_TRANSFORM: return 2;
        case UI_PANEL_GROUP_RIGHT_EDIT_SELECT: return object_mode ? 1 : 0;
        default: return 0;
    }
}

static SDL_Rect UIPanel_RightControlsGroupRect(const UIPanelState* ui, UIPanelGroup group) {
    SDL_Rect zero = {0, 0, 0, 0};
    if (!ui) return zero;
    switch (group) {
        case UI_PANEL_GROUP_RIGHT_VIEW: return ui->viewPane.viewRect;
        case UI_PANEL_GROUP_RIGHT_MODES: return ui->viewPane.modesRect;
        case UI_PANEL_GROUP_RIGHT_PRIMITIVES: return ui->createPane.primitivesRect;
        case UI_PANEL_GROUP_RIGHT_OPERATIONS: return ui->createPane.operationsRect;
        case UI_PANEL_GROUP_RIGHT_CONSTRUCTION: return ui->createPane.constructionRect;
        case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS: return ui->objectPane.actionsRect;
        case UI_PANEL_GROUP_RIGHT_PRISM: return ui->objectPane.prismRect;
        case UI_PANEL_GROUP_RIGHT_GIZMO: return ui->objectPane.gizmoRect;
        case UI_PANEL_GROUP_RIGHT_TRANSFORM: return ui->objectPane.transformRect;
        case UI_PANEL_GROUP_RIGHT_EDIT_SELECT: return ui->editPane.selectionModeRect;
        default: return zero;
    }
}

static void UIPanel_RightControlsRowGeometry(SDL_Rect group_rect,
                                             int columns,
                                             int compact_gap,
                                             int* out_x,
                                             int* out_widths,
                                             int capacity) {
    int usable_width = group_rect.w;
    int total_gap = 0;
    int base_width = 0;
    if (out_x) *out_x = group_rect.x;
    if (!out_widths || capacity <= 0 || columns <= 0) return;
    if (columns > capacity) columns = capacity;
    total_gap = compact_gap * (columns - 1);
    usable_width -= total_gap;
    if (usable_width < columns * 22) usable_width = columns * 22;
    base_width = usable_width / columns;
    if (out_x) {
        int row_width = (base_width * columns) + total_gap;
        if (row_width < group_rect.w) {
            *out_x = group_rect.x + ((group_rect.w - row_width) / 2);
        }
    }
    for (int col = 0; col < columns; ++col) {
        out_widths[col] = base_width;
    }
}

static void UIPanel_RightControlsLayoutGroup(UIPanelState* ui,
                                             UIPanelGroup group,
                                             const UIPanelLayoutMetrics* metrics) {
    SDL_Rect group_rect = {0, 0, 0, 0};
    int row_y = 0;
    int last_row_key = -1;

    if (!ui || !metrics) return;
    group_rect = UIPanel_RightControlsGroupRect(ui, group);
    if (group_rect.w <= 0 || group_rect.h <= 0) return;
    row_y = group_rect.y + metrics->group_header_height_px;

    for (int i = 0; i < ui->count; ++i) {
        UIButton* button = &ui->buttons[i];
        UIPanelRightControlRowSpec row_spec = {0, 0, 0};
        int row_indices[4] = {-1, -1, -1, -1};
        int row_widths[4] = {0, 0, 0, 0};
        int row_x = group_rect.x;

        if (button->side != UI_PANEL_RIGHT || button->group != group) continue;
        if (!UIPanel_RightControlRowSpecForButton(button->id, &row_spec)) continue;
        if (row_spec.row_key == last_row_key) continue;

        for (int j = 0; j < ui->count; ++j) {
            UIButton* place = &ui->buttons[j];
            UIPanelRightControlRowSpec place_spec = {0, 0, 0};
            if (place->side != UI_PANEL_RIGHT || place->group != group) continue;
            if (!UIPanel_RightControlRowSpecForButton(place->id, &place_spec)) continue;
            if (place_spec.row_key != row_spec.row_key) continue;
            if (place_spec.column_index < 0 || place_spec.column_index >= row_spec.columns) continue;
            if (place_spec.column_index >= 4) continue;
            row_indices[place_spec.column_index] = j;
        }

        UIPanel_RightControlsRowGeometry(group_rect,
                                         row_spec.columns,
                                         metrics->compact_row_gap_px,
                                         &row_x,
                                         row_widths,
                                         4);
        for (int col = 0; col < row_spec.columns && col < 4; ++col) {
            UIButton* place = NULL;
            if (row_indices[col] < 0) continue;
            place = &ui->buttons[row_indices[col]];
            place->bounds = (SDL_Rect){ row_x, row_y, row_widths[col], metrics->button_height_px };
            row_x += row_widths[col] + metrics->compact_row_gap_px;
        }

        row_y += metrics->button_height_px + metrics->button_spacing_px;
        last_row_key = row_spec.row_key;
    }
}

int UIPanel_RightControlsSectionHeight(const UIPanelLayoutMetrics* metrics, UIPanelGroup group) {
    int rows = 0;
    if (!metrics) return 0;
    rows = UIPanel_RightControlsRowCount(group);
    if (rows <= 0) return 0;
    return metrics->group_header_height_px +
           (metrics->button_height_px * rows) +
           (metrics->button_spacing_px * (rows - 1));
}

void UIPanel_LayoutRightPaneButtons(UIPanelState* ui, const UIPanelLayoutMetrics* metrics) {
    if (!ui || !metrics) return;

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].side == UI_PANEL_RIGHT) {
            ui->buttons[i].bounds = (SDL_Rect){0, 0, 0, 0};
        }
    }

    switch (ui->activeRightTab) {
        case UI_PANEL_RIGHT_TAB_VIEW:
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_VIEW, metrics);
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_MODES, metrics);
            break;
        case UI_PANEL_RIGHT_TAB_CREATE:
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_PRIMITIVES, metrics);
            if (Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
                UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_OPERATIONS, metrics);
            } else {
                UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_OPERATIONS, metrics);
                UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_CONSTRUCTION, metrics);
            }
            break;
        case UI_PANEL_RIGHT_TAB_OBJECT:
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS, metrics);
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_PRISM, metrics);
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_GIZMO, metrics);
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_TRANSFORM, metrics);
            break;
        case UI_PANEL_RIGHT_TAB_EDIT:
            UIPanel_RightControlsLayoutGroup(ui, UI_PANEL_GROUP_RIGHT_EDIT_SELECT, metrics);
            break;
        default:
            break;
    }
}
