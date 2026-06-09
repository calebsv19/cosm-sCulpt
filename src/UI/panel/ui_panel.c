#include "UI/ui_panel_create_summary.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_file_summary.h"
#include "UI/ui_panel_file_controls.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/ui_panel_create_layout.h"
#include "UI/ui_panel_edit_layout.h"
#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_object_inspector.h"
#include "UI/ui_panel_object_layout.h"
#include "UI/ui_panel_right_controls.h"
#include "UI/ui_panel_scene_layout.h"
#include "UI/ui_panel_scene_summary.h"
#include "UI/ui_panel_overlay_render.h"
#include "UI/panel/ui_panel_object_workspace_layout.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_view_layout.h"
#include "UI/ui_panel_view_summary.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"
#include "Editor/primitive_placement_preview.h"
#include "ObjectAuthoring/object_authoring_session.h"
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

static void UIPanel_SyncObjectAuthoringAfterPrimitiveCreate(GlobalState* state,
                                                            uint32_t object_id) {
    if (!state || object_id == 0u) return;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    if (!ObjectAuthoringSession_MirrorBodiesFromLayout(&state->objectAuthoring,
                                                       &state->layout)) {
        SDL_Log("[UI] Object authoring sync failed after primitive creation.");
        return;
    }
    (void)ObjectAuthoringSession_SetSelection(&state->objectAuthoring,
                                              object_id,
                                              OBJECT3D_FACE_NONE);
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
        "Clear Select",
        "Delete Obj",
        "Save JSON",
        "Load JSON",
        "Load Scene",
        "Mesh Assets",
        "Export Shape",
        "Export Scene",
        "Session In Edit",
        "Session In Pick",
        "Output Edit",
        "Output Pick"
    };
    static const char* k_left_group_titles[] = {
        "Selection",
        "Scene Bounds",
        "File / IO",
        "Session Paths",
        "Asset IO",
        "Asset Paths"
    };
    static const char* k_left_tab_labels[] = {
        "Scene",
        "File",
        "Model",
        "Assets"
    };
    static const char* k_right_button_labels[] = {
        "Toggle Delete (D)",
        "Pin Anchor (P)",
        "Link Handles (L)",
        "Mode: 3D (M)",
        "Face Select",
        "Sketch Rect",
        "Sketch Select",
        "Clear Sketch",
        "Place Mesh",
        "Cut Prism",
        "Gizmo: Mode (X)",
        "Clear Select",
        "Delete Object",
        "Extrude +",
        "Cut Prism",
        "Save Asset",
        "Load Asset",
        "New Asset",
        "Bounds: Off",
        "Clamp: Off",
        "Edit BMin",
        "Edit BMax",
        "Body",
        "Face",
        "Edge",
        "Vertex"
    };
    static const char* k_right_group_titles[] = {
        "View",
        "Modes",
        "Primitives",
        "Construction",
        "Prism",
        "Dimensions",
        "Gizmo",
        "Transform",
        "Object Actions",
        "Target / Sketch",
        "Solid Command",
        "Selection Actions",
        "Selection Mode"
    };
    static const char* k_right_tab_labels[] = {
        "View",
        "Create",
        "Object",
        "Tools",
        "Properties",
        "Edit"
    };
    int font_h = 14;
    int text_pad_x = 9;
    int pane_padding = 8;
    int spacing = 5;
    int button_h = 26;
    int group_header_h = 14;
    int group_gap = 8;
    int compact_row_gap = 3;
    int tab_height = 24;
    int overlay_h = 64;
    int left_button_w = 168;
    int right_button_w = 168;
    int left_label_w = 0;
    int right_label_w = 0;
    int left_title_w = 0;
    int right_title_w = 0;
    int left_tab_w = 0;
    int right_tab_w = 0;

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
    tab_height = button_h;
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
    left_tab_w = UIPanel_MaxWidthForLabels(
        k_left_tab_labels,
        sizeof(k_left_tab_labels) / sizeof(k_left_tab_labels[0]));
    right_tab_w = UIPanel_MaxWidthForLabels(
        k_right_tab_labels,
        sizeof(k_right_tab_labels) / sizeof(k_right_tab_labels[0]));

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
    out_metrics->tab_height_px = tab_height;
    out_metrics->left_button_width_px = left_button_w;
    out_metrics->right_button_width_px = right_button_w;
    out_metrics->desired_top_pane_height_px = overlay_h + 1;
    {
        int left_tab_total = (left_tab_w + (text_pad_x * 2) + 6) * UI_PANEL_LEFT_TAB_COUNT;
        left_tab_total += spacing * (UI_PANEL_LEFT_TAB_COUNT - 1);
        int right_tab_total = (right_tab_w + (text_pad_x * 2) + 10) * UI_PANEL_RIGHT_TAB_COUNT;
        right_tab_total += spacing * (UI_PANEL_RIGHT_TAB_COUNT - 1);
        int desired_left = left_button_w + (pane_padding * 2);
        int desired_right = right_button_w + (pane_padding * 2);
        if (desired_left < left_tab_total + (pane_padding * 2)) {
            desired_left = left_tab_total + (pane_padding * 2);
        }
        if (desired_right < right_tab_total + (pane_padding * 2)) {
            desired_right = right_tab_total + (pane_padding * 2);
        }
        if (desired_left < 190) desired_left = 190;
        if (desired_right < 170) desired_right = 170;
        out_metrics->desired_left_pane_width_px = desired_left;
        out_metrics->desired_right_pane_width_px = desired_right;
    }
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

bool UIPanel_CreatePlanePrimitiveFromActiveContext(bool disable_bounds_lock) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        SDL_Log("[UI] Plane creation blocked: SPACE_MODE_3D required.");
        return false;
    }

    PrimitivePlacementPreview preview = {0};
    if (!Editor_PrimitivePlacementPreview_Build(state,
                                                PRIMITIVE_PLACEMENT_PREVIEW_PLANE,
                                                &preview)) {
        SDL_Log("[UI] Plane primitive creation failed: no valid placement preview.");
        return false;
    }

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = preview.width;
    params.height = preview.height;
    params.lockToBounds = !disable_bounds_lock;
    params.useExplicitFrame = true;
    params.explicitFrame = preview.frame;

    Editor_HistoryCapture(&state->editor, &state->layout);
    uint32_t objectId = 0u;
    bool boundsAdjusted = false;
    if (!Layout_CreatePlanePrimitive(&state->layout, &params, &objectId, &boundsAdjusted)) {
        SDL_Log("[UI] Plane primitive creation failed.");
        return false;
    }

    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    Editor_ClearAnchorSelection(&state->editor);
    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObjectAssetBodyId = object_mode ? objectId : 0u;
    state->editor.selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    state->editor.selectedObject3DResizeHandle =
        object_mode ? PLANE_RESIZE_HANDLE_NONE : PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    state->editor.objectAuthoringMode = object_mode
        ? OBJECT_AUTHORING_MODE_NONE
        : state->editor.objectAuthoringMode;
    state->editor.primitivePlacementPreview = PRIMITIVE_PLACEMENT_PREVIEW_NONE;
    state->editor.selectedWallIndex = -1;
    state->editor.selectedHandleAnchor = -1;
    state->editor.selectedHandleComponent = -1;
    UIPanel_SyncObjectAuthoringAfterPrimitiveCreate(state, objectId);
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

    PrimitivePlacementPreview preview = {0};
    if (!Editor_PrimitivePlacementPreview_Build(state,
                                                PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM,
                                                &preview)) {
        SDL_Log("[UI] Rect prism primitive creation failed: no valid placement preview.");
        return false;
    }

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = preview.width;
    params.height = preview.height;
    params.depth = preview.depth;
    params.lockToBounds = !disable_bounds_lock;
    params.useExplicitFrame = true;
    params.explicitFrame = preview.frame;

    Editor_HistoryCapture(&state->editor, &state->layout);
    uint32_t objectId = 0u;
    bool boundsAdjusted = false;
    if (!Layout_CreateRectPrismPrimitive(&state->layout, &params, &objectId, &boundsAdjusted)) {
        SDL_Log("[UI] Rect prism primitive creation failed.");
        return false;
    }

    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    Editor_ClearAnchorSelection(&state->editor);
    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObjectAssetBodyId = object_mode ? objectId : 0u;
    state->editor.selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle =
        object_mode ? RECT_PRISM_RESIZE_HANDLE_NONE : RECT_PRISM_RESIZE_HANDLE_CORNER_6;
    state->editor.objectAuthoringMode = object_mode
        ? OBJECT_AUTHORING_MODE_NONE
        : state->editor.objectAuthoringMode;
    state->editor.primitivePlacementPreview = PRIMITIVE_PLACEMENT_PREVIEW_NONE;
    state->editor.selectedWallIndex = -1;
    state->editor.selectedHandleAnchor = -1;
    state->editor.selectedHandleComponent = -1;
    UIPanel_SyncObjectAuthoringAfterPrimitiveCreate(state, objectId);
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Rect prism primitive created (id=%u%s)",
            objectId,
            boundsAdjusted ? ", bounds-adjusted" : "");
    return true;
}

bool UIPanel_FitSceneBoundsToSelectedObject(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;

    const uint32_t objectId = state->editor.selectedObject3DId;
    const Object3D* object = Layout_ObjectStore_FindConst(&state->layout.objectStore, objectId);
    Vec3 objectMin = {0};
    Vec3 objectMax = {0};
    if (!Layout_Object3D_ComputeWorldAABB(object, &objectMin, &objectMax)) {
        SDL_Log("[UI] Fit scene bounds blocked: select a plane or prism first.");
        return false;
    }

    Editor_HistoryCapture(&state->editor, &state->layout);
    if (!Layout_FitSceneBounds3DToObject(&state->layout, objectId, state->grid.gridSize)) {
        SDL_Log("[UI] Fit scene bounds failed for object id=%u.", objectId);
        return false;
    }

    state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    state->editor.hoveredSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    state->editor.hoveredSceneBoundsGizmoAxis = -1;
    state->editor.activeSceneBoundsGizmoAxis = -1;
    state->editor.isResizingSceneBounds = false;
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Scene bounds fit to selected object id=%u.", objectId);
    return true;
}

void UIPanel_OnWindowResized(int screenW, int screenH) {
    UIPanelLayoutMetrics metrics;
    int padding = 10;
    int leftBtnW = 168;
    int rightBtnW = 168;
    int btnH = 26;
    int spacing = 5;
    int groupHeaderHeight = 14;
    int groupGap = 10;
    int topOffset = 74;
    int leftX = padding;
    int leftY = topOffset;
    int leftW = leftBtnW;
    int rightX = screenW - rightBtnW - padding;
    int rightY = topOffset;
    int rightW = rightBtnW;
    SDL_Rect leftPaneRect = {0, 0, 0, 0};
    SDL_Rect rightPaneRect = {0, 0, 0, 0};
    bool hasLeftPane = false;
    bool hasRightPane = false;
    UIPanelGroup leftGroup = UI_PANEL_GROUP_NONE;

    UIPanel_GetLayoutMetrics(&metrics);
    padding = metrics.pane_padding_px;
    leftBtnW = metrics.left_button_width_px;
    rightBtnW = metrics.right_button_width_px;
    btnH = metrics.button_height_px;
    spacing = metrics.button_spacing_px;
    groupHeaderHeight = metrics.group_header_height_px;
    groupGap = metrics.group_gap_px;
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

    hasLeftPane = UIPanel_QueryPaneRect(LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS, &leftPaneRect);
    hasRightPane = UIPanel_QueryPaneRect(LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &rightPaneRect);
    if (!hasLeftPane) {
        leftPaneRect = (SDL_Rect){ leftX - padding, topOffset - padding, leftW + (padding * 2), screenH - topOffset };
    }
    if (!hasRightPane) {
        rightPaneRect = (SDL_Rect){ rightX - padding, topOffset - padding, rightW + (padding * 2), screenH - topOffset };
    }
    UIPanel_UpdateTabLayout(&g_uiPanel, &leftPaneRect, &rightPaneRect, &metrics);
    UIPanel_UpdateScenePaneLayout(&g_uiPanel);
    UIPanel_UpdateObjectWorkspacePaneLayout(&g_uiPanel);
    UIPanel_UpdateFilePaneLayout(&g_uiPanel);
    UIPanel_UpdateViewPaneLayout(&g_uiPanel);
    UIPanel_UpdateCreatePaneLayout(&g_uiPanel);
    UIPanel_UpdateObjectPaneLayout(&g_uiPanel);
    UIPanel_UpdateEditPaneLayout(&g_uiPanel);
    leftX = g_uiPanel.leftBodyRect.x;
    leftY = g_uiPanel.leftBodyRect.y;
    leftW = g_uiPanel.leftBodyRect.w;
    rightX = g_uiPanel.rightBodyRect.x;
    rightY = g_uiPanel.rightBodyRect.y;
    rightW = g_uiPanel.rightBodyRect.w;
    int leftSceneSelectionY = leftY;
    int leftSceneBoundsY = leftY;
    int leftFileActionsY = leftY;
    int leftRootPathsY = leftY;
    {
        if (UIPanel_GetActiveLeftTab(&g_uiPanel) == UI_PANEL_LEFT_TAB_SCENE &&
            Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
            leftSceneSelectionY = g_uiPanel.scenePane.selectionRect.y;
            leftSceneBoundsY = g_uiPanel.scenePane.boundsRect.y;
        }
        if (UIPanel_GetActiveLeftTab(&g_uiPanel) == UI_PANEL_LEFT_TAB_FILE) {
            leftFileActionsY = g_uiPanel.filePane.fileActionsRect.y;
            leftRootPathsY = g_uiPanel.filePane.rootPathsRect.y;
        }
        if (UIPanel_GetActiveLeftTab(&g_uiPanel) == UI_PANEL_LEFT_TAB_FILE) {
            int fileSummaryHeight = UIPanel_FileSummaryReservedHeight(&g_uiPanel);
            if (fileSummaryHeight > 0) {
                leftY += fileSummaryHeight + groupGap;
            }
        }
    }

    for (int i = 0; i < g_uiPanel.count; ++i) {
        UIButton* btn = &g_uiPanel.buttons[i];
        if (btn->side == UI_PANEL_RIGHT) continue;
        if (btn->side == UI_PANEL_LEFT) {
            if (!UIPanel_ShouldShowGroup(&g_uiPanel, btn->group)) {
                btn->bounds = (SDL_Rect){0, 0, 0, 0};
                continue;
            }
            if ((btn->group == UI_PANEL_GROUP_LEFT_SCENE_SELECTION ||
                 btn->group == UI_PANEL_GROUP_LEFT_SCENE_BOUNDS) &&
                UIPanel_GetActiveLeftTab(&g_uiPanel) == UI_PANEL_LEFT_TAB_SCENE &&
                Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
                int* groupY = (btn->group == UI_PANEL_GROUP_LEFT_SCENE_SELECTION)
                                  ? &leftSceneSelectionY
                                  : &leftSceneBoundsY;
                btn->bounds = (SDL_Rect){ leftX, *groupY + groupHeaderHeight, leftW, btnH };
                *groupY += btnH + spacing;
                continue;
            }
            if ((btn->group == UI_PANEL_GROUP_LEFT_FILE_IO ||
                 btn->group == UI_PANEL_GROUP_LEFT_ROOT_PATHS) &&
                UIPanel_GetActiveLeftTab(&g_uiPanel) == UI_PANEL_LEFT_TAB_FILE) {
                int* groupY = (btn->group == UI_PANEL_GROUP_LEFT_FILE_IO)
                                  ? &leftFileActionsY
                                  : &leftRootPathsY;
                btn->bounds = (SDL_Rect){ leftX, *groupY + groupHeaderHeight, leftW, btnH };
                *groupY += btnH + spacing;
                continue;
            }
            if (btn->group != leftGroup) {
                if (leftGroup != UI_PANEL_GROUP_NONE) leftY += groupGap;
                leftY += groupHeaderHeight;
                leftGroup = btn->group;
            }
            btn->bounds = (SDL_Rect){ leftX, leftY, leftW, btnH };
            leftY += btnH + spacing;
        }
    }

    if (UIPanel_GetActiveLeftTab(&g_uiPanel) == UI_PANEL_LEFT_TAB_FILE) {
        UIPanel_LayoutFilePaneButtons(&g_uiPanel, &metrics, metrics.button_text_pad_px);
    }
    UIPanel_LayoutRightPaneButtons(&g_uiPanel, &metrics);
}

void UIPanel_Init(int screenW, int screenH) {
    g_uiPanel.count = 0;
    UIPanel_InitShellState(&g_uiPanel);
    g_uiPanel.sceneList.scrollOffsetPx = 0.0f;
    g_uiPanel.sceneList.hoverIndex = -1;
    g_uiPanel.sceneList.expandedObjectId = 0u;
    g_uiPanel.sceneList.scrollbarDragging = false;
    g_uiPanel.sceneList.scrollbarDragStartY = 0;
    g_uiPanel.sceneList.scrollbarDragStartOffsetPx = 0.0f;
    g_uiPanel.objectModelTree.operationScrollOffsetPx = 0.0f;
    g_uiPanel.objectModelTree.hoverOperationIndex = -1;
    g_uiPanel.objectModelTree.operationScrollbarDragging = false;
    g_uiPanel.objectModelTree.operationScrollbarDragStartY = 0;
    g_uiPanel.objectModelTree.operationScrollbarDragStartOffsetPx = 0.0f;
    g_uiPanel.saveDialog.active = false;
    g_uiPanel.saveDialog.buffer[0] = '\0';
    g_uiPanel.saveDialog.length = 0;
    g_uiPanel.saveDialog.cursor = 0;
    g_uiPanel.loadMenu.open = false;
    g_uiPanel.loadMenu.visible = false;
    g_uiPanel.loadMenu.mode = UI_LOAD_MENU_MODE_NONE;
    g_uiPanel.loadMenu.anchorButtonId = UI_BTN_LOAD_JSON;
    g_uiPanel.loadMenu.rootPath[0] = '\0';
    g_uiPanel.loadMenu.count = 0;
    g_uiPanel.loadMenu.hoverIndex = -1;
    g_uiPanel.loadMenu.activeIndex = -1;
    g_uiPanel.loadMenu.scrollOffsetPx = 0.0f;
    g_uiPanel.loadMenu.scrollbarDragging = false;
    g_uiPanel.loadMenu.scrollbarDragStartY = 0;
    g_uiPanel.loadMenu.scrollbarDragStartOffsetPx = 0.0f;
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
    AddButton(&g_uiPanel, "Load Scene", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_LOAD_SCENE);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Load STL", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_LOAD_STL);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Mesh Assets", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_LOAD_MESH_ASSET);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Export Shape", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_EXPORT_SHAPE);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Export Scene", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_EXPORT_SCENE);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Use Session", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_FILE_BROWSER_USE_ACTIVE);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Clear Last", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_FILE_IO, UI_BTN_FILE_BROWSER_CLEAR_REMEMBERED);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Session In Edit", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_INPUT_ROOT_EDIT);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Session In Pick", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_INPUT_ROOT_FOLDER);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Output Edit", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_OUTPUT_ROOT_EDIT);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Output Pick", xL, yL, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_ROOT_PATHS, UI_BTN_OUTPUT_ROOT_FOLDER);

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
    AddButton(&g_uiPanel, "Place Mesh", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRIMITIVES, UI_BTN_PLACE_MESH_INSTANCE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Face Select", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRIMITIVES, UI_BTN_OBJECT_FACE_SELECT);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Sketch Select", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRIMITIVES, UI_BTN_OBJECT_SKETCH_SELECT);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Clear Sketch", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_PRIMITIVES, UI_BTN_OBJECT_SKETCH_CLEAR);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Extrude +", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_OPERATIONS, UI_BTN_EXTRUDE_ADD);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Extrude -", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_OPERATIONS, UI_BTN_EXTRUDE_CUT);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Depth -", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_OPERATIONS, UI_BTN_EXTRUDE_DEPTH_DEC);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Depth +", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_OPERATIONS, UI_BTN_EXTRUDE_DEPTH_INC);
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
    AddButton(&g_uiPanel, "Clear Select", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS, UI_BTN_OBJECT_CLEAR_SELECTION);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Delete Object", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS, UI_BTN_OBJECT_DELETE_SELECTED);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Gizmo: Mode (X)", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_GIZMO, UI_BTN_TOGGLE_OBJECT_GIZMO_MODE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edit Pos", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_POSITION);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Rot X", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_ROTATION_X);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Rot Y", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_ROTATION_Y);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Rot Z", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_TRANSFORM, UI_BTN_EDIT_OBJECT_ROTATION_Z);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Body", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_EDIT_SELECT, UI_BTN_OBJECT_EDIT_BODY_MODE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Face", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_EDIT_SELECT, UI_BTN_OBJECT_EDIT_FACE_MODE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edge", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_EDIT_SELECT, UI_BTN_OBJECT_EDIT_EDGE_MODE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Vertex", xR, yR, rightBtnW, btnH, UI_PANEL_RIGHT, UI_PANEL_GROUP_RIGHT_EDIT_SELECT, UI_BTN_OBJECT_EDIT_VERTEX_MODE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Clear Select", xL, topOffset, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_SCENE_SELECTION, UI_BTN_SCENE_CLEAR_SELECTION);
    AddButton(&g_uiPanel, "Delete Obj", xL, topOffset, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_SCENE_SELECTION, UI_BTN_SCENE_DELETE_SELECTED);
    AddButton(&g_uiPanel, "Bounds: Off", xL, topOffset, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_SCENE_BOUNDS, UI_BTN_TOGGLE_SCENE_BOUNDS);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Clamp: Off", xL, topOffset, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_SCENE_BOUNDS, UI_BTN_TOGGLE_SCENE_BOUNDS_CLAMP);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edit BMin", xL, topOffset, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_SCENE_BOUNDS, UI_BTN_EDIT_SCENE_BOUNDS_MIN);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Edit BMax", xL, topOffset, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_SCENE_BOUNDS, UI_BTN_EDIT_SCENE_BOUNDS_MAX);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Fit B->Obj", xL, topOffset, leftBtnW, btnH, UI_PANEL_LEFT, UI_PANEL_GROUP_LEFT_SCENE_BOUNDS, UI_BTN_FIT_SCENE_BOUNDS_TO_OBJECT);

    UIPanel_OnWindowResized(screenW, screenH);
    UIPanel_LoadFileBrowserMode(&g_uiPanel);
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

void UIPanel_ResetTransientUiState(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    ui->loadMenu.open = false;
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = -1;
    ui->loadMenu.scrollOffsetPx = 0.0f;
    ui->loadMenu.scrollbarDragging = false;
    ui->sceneList.scrollbarDragging = false;
    ui->objectModelTree.operationScrollbarDragging = false;
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    if (SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
}

void UIPanel_RenderOverlays(SDL_Renderer* renderer) {
    UIPanel_RenderOverlayDialogs(renderer, &g_uiPanel);
}
