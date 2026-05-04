#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_overlay_render.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"
#include "Tools/shape_from_layout.h"
#include "Tools/shape_export.h"
#include "ShapeLib/shape_json.h"
#include "Render/vulkan_adapter.h"
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

static UIPanelState g_uiPanel;  // Internal static state

UIPanelState* UIPanel_Get(void) {
    return &g_uiPanel;
}

static void AddButton(UIPanelState* ui,
                      const char* label,
                      int x,
                      int y,
                      int w,
                      int h,
                      UIPanelSide side,
                      UIPanelGroup group,
                      int id) {
    if (ui->count >= MAX_UI_BUTTONS) return;

    UIButton* b = &ui->buttons[ui->count++];
    strncpy(b->label, label, sizeof(b->label) - 1);
    b->label[sizeof(b->label) - 1] = '\0';
    b->bounds = (SDL_Rect){ x, y, w, h };
    b->side = side;
    b->group = group;
    b->id = id;
    b->hovered = false;
    b->pressed = false;
}

static SDL_Rect UIPanel_CoreRectToSDLRect(CorePaneRect rect) {
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

static bool UIPanel_QueryPaneRect(LineDrawingPaneRole role, SDL_Rect* out_rect) {
    CorePaneRect pane_rect = {0};
    const LineDrawingPaneHost* pane_host = NULL;
    if (!out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};

    pane_host = Global_GetPaneHostConst();
    if (!pane_host || !pane_host->initialized) return false;
    if (!LineDrawingPaneHost_GetRectForRole(pane_host, role, &pane_rect)) return false;

    *out_rect = UIPanel_CoreRectToSDLRect(pane_rect);
    return out_rect->w > 0 && out_rect->h > 0;
}

static void UIPanel_ResolveButtonLaneLayout(int screenW,
                                            int screenH,
                                            UIPanelSide side,
                                            int default_x,
                                            int default_y,
                                            int default_w,
                                            int* out_x,
                                            int* out_y,
                                            int* out_w) {
    SDL_Rect pane = {0, 0, 0, 0};
    const int padding = 10;
    int x = default_x;
    int y = default_y;
    int w = default_w;
    bool has_pane = false;
    (void)screenH;

    if (side == UI_PANEL_LEFT) {
        has_pane = UIPanel_QueryPaneRect(LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS, &pane);
    } else {
        has_pane = UIPanel_QueryPaneRect(LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &pane);
    }

    if (has_pane) {
        const int available = pane.w - padding * 2;
        int clamped_w = default_w;
        if (available > 0 && available < clamped_w) {
            clamped_w = available;
        }
        if (clamped_w < 72) {
            clamped_w = available > 0 ? available : 72;
        }
        if (clamped_w < 24) clamped_w = 24;
        w = clamped_w;
        y = pane.y + padding;
        x = pane.x + (pane.w - w) / 2;
    } else if (side == UI_PANEL_RIGHT) {
        x = screenW - default_w - padding;
    }

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = w;
}

typedef struct UIPanelCompactRowSpec {
    int row_key;
    int columns;
    int column_index;
} UIPanelCompactRowSpec;

static bool UIPanel_GetCompactRowSpec(int button_id, UIPanelCompactRowSpec* out_spec) {
    UIPanelCompactRowSpec spec = {0, 0, 0};
    switch (button_id) {
        case UI_BTN_RESET_ORIGIN: spec = (UIPanelCompactRowSpec){ 1, 3, 0 }; break;
        case UI_BTN_ZOOM_IN: spec = (UIPanelCompactRowSpec){ 1, 3, 1 }; break;
        case UI_BTN_ZOOM_OUT: spec = (UIPanelCompactRowSpec){ 1, 3, 2 }; break;

        case UI_BTN_CREATE_PLANE: spec = (UIPanelCompactRowSpec){ 2, 2, 0 }; break;
        case UI_BTN_CREATE_RECT_PRISM: spec = (UIPanelCompactRowSpec){ 2, 2, 1 }; break;

        case UI_BTN_EDIT_PRISM_WIDTH: spec = (UIPanelCompactRowSpec){ 3, 4, 0 }; break;
        case UI_BTN_EDIT_PRISM_HEIGHT: spec = (UIPanelCompactRowSpec){ 3, 4, 1 }; break;
        case UI_BTN_EDIT_PRISM_DEPTH: spec = (UIPanelCompactRowSpec){ 3, 4, 2 }; break;
        case UI_BTN_CYCLE_DISPLAY_UNITS: spec = (UIPanelCompactRowSpec){ 3, 4, 3 }; break;

        case UI_BTN_SET_CONSTRUCTION_PLANE_XY: spec = (UIPanelCompactRowSpec){ 4, 3, 0 }; break;
        case UI_BTN_SET_CONSTRUCTION_PLANE_YZ: spec = (UIPanelCompactRowSpec){ 4, 3, 1 }; break;
        case UI_BTN_SET_CONSTRUCTION_PLANE_XZ: spec = (UIPanelCompactRowSpec){ 4, 3, 2 }; break;

        case UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_NEG: spec = (UIPanelCompactRowSpec){ 5, 3, 0 }; break;
        case UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_POS: spec = (UIPanelCompactRowSpec){ 5, 3, 1 }; break;
        case UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET: spec = (UIPanelCompactRowSpec){ 5, 3, 2 }; break;

        case UI_BTN_EDIT_OBJECT_ROTATION_X: spec = (UIPanelCompactRowSpec){ 6, 3, 0 }; break;
        case UI_BTN_EDIT_OBJECT_ROTATION_Y: spec = (UIPanelCompactRowSpec){ 6, 3, 1 }; break;
        case UI_BTN_EDIT_OBJECT_ROTATION_Z: spec = (UIPanelCompactRowSpec){ 6, 3, 2 }; break;
        default: return false;
    }
    if (out_spec) *out_spec = spec;
    return true;
}

static TTF_Font* UIPanel_GetLayoutFont(void) {
    return FontManager_GetUIPanelFont();
}

static int UIPanel_FontHeightPx(void) {
    TTF_Font* font = UIPanel_GetLayoutFont();
    int h = 14;
    if (font) {
        h = TTF_FontHeight(font);
    }
    if (h < 12) h = 12;
    return h;
}

static int UIPanel_MeasureTextWidthPx(const char* text) {
    TTF_Font* font = UIPanel_GetLayoutFont();
    int width = 0;
    if (!text || !text[0]) return 0;
    if (font && TTF_SizeUTF8(font, text, &width, NULL) == 0 && width > 0) {
        return width;
    }
    return (int)strlen(text) * 8;
}

const char* UIPanel_ViewPlaneAxisLabel(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "YZ";
        case VIEW_PLANE_XZ: return "XZ";
        case VIEW_PLANE_XY:
        default: return "XY";
    }
}

const char* UIPanel_ViewPlaneCoordinateLabel(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "x";
        case VIEW_PLANE_XZ: return "y";
        case VIEW_PLANE_XY:
        default: return "z";
    }
}

ViewPlane UIPanel_CurrentConstructionViewPlane(const GlobalState* state) {
    if (state && Layout_ConstructionPlane3D_IsValid(&state->layout.scene3d.constructionPlane)) {
        return Layout_ConstructionPlane3D_ToViewPlane(&state->layout.scene3d.constructionPlane);
    }
    if (state) {
        return state->activePlane;
    }
    return (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
}

static const char* UIPanel_ButtonLayoutLabel(const UIButton* btn, char* dynamic_label, size_t dynamic_label_size) {
    GlobalState* state = Global_Get();
    if (!btn) return "";
    if (!dynamic_label || dynamic_label_size == 0) return btn->label;

    switch (btn->id) {
        case UI_BTN_TOGGLE_SPACE_MODE: {
            const char* modeLabel = state ? Global_GetSpaceModeLabel(state->spaceMode) : "3D";
            snprintf(dynamic_label, dynamic_label_size, "%s (M)", modeLabel);
            return dynamic_label;
        }
        case UI_BTN_TOGGLE_OBJECT_GIZMO_MODE: {
            const bool rotateMode = state ? state->editor.object3DRotateMode : false;
            snprintf(dynamic_label, dynamic_label_size, "%s (X)", rotateMode ? "Rotate" : "Move");
            return dynamic_label;
        }
        case UI_BTN_TOGGLE_SCENE_BOUNDS: {
            const bool enabled = state ? state->layout.scene3d.bounds.enabled : false;
            snprintf(dynamic_label, dynamic_label_size, "Bounds: %s", enabled ? "On" : "Off");
            return dynamic_label;
        }
        case UI_BTN_TOGGLE_SCENE_BOUNDS_CLAMP: {
            const bool clampOnEdit = state ? state->layout.scene3d.bounds.clampOnEdit : false;
            snprintf(dynamic_label, dynamic_label_size, "Clamp: %s", clampOnEdit ? "On" : "Off");
            return dynamic_label;
        }
        case UI_BTN_CYCLE_DISPLAY_UNITS:
            snprintf(dynamic_label, dynamic_label_size, "%s", UIPanel_GetDisplayUnitSymbol());
            return dynamic_label;
        case UI_BTN_EDIT_PRISM_WIDTH:
            snprintf(dynamic_label, dynamic_label_size, "W");
            return dynamic_label;
        case UI_BTN_EDIT_PRISM_HEIGHT:
            snprintf(dynamic_label, dynamic_label_size, "H");
            return dynamic_label;
        case UI_BTN_EDIT_PRISM_DEPTH:
            snprintf(dynamic_label, dynamic_label_size, "D");
            return dynamic_label;
        case UI_BTN_SET_CONSTRUCTION_PLANE_XY:
        case UI_BTN_SET_CONSTRUCTION_PLANE_YZ:
        case UI_BTN_SET_CONSTRUCTION_PLANE_XZ: {
            const ViewPlane plane = UIPanel_CurrentConstructionViewPlane(state);
            const ViewPlaneAxis button_axis =
                (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_YZ) ? VIEW_PLANE_YZ :
                (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XZ) ? VIEW_PLANE_XZ :
                VIEW_PLANE_XY;
            snprintf(dynamic_label,
                     dynamic_label_size,
                     "%s%s%s",
                     (plane.axis == button_axis) ? "[" : "",
                     UIPanel_ViewPlaneAxisLabel(button_axis),
                     (plane.axis == button_axis) ? "]" : "");
            return dynamic_label;
        }
        case UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET: {
            const ViewPlane plane = UIPanel_CurrentConstructionViewPlane(state);
            snprintf(dynamic_label,
                     dynamic_label_size,
                     "Edit %s",
                     UIPanel_ViewPlaneCoordinateLabel(plane.axis));
            return dynamic_label;
        }
        default:
            return btn->label;
    }
}

static int UIPanel_CompactButtonWidthPx(const UIButton* btn, int text_pad_x) {
    char dynamic_label[64];
    int width = UIPanel_MeasureTextWidthPx(UIPanel_ButtonLayoutLabel(btn, dynamic_label, sizeof(dynamic_label)));
    width += text_pad_x * 2;
    if (width < 22) width = 22;
    return width;
}

static int UIPanel_MaxWidthForLabels(const char* const* labels, size_t count) {
    int max_width = 0;
    for (size_t i = 0; i < count; ++i) {
        int width = UIPanel_MeasureTextWidthPx(labels[i]);
        if (width > max_width) max_width = width;
    }
    return max_width;
}

void UIPanel_GetLayoutMetrics(UIPanelLayoutMetrics* out_metrics) {
    static const char* k_left_button_labels[] = {
        "Save JSON",
        "Load JSON",
        "Export Shape",
        "Export Scene",
        "Input Edit",
        "Input Folder",
        "Output Edit",
        "Output Folder"
    };
    static const char* k_left_group_titles[] = {
        "File / IO",
        "Root Paths"
    };
    static const char* k_right_button_labels[] = {
        "Toggle Delete (D)",
        "Pin Anchor (P)",
        "Link Handles (L)",
        "Mode: 3D (M)",
        "Gizmo: Rotate (X)",
        "Bounds: Off",
        "Clamp: Off",
        "Edit BMin",
        "Edit BMax"
    };
    static const char* k_right_group_titles[] = {
        "View",
        "Modes",
        "Primitives",
        "Construction",
        "Prism",
        "Gizmo",
        "Scene Bounds"
    };
    int font_h = 14;
    int text_pad_x = 9;
    int pane_padding = 8;
    int spacing = 5;
    int button_h = 26;
    int group_header_h = 14;
    int group_gap = 8;
    int compact_row_gap = 3;
    int overlay_h = 64;
    int left_button_w = 168;
    int right_button_w = 168;
    int left_label_w = 0;
    int right_label_w = 0;
    int left_title_w = 0;
    int right_title_w = 0;

    if (!out_metrics) return;
    memset(out_metrics, 0, sizeof(*out_metrics));

    font_h = UIPanel_FontHeightPx();
    text_pad_x = 5 + (font_h / 4);
    if (text_pad_x < 5) text_pad_x = 5;
    pane_padding = 4 + (font_h / 5);
    if (pane_padding < 5) pane_padding = 5;
    spacing = 2 + (font_h / 10);
    if (spacing < 3) spacing = 3;
    button_h = font_h + 4;
    if (button_h < 18) button_h = 18;
    group_header_h = font_h + 1;
    if (group_header_h < 13) group_header_h = 13;
    group_gap = 4 + (font_h / 7);
    if (group_gap < 5) group_gap = 5;
    compact_row_gap = 2 + (font_h / 10);
    if (compact_row_gap < 2) compact_row_gap = 2;
    overlay_h = InfoOverlay_HeightPx();
    if (overlay_h < 48) overlay_h = 48;

    left_label_w = UIPanel_MaxWidthForLabels(
        k_left_button_labels,
        sizeof(k_left_button_labels) / sizeof(k_left_button_labels[0]));
    right_label_w = UIPanel_MaxWidthForLabels(
        k_right_button_labels,
        sizeof(k_right_button_labels) / sizeof(k_right_button_labels[0]));
    left_title_w = UIPanel_MaxWidthForLabels(
        k_left_group_titles,
        sizeof(k_left_group_titles) / sizeof(k_left_group_titles[0]));
    right_title_w = UIPanel_MaxWidthForLabels(
        k_right_group_titles,
        sizeof(k_right_group_titles) / sizeof(k_right_group_titles[0]));

    left_button_w = left_label_w + (text_pad_x * 2);
    right_button_w = right_label_w + (text_pad_x * 2);
    if (left_button_w < left_title_w + text_pad_x) left_button_w = left_title_w + text_pad_x;
    if (right_button_w < right_title_w + text_pad_x) right_button_w = right_title_w + text_pad_x;
    if (left_button_w < 84) left_button_w = 84;
    if (right_button_w < 96) right_button_w = 96;

    out_metrics->button_text_pad_px = text_pad_x;
    out_metrics->overlay_height_px = overlay_h;
    out_metrics->top_offset_px = overlay_h + pane_padding;
    out_metrics->pane_padding_px = pane_padding;
    out_metrics->button_spacing_px = spacing;
    out_metrics->button_height_px = button_h;
    out_metrics->group_header_height_px = group_header_h;
    out_metrics->group_gap_px = group_gap;
    out_metrics->compact_row_gap_px = compact_row_gap;
    out_metrics->left_button_width_px = left_button_w;
    out_metrics->right_button_width_px = right_button_w;
    out_metrics->desired_top_pane_height_px = overlay_h + 1;
    out_metrics->desired_left_pane_width_px = left_button_w + (pane_padding * 2);
    out_metrics->desired_right_pane_width_px = right_button_w + (pane_padding * 2);
}

void UIPanel_ExportShape(void) {
    GlobalState* state = Global_Get();
    const char* output_root = NULL;
    if (!state) return;

    const char* requested = Global_GetCurrentConfigPath();
    if (!requested || !*requested) {
        requested = "layout_export.json";
    }
    output_root = Global_GetOutputRoot();

    char exportPath[SHAPE_EXPORT_PATH_MAX];
    if (!ShapeExport_BuildPathInRoot(output_root ? output_root : ShapeExport_GetExportDir(),
                                     requested,
                                     exportPath,
                                     sizeof(exportPath))) {
        SDL_Log("[UI] Export failed: unable to prepare export path for '%s'", requested);
        return;
    }

    Layout_CompactDeletedElements(&state->layout);

    ShapeDocument doc;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlaneAxis exportAxis = SpaceAdapter_ActivePlaneAxis(&viewCtx);
    if (!ShapeDocument_FromLayoutProjected(requested, &state->layout, exportAxis, &doc)) {
        SDL_Log("[UI] Export failed: could not build shape data.");
        return;
    }

    if (ShapeDocument_SaveToJsonFile(&doc, exportPath)) {
        SDL_Log("[UI] Exported Shape JSON to %s", exportPath);
    } else {
        SDL_Log("[UI] Export failed: could not write %s", exportPath);
    }

    ShapeDocument_Free(&doc);
}

static PlaneFrame3 UIPanel_FrameFromConstructionPlane(const GlobalState* state) {
    PlaneFrame3 frame = {0};
    if (!state) return frame;

    const ConstructionPlane3D* cp = &state->layout.scene3d.constructionPlane;
    if (!Layout_ConstructionPlane3D_IsValid(cp)) {
        Plane3 fallbackPlane = Plane3_FromViewPlane((ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f });
        return PlaneFrame3_FromPlane(fallbackPlane, (Vec3){ 0.0f, 0.0f, 0.0f });
    }

    if (cp->mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME) {
        frame = cp->customFrame;
    } else {
        Plane3 p = Plane3_FromViewPlane(cp->axisAligned);
        Vec3 origin = { 0.0f, 0.0f, 0.0f };
        switch (cp->axisAligned.axis) {
            case VIEW_PLANE_YZ: origin.x = cp->axisAligned.offset; break;
            case VIEW_PLANE_XZ: origin.y = cp->axisAligned.offset; break;
            case VIEW_PLANE_XY:
            default: origin.z = cp->axisAligned.offset; break;
        }
        frame = PlaneFrame3_FromPlane(p, origin);
        frame.origin = origin;
    }
    return frame;
}

static Vec3 UIPanel_ResolvePlaneSpawnOrigin(const GlobalState* state,
                                            const SpaceViewContext* viewCtx,
                                            PlaneFrame3 frame) {
    Vec3 origin = frame.origin;
    if (!state || !viewCtx) return origin;

    Vec3 candidate = origin;
    const int cx = state->screenWidth / 2;
    const int cy = state->screenHeight / 2;
    if (SpaceAdapter_ScreenToWorld(cx, cy, &state->grid, viewCtx, false, &candidate)) {
        // candidate from screen center.
    } else {
        candidate = viewCtx->camera.target;
    }

    if (state->layout.scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED) {
        switch (state->layout.scene3d.constructionPlane.axisAligned.axis) {
            case VIEW_PLANE_YZ:
                candidate.x = state->layout.scene3d.constructionPlane.axisAligned.offset;
                break;
            case VIEW_PLANE_XZ:
                candidate.y = state->layout.scene3d.constructionPlane.axisAligned.offset;
                break;
            case VIEW_PLANE_XY:
            default:
                candidate.z = state->layout.scene3d.constructionPlane.axisAligned.offset;
                break;
        }
        return candidate;
    }

    {
        Plane3 plane = Plane3_FromPointNormal(frame.origin, frame.normal);
        return Plane3_ProjectPoint(plane, candidate);
    }
}

bool UIPanel_CreatePlanePrimitiveFromActiveContext(bool disable_bounds_lock) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        SDL_Log("[UI] Plane creation blocked: SPACE_MODE_3D required.");
        return false;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    PlaneFrame3 frame = UIPanel_FrameFromConstructionPlane(state);
    frame.origin = UIPanel_ResolvePlaneSpawnOrigin(state, &viewCtx, frame);

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = fmaxf(state->grid.gridSize * 4.0f, 1.0f);
    params.height = fmaxf(state->grid.gridSize * 4.0f, 1.0f);
    params.lockToBounds = !disable_bounds_lock;
    params.useExplicitFrame = true;
    params.explicitFrame = frame;

    Editor_HistoryCapture(&state->editor, &state->layout);
    uint32_t objectId = 0u;
    bool boundsAdjusted = false;
    if (!Layout_CreatePlanePrimitive(&state->layout, &params, &objectId, &boundsAdjusted)) {
        SDL_Log("[UI] Plane primitive creation failed.");
        return false;
    }

    Editor_ClearAnchorSelection(&state->editor);
    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    state->editor.selectedWallIndex = -1;
    state->editor.selectedHandleAnchor = -1;
    state->editor.selectedHandleComponent = -1;
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Plane primitive created (id=%u%s)",
            objectId,
            boundsAdjusted ? ", bounds-adjusted" : "");
    return true;
}

bool UIPanel_CreateRectPrismPrimitiveFromActiveContext(bool disable_bounds_lock) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        SDL_Log("[UI] Rect prism creation blocked: SPACE_MODE_3D required.");
        return false;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    PlaneFrame3 frame = UIPanel_FrameFromConstructionPlane(state);
    frame.origin = UIPanel_ResolvePlaneSpawnOrigin(state, &viewCtx, frame);

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = fmaxf(state->grid.gridSize * 4.0f, 1.0f);
    params.height = fmaxf(state->grid.gridSize * 4.0f, 1.0f);
    params.depth = fmaxf(state->grid.gridSize * 4.0f, 1.0f);
    params.lockToBounds = !disable_bounds_lock;
    params.useExplicitFrame = true;
    params.explicitFrame = frame;

    Editor_HistoryCapture(&state->editor, &state->layout);
    uint32_t objectId = 0u;
    bool boundsAdjusted = false;
    if (!Layout_CreateRectPrismPrimitive(&state->layout, &params, &objectId, &boundsAdjusted)) {
        SDL_Log("[UI] Rect prism primitive creation failed.");
        return false;
    }

    Editor_ClearAnchorSelection(&state->editor);
    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    state->editor.selectedWallIndex = -1;
    state->editor.selectedHandleAnchor = -1;
    state->editor.selectedHandleComponent = -1;
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Rect prism primitive created (id=%u%s)",
            objectId,
            boundsAdjusted ? ", bounds-adjusted" : "");
    return true;
}

void UIPanel_OnWindowResized(int screenW, int screenH) {
    UIPanelLayoutMetrics metrics;
    int padding = 10;
    int leftBtnW = 168;
    int rightBtnW = 168;
    int btnH = 26;
    int spacing = 5;
    int compactRowGap = 4;
    int groupHeaderHeight = 14;
    int groupGap = 10;
    int textPadX = 9;
    int topOffset = 74;
    int leftX = padding;
    int leftY = topOffset;
    int leftW = leftBtnW;
    int rightX = screenW - rightBtnW - padding;
    int rightY = topOffset;
    int rightW = rightBtnW;
    UIPanelGroup leftGroup = UI_PANEL_GROUP_NONE;
    UIPanelGroup rightGroup = UI_PANEL_GROUP_NONE;

    UIPanel_GetLayoutMetrics(&metrics);
    padding = metrics.pane_padding_px;
    leftBtnW = metrics.left_button_width_px;
    rightBtnW = metrics.right_button_width_px;
    btnH = metrics.button_height_px;
    spacing = metrics.button_spacing_px;
    compactRowGap = metrics.compact_row_gap_px;
    groupHeaderHeight = metrics.group_header_height_px;
    groupGap = metrics.group_gap_px;
    textPadX = metrics.button_text_pad_px;
    topOffset = metrics.top_offset_px;
    leftX = padding;
    leftY = topOffset;
    leftW = leftBtnW;
    rightX = screenW - rightBtnW - padding;
    rightY = topOffset;
    rightW = rightBtnW;

    UIPanel_ResolveButtonLaneLayout(screenW,
                                    screenH,
                                    UI_PANEL_LEFT,
                                    leftX,
                                    leftY,
                                    leftBtnW,
                                    &leftX,
                                    &leftY,
                                    &leftW);
    UIPanel_ResolveButtonLaneLayout(screenW,
                                    screenH,
                                    UI_PANEL_RIGHT,
                                    rightX,
                                    rightY,
                                    rightBtnW,
                                    &rightX,
                                    &rightY,
                                    &rightW);

    for (int i = 0; i < g_uiPanel.count; ++i) {
        UIButton* btn = &g_uiPanel.buttons[i];
        if (btn->side == UI_PANEL_LEFT) {
            if (btn->group != leftGroup) {
                if (leftGroup != UI_PANEL_GROUP_NONE) leftY += groupGap;
                leftY += groupHeaderHeight;
                leftGroup = btn->group;
            }
            btn->bounds = (SDL_Rect){ leftX, leftY, leftW, btnH };
            leftY += btnH + spacing;
        }
    }

    for (int i = 0; i < g_uiPanel.count; ++i) {
        UIButton* btn = &g_uiPanel.buttons[i];
        UIPanelCompactRowSpec rowSpec = {0, 0, 0};
        if (btn->side != UI_PANEL_RIGHT) continue;
        if (btn->group != rightGroup) {
            if (rightGroup != UI_PANEL_GROUP_NONE) rightY += groupGap;
            rightY += groupHeaderHeight;
            rightGroup = btn->group;
        }

        if (UIPanel_GetCompactRowSpec(btn->id, &rowSpec)) {
            int rowStart = i;
            int rowEnd = i;
            int rowWidths[4] = {0, 0, 0, 0};
            int rowTotalWidth = 0;
            int rowStartX = rightX;

            while ((rowEnd + 1) < g_uiPanel.count) {
                UIPanelCompactRowSpec nextSpec = {0, 0, 0};
                UIButton* next = &g_uiPanel.buttons[rowEnd + 1];
                if (next->side != UI_PANEL_RIGHT || next->group != btn->group) break;
                if (!UIPanel_GetCompactRowSpec(next->id, &nextSpec)) break;
                if (nextSpec.row_key != rowSpec.row_key) break;
                rowEnd++;
            }

            for (int j = rowStart; j <= rowEnd; ++j) {
                UIButton* place = &g_uiPanel.buttons[j];
                UIPanelCompactRowSpec placeSpec = {0, 0, 0};
                int col = 0;
                int cellWidth = 22;
                if (UIPanel_GetCompactRowSpec(place->id, &placeSpec)) {
                    col = placeSpec.column_index;
                    if (col < 0) col = 0;
                    if (col >= 4) col = 3;
                }
                cellWidth = UIPanel_CompactButtonWidthPx(place, textPadX);
                if (cellWidth > rightW) cellWidth = rightW;
                rowWidths[col] = cellWidth;
            }

            for (int col = 0; col < rowSpec.columns && col < 4; ++col) {
                if (col > 0) rowTotalWidth += compactRowGap;
                rowTotalWidth += rowWidths[col];
            }
            if (rowTotalWidth < rightW) {
                rowStartX = rightX + (rightW - rowTotalWidth) / 2;
            }

            for (int j = rowStart; j <= rowEnd; ++j) {
                UIPanelCompactRowSpec placeSpec = {0, 0, 0};
                UIButton* place = &g_uiPanel.buttons[j];
                int col = 0;
                int x = rowStartX;
                if (UIPanel_GetCompactRowSpec(place->id, &placeSpec)) {
                    col = placeSpec.column_index;
                    if (col < 0) col = 0;
                    if (col >= rowSpec.columns) col = rowSpec.columns - 1;
                }
                for (int prior = 0; prior < col; ++prior) {
                    x += rowWidths[prior] + compactRowGap;
                }
                place->bounds = (SDL_Rect){
                    x,
                    rightY,
                    rowWidths[col],
                    btnH
                };
            }
            rightY += btnH + spacing;
            i = rowEnd;
            continue;
        }

        btn->bounds = (SDL_Rect){ rightX, rightY, rightW, btnH };
        rightY += btnH + spacing;
    }
}

void UIPanel_Init(int screenW, int screenH) {
    g_uiPanel.count = 0;
    g_uiPanel.saveDialog.active = false;
    g_uiPanel.saveDialog.buffer[0] = '\0';
    g_uiPanel.saveDialog.length = 0;
    g_uiPanel.saveDialog.cursor = 0;
    g_uiPanel.loadMenu.open = false;
    g_uiPanel.loadMenu.count = 0;
    g_uiPanel.loadMenu.hoverIndex = -1;
    g_uiPanel.rootDialog.active = false;
    g_uiPanel.rootDialog.target = UI_ROOT_TARGET_NONE;
    g_uiPanel.rootDialog.buffer[0] = '\0';
    g_uiPanel.rootDialog.length = 0;
    g_uiPanel.rootDialog.cursor = 0;
    g_uiPanel.prismDimensionDialog.active = false;
    g_uiPanel.prismDimensionDialog.target = UI_PRISM_DIMENSION_TARGET_NONE;
    g_uiPanel.prismDimensionDialog.objectId = 0u;
    g_uiPanel.prismDimensionDialog.buffer[0] = '\0';
    g_uiPanel.prismDimensionDialog.length = 0;
    g_uiPanel.prismDimensionDialog.cursor = 0;
    g_uiPanel.sceneBoundsDialog.active = false;
    g_uiPanel.sceneBoundsDialog.target = UI_SCENE_BOUNDS_TARGET_NONE;
    g_uiPanel.sceneBoundsDialog.buffer[0] = '\0';
    g_uiPanel.sceneBoundsDialog.length = 0;
    g_uiPanel.sceneBoundsDialog.cursor = 0;
    g_uiPanel.constructionPlaneDialog.active = false;
    g_uiPanel.constructionPlaneDialog.target = UI_CONSTRUCTION_PLANE_DIALOG_TARGET_NONE;
    g_uiPanel.constructionPlaneDialog.buffer[0] = '\0';
    g_uiPanel.constructionPlaneDialog.length = 0;
    g_uiPanel.constructionPlaneDialog.cursor = 0;
    g_uiPanel.objectTransformDialog.active = false;
    g_uiPanel.objectTransformDialog.target = UI_OBJECT_TRANSFORM_DIALOG_TARGET_NONE;
    g_uiPanel.objectTransformDialog.objectId = 0u;
    g_uiPanel.objectTransformDialog.buffer[0] = '\0';
    g_uiPanel.objectTransformDialog.length = 0;
    g_uiPanel.objectTransformDialog.cursor = 0;
    g_uiPanel.displayUnit = CORE_UNIT_FOOT;

    UIPanelLayoutMetrics metrics;
    int padding = 10;
    int leftBtnW = 168;
    int rightBtnW = 168;
    int btnH = 26;
    int spacing = 5;
    int topOffset = 74;

    UIPanel_GetLayoutMetrics(&metrics);
    padding = metrics.pane_padding_px;
    leftBtnW = metrics.left_button_width_px;
    rightBtnW = metrics.right_button_width_px;
    btnH = metrics.button_height_px;
    spacing = metrics.button_spacing_px;
    topOffset = metrics.top_offset_px;

    int xL = padding;
    int yL = topOffset;
    AddButton(&g_uiPanel, "Save JSON", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_SAVE_JSON);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Load JSON", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_LOAD_JSON);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Export Shape", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_EXPORT_SHAPE);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Export Scene", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_EXPORT_SCENE);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Input Edit", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_INPUT_ROOT_EDIT);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Input Folder", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_INPUT_ROOT_FOLDER);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Output Edit", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_OUTPUT_ROOT_EDIT);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Output Folder", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_OUTPUT_ROOT_FOLDER);

    int xR = screenW - rightBtnW - padding;
    int yR = topOffset;
    AddButton(&g_uiPanel, "O", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_VIEW, UI_BTN_RESET_ORIGIN);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "+", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_VIEW, UI_BTN_ZOOM_IN);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "-", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_VIEW, UI_BTN_ZOOM_OUT);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Toggle Delete (D)", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_MODES, UI_BTN_TOGGLE_DELETE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Pin Anchor (P)", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_MODES, UI_BTN_PIN_ANCHOR);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Link Handles (L)", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_MODES, UI_BTN_LINK_HANDLES);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Mode: 3D (M)", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_MODES, UI_BTN_TOGGLE_SPACE_MODE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "+Plane", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRIMITIVES, UI_BTN_CREATE_PLANE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "+Prism", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRIMITIVES, UI_BTN_CREATE_RECT_PRISM);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "XY", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_CONSTRUCTION, UI_BTN_SET_CONSTRUCTION_PLANE_XY);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "YZ", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_CONSTRUCTION, UI_BTN_SET_CONSTRUCTION_PLANE_YZ);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "XZ", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_CONSTRUCTION, UI_BTN_SET_CONSTRUCTION_PLANE_XZ);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "-", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_CONSTRUCTION, UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_NEG);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "+", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_CONSTRUCTION, UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_POS);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edit Off", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_CONSTRUCTION, UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "W", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRISM, UI_BTN_EDIT_PRISM_WIDTH);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "H", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRISM, UI_BTN_EDIT_PRISM_HEIGHT);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "D", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRISM, UI_BTN_EDIT_PRISM_DEPTH);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "ft", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRISM, UI_BTN_CYCLE_DISPLAY_UNITS);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Gizmo: Move (X)", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_GIZMO, UI_BTN_TOGGLE_OBJECT_GIZMO_MODE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edit Pos", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_POSITION);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Rot X", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_ROTATION_X);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Rot Y", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_ROTATION_Y);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Rot Z", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_ROTATION_Z);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Bounds: Off", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_BOUNDS, UI_BTN_TOGGLE_SCENE_BOUNDS);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Clamp: Off", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_BOUNDS, UI_BTN_TOGGLE_SCENE_BOUNDS_CLAMP);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edit BMin", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_BOUNDS, UI_BTN_EDIT_SCENE_BOUNDS_MIN);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edit BMax", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_BOUNDS, UI_BTN_EDIT_SCENE_BOUNDS_MAX);

    UIPanel_OnWindowResized(screenW, screenH);
    UIPanel_RefreshConfigList();
}

const UIButton* UIPanel_GetButtons(UIPanelState* ui, int* outCount) {
    if (outCount) *outCount = ui->count;
    return ui->buttons;
}

bool UIPanel_IsSaveDialogActive(void) {
    return g_uiPanel.saveDialog.active;
}

bool UIPanel_IsRootDialogActive(void) {
    return g_uiPanel.rootDialog.active;
}

bool UIPanel_IsPrismDimensionDialogActive(void) {
    return g_uiPanel.prismDimensionDialog.active;
}

bool UIPanel_IsSceneBoundsDialogActive(void) {
    return g_uiPanel.sceneBoundsDialog.active;
}

bool UIPanel_IsConstructionPlaneDialogActive(void) {
    return g_uiPanel.constructionPlaneDialog.active;
}

bool UIPanel_IsObjectTransformDialogActive(void) {
    return g_uiPanel.objectTransformDialog.active;
}

bool UIPanel_IsCapturingKeyboard(void) {
    return UIPanel_IsSaveDialogActive() ||
           UIPanel_IsRootDialogActive() ||
           UIPanel_IsPrismDimensionDialogActive() ||
           UIPanel_IsSceneBoundsDialogActive() ||
           UIPanel_IsConstructionPlaneDialogActive() ||
           UIPanel_IsObjectTransformDialogActive();
}

void UIPanel_RenderOverlays(SDL_Renderer* renderer) {
    UIPanel_RenderOverlayDialogs(renderer, &g_uiPanel);
}
