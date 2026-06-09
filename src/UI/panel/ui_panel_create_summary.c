#include "UI/ui_panel_create_summary.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Editor/object_face_sketch.h"
#include "Layout/scene/layout_object_faces.h"
#include "ObjectAuthoring/object_authoring_document.h"
#include "Editor/primitive_placement_preview.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

enum {
    UI_CREATE_SUMMARY_HEADER_LINES = 5,
    UI_CREATE_WORKSPACE_PAD_MIN = 8,
    UI_CREATE_CARD_ACCENT_HEIGHT = 4
};

static int UIPanelCreateSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelCreateSummary_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelCreateSummary_PanelPad(void) {
    int pad = UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
    if (pad < UI_CREATE_WORKSPACE_PAD_MIN) pad = UI_CREATE_WORKSPACE_PAD_MIN;
    return pad;
}

static void UIPanelCreateSummary_FormatDimension(float world_value,
                                                 char* out,
                                                 size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.2f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.2f", world_value);
    }
}

static const char* UIPanelCreateSummary_OperationLabel(ObjectAuthoringOperationKind kind) {
    switch (kind) {
        case OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE: return "Create Primitive";
        case OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE: return "Sketch Rect";
        case OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD: return "Extrude Add";
        case OBJECT_AUTHORING_OPERATION_EXTRUDE_CUT: return "Extrude Cut";
        case OBJECT_AUTHORING_OPERATION_NONE:
        default: return "None";
    }
}

static const char* UIPanelCreateSummary_ExtrudeModeLabel(ObjectFaceExtrudeMode mode) {
    switch (mode) {
        case OBJECT_FACE_EXTRUDE_MODE_ADD: return "Extrude Add";
        case OBJECT_FACE_EXTRUDE_MODE_CUT: return "Extrude Cut";
        case OBJECT_FACE_EXTRUDE_MODE_NONE:
        default: return "None";
    }
}

static int UIPanelCreateSummary_DrawLines(SDL_Renderer* renderer,
                                          TTF_Font* font,
                                          SDL_Rect rect,
                                          const char* title,
                                          const char* const* lines,
                                          const SDL_Color* line_colors,
                                          int line_count,
                                          bool wrap_lines,
                                          SDL_Color title_color,
                                          SDL_Color divider_color) {
    int font_h = UIPanelCreateSummary_FontHeight();
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int line_gap = metrics.section_gap;
    int panel_pad = metrics.pad_y;
    int max_width = rect.w - (metrics.pad_x * 2);
    int content_bottom = rect.y + rect.h - panel_pad;
    int y = 0;

    if (!renderer || !font || rect.w <= 0 || rect.h <= 0) return 0;

    y = rect.y + panel_pad;
    if (title && title[0]) {
        UIPanelSummary_DrawText(renderer, font, title, rect.x + metrics.pad_x, y, title_color);
        y += font_h + line_gap;
    }

    for (int i = 0; i < line_count; ++i) {
        int lines_drawn = 0;
        int max_lines = 0;
        if (!lines[i] || !lines[i][0]) continue;
        if (y + font_h > rect.y + rect.h - panel_pad) break;
        if (wrap_lines) {
            max_lines = (content_bottom - y + line_gap) / (font_h + line_gap);
            if (max_lines <= 0) break;
            lines_drawn = UIPanelSummary_DrawWrappedText(renderer,
                                                         font,
                                                         lines[i],
                                                         rect.x + metrics.pad_x,
                                                         y,
                                                         max_width,
                                                         font_h,
                                                         line_gap,
                                                         max_lines,
                                                         line_colors ? line_colors[i] : title_color);
            y += lines_drawn * (font_h + line_gap);
        } else {
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           lines[i],
                                           rect.x + metrics.pad_x,
                                           y,
                                           max_width,
                                           font_h + 4,
                                           line_colors ? line_colors[i] : title_color);
            y += font_h + line_gap;
            lines_drawn = 1;
        }
        if (i == 0 && y < rect.y + rect.h - panel_pad) {
            UIPanelSummary_DrawDivider(renderer,
                                       rect,
                                       y - (line_gap / 2),
                                       metrics.pad_x,
                                       divider_color,
                                       90);
        }
    }

    return y;
}

int UIPanel_CreateSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_CREATE) return 0;
    font_h = UIPanelCreateSummary_FontHeight();
    line_gap = UIPanelCreateSummary_LineGap();
    pad = UIPanelCreateSummary_PanelPad();
    return (pad * 2) + (font_h * UI_CREATE_SUMMARY_HEADER_LINES) +
           (line_gap * (UI_CREATE_SUMMARY_HEADER_LINES - 1));
}

void Render_UIPanelCreateSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    PrimitivePlacementPreview plane_preview = {0};
    PrimitivePlacementPreview prism_preview = {0};
    const bool plane_ready = state &&
        Editor_PrimitivePlacementPreview_Build(state,
                                               PRIMITIVE_PLACEMENT_PREVIEW_PLANE,
                                               &plane_preview);
    const bool prism_ready = state &&
        Editor_PrimitivePlacementPreview_Build(state,
                                               PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM,
                                               &prism_preview);
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f };
    PrimitivePlacementPreviewKind active_preview = PRIMITIVE_PLACEMENT_PREVIEW_NONE;
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 200};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect workspace_rect = {0, 0, 0, 0};
    char summary_space[128];
    char summary_plane[128];
    char summary_mode[128];
    char summary_grid[128];
    char summary_stage[128];
    const char* summary_lines[5];
    SDL_Color summary_colors[5];
    char work_tool[128];
    char work_ready[128];
    char work_plane[128];
    char work_prism[128];
    char work_next[128];
    char work_future[128];
    const char* work_lines[6];
    SDL_Color work_colors[6];

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_CREATE) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    summary_rect = ui->createPane.summaryRect;
    workspace_rect = ui->createPane.workspaceRect;
    if (summary_rect.w <= 0 || summary_rect.h <= 0) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;

    plane = UIPanel_CurrentConstructionViewPlane(state);
    active_preview = state->editor.primitivePlacementPreview;

    if (Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
        const bool tool_armed = state->editor.objectFaceSketchToolArmed;
        const bool dragging = state->editor.objectFaceSketchDragging;
        const bool has_rect = state->editor.objectFaceSketchHasRectangle;
        const bool sketch_selected = Editor_ObjectFaceSketchIsSelected(&state->editor);
        const char* stage_label = Editor_ObjectAuthoringStageLabel(&state->editor);
        const char* prompt_label = Editor_ObjectAuthoringPromptLabel(&state->editor);
        const ObjectAuthoringDocument* doc =
            state->objectAuthoring.attached ? &state->objectAuthoring.document : NULL;
        const ObjectAuthoringOperation* selected_op =
            doc ? ObjectAuthoringDocument_FindOperation(doc, doc->selectedOperationId) : NULL;

        snprintf(summary_space,
                 sizeof(summary_space),
                 "Target  choose a face");
        snprintf(summary_plane,
                 sizeof(summary_plane),
                 "Sketch  draw / select / clear");
        snprintf(summary_mode,
                 sizeof(summary_mode),
                 "Solid  Extrude + / Extrude -");
        snprintf(summary_grid,
                 sizeof(summary_grid),
                 "Mode  %s",
                 Editor_ObjectAuthoringModeLabel(state->editor.objectAuthoringMode));
        snprintf(summary_stage,
                 sizeof(summary_stage),
                 "%s  |  %zu ops",
                 stage_label,
                 doc ? doc->operationCount : 0u);

        summary_lines[0] = summary_space;
        summary_lines[1] = summary_plane;
        summary_lines[2] = summary_mode;
        summary_lines[3] = summary_grid;
        summary_lines[4] = summary_stage;
        summary_colors[0] = accent_color;
        summary_colors[1] = value_color;
        summary_colors[2] = value_color;
        summary_colors[3] = value_color;
        summary_colors[4] = label_color;

        UIPanelSummary_DrawCard(renderer, summary_rect, fill_color, border_color, accent_color, metrics.accent_h);
        UIPanelCreateSummary_DrawLines(renderer,
                                       font,
                                       summary_rect,
                                       "Command Actions",
                                       summary_lines,
                                       summary_colors,
                                       5,
                                       false,
                                       label_color,
                                       accent_color);

        if (workspace_rect.w <= 0 || workspace_rect.h <= 0) return;

        snprintf(work_tool,
                 sizeof(work_tool),
                 "Command  %s",
                 prompt_label);
        if (state->editor.selectedObjectAssetBodyId != 0u &&
            state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE) {
            const ObjectAuthoringFaceId face_id =
                ObjectAuthoringFaceId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    state->editor.selectedObjectAssetFace);
            snprintf(work_ready,
                     sizeof(work_ready),
                     "Target  FaceID %u  Body #%u  %s",
                     face_id,
                     state->editor.selectedObjectAssetBodyId,
                     state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE
                         ? Layout_Object3DFaceKind_Label(state->editor.selectedObjectAssetFace)
                         : "None");
        } else if (state->editor.selectedObjectAssetBodyId != 0u) {
            snprintf(work_ready,
                     sizeof(work_ready),
                     "Target  Body #%u  Face None",
                     state->editor.selectedObjectAssetBodyId);
        } else {
            snprintf(work_ready, sizeof(work_ready), "Target  None");
        }
        snprintf(work_plane,
                 sizeof(work_plane),
                 "Sketch  %s",
                 sketch_selected
                     ? "Selected"
                     : has_rect
                         ? "Present"
                         : dragging
                             ? "Drawing"
                             : tool_armed
                                 ? "Armed"
                                 : "None");
        if (state->editor.objectFaceExtrudeToolArmed) {
            char depth_text[32];
            char step_text[32];
            UIPanelCreateSummary_FormatDimension(state->editor.objectFaceExtrudeDepth,
                                                 depth_text,
                                                 sizeof(depth_text));
            UIPanelCreateSummary_FormatDimension(state->grid.gridSize > 0.0f
                                                     ? state->grid.gridSize
                                                     : 1.0f,
                                                 step_text,
                                                 sizeof(step_text));
            snprintf(work_prism,
                     sizeof(work_prism),
                     "Live Op  %s preview",
                     UIPanelCreateSummary_ExtrudeModeLabel(state->editor.objectFaceExtrudeMode));
            snprintf(work_next,
                     sizeof(work_next),
                     "Parameter  Depth %s  |  Step %s",
                     depth_text,
                     step_text);
        } else if (selected_op) {
            snprintf(work_prism,
                     sizeof(work_prism),
                     "Selected Op  #%u  %s",
                     selected_op->operationId,
                     UIPanelCreateSummary_OperationLabel(selected_op->kind));
            snprintf(work_next,
                     sizeof(work_next),
                     "Parameters  no live command active");
        } else {
            snprintf(work_prism,
                     sizeof(work_prism),
                     "Selected Op  none");
            snprintf(work_next,
                     sizeof(work_next),
                     "Parameters  choose a command first");
        }
        if (state->editor.objectFaceExtrudeToolArmed) {
            snprintf(work_future,
                     sizeof(work_future),
                     "Commit  press Extrude +/- again");
        } else {
            work_future[0] = '\0';
        }

        work_lines[0] = work_tool;
        work_lines[1] = work_ready;
        work_lines[2] = work_plane;
        work_lines[3] = work_prism;
        work_lines[4] = work_next;
        work_lines[5] = work_future;
        work_colors[0] = value_color;
        work_colors[1] = value_color;
        work_colors[2] = value_color;
        work_colors[3] = value_color;
        work_colors[4] = label_color;
        work_colors[5] = label_color;

        fill_color = palette.workspace_fill;
        fill_color.a = palette.workspace_fill.a;
        UIPanelSummary_DrawCard(renderer, workspace_rect, fill_color, border_color, accent_color, metrics.accent_h);
        UIPanelCreateSummary_DrawLines(renderer,
                                       font,
                                       workspace_rect,
                                       "Active Command",
                                       work_lines,
                                       work_colors,
                                       6,
                                       true,
                                       label_color,
                                       accent_color);
        return;
    }

    snprintf(summary_space,
             sizeof(summary_space),
             "Mode  %s",
             Global_GetSpaceModeLabel(state->spaceMode));
    snprintf(summary_plane,
             sizeof(summary_plane),
             "Plane  %s   %s=%.2f",
             UIPanel_ViewPlaneAxisLabel(plane.axis),
             UIPanel_ViewPlaneCoordinateLabel(plane.axis),
             plane.offset);
    {
        char grid_text[32];
        UIPanelCreateSummary_FormatDimension(state->grid.gridSize, grid_text, sizeof(grid_text));
        snprintf(summary_grid, sizeof(summary_grid), "Grid  %s step", grid_text);
    }
    snprintf(summary_mode,
             sizeof(summary_mode),
             "Tool  %s",
             (active_preview == PRIMITIVE_PLACEMENT_PREVIEW_PLANE) ? "Plane staging" :
             (active_preview == PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM) ? "Prism staging" :
             "Ready to stage");
    snprintf(summary_stage,
             sizeof(summary_stage),
             "Bottom controls stay anchored");

    summary_lines[0] = summary_space;
    summary_lines[1] = summary_plane;
    summary_lines[2] = summary_mode;
    summary_lines[3] = summary_grid;
    summary_lines[4] = summary_stage;
    summary_colors[0] = accent_color;
    summary_colors[1] = value_color;
    summary_colors[2] = value_color;
    summary_colors[3] = value_color;
    summary_colors[4] = label_color;

    UIPanelSummary_DrawCard(renderer, summary_rect, fill_color, border_color, accent_color, metrics.accent_h);
    UIPanelCreateSummary_DrawLines(renderer,
                                   font,
                                   summary_rect,
                                   "Create",
                                   summary_lines,
                                   summary_colors,
                                   5,
                                   false,
                                   label_color,
                                   accent_color);

    if (workspace_rect.w <= 0 || workspace_rect.h <= 0) return;

    if (plane_ready) {
        char w_text[32];
        char h_text[32];
        UIPanelCreateSummary_FormatDimension(plane_preview.width, w_text, sizeof(w_text));
        UIPanelCreateSummary_FormatDimension(plane_preview.height, h_text, sizeof(h_text));
        snprintf(work_plane,
                 sizeof(work_plane),
                 "Plane staging  %s x %s on %s",
                 w_text,
                 h_text,
                 UIPanel_ViewPlaneAxisLabel(plane.axis));
    } else {
        snprintf(work_plane,
                 sizeof(work_plane),
                 "Plane staging unavailable in the current space mode");
    }

    if (prism_ready) {
        char w_text[32];
        char h_text[32];
        char d_text[32];
        UIPanelCreateSummary_FormatDimension(prism_preview.width, w_text, sizeof(w_text));
        UIPanelCreateSummary_FormatDimension(prism_preview.height, h_text, sizeof(h_text));
        UIPanelCreateSummary_FormatDimension(prism_preview.depth, d_text, sizeof(d_text));
        snprintf(work_prism,
                 sizeof(work_prism),
                 "Prism staging  %s x %s x %s",
                 w_text,
                 h_text,
                 d_text);
    } else {
        snprintf(work_prism,
                 sizeof(work_prism),
                 "Prism staging appears once 3D placement is valid");
    }

    snprintf(work_tool,
             sizeof(work_tool),
             "Primary tool  %s",
             (active_preview == PRIMITIVE_PLACEMENT_PREVIEW_PLANE) ? "Plane" :
             (active_preview == PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM) ? "Prism" :
             "Choose from the bottom create row");
    snprintf(work_ready,
             sizeof(work_ready),
             "Ready now  %s",
             (plane_ready || prism_ready) ? "Author on the active construction plane" :
             "Switch mode/plane until staging becomes valid");
    snprintf(work_next,
             sizeof(work_next),
             "Presets and reusable dimensions can extend this pane later");
    snprintf(work_future,
             sizeof(work_future),
             "Tool options appear here when a create tool is active");

    work_lines[0] = work_tool;
    work_lines[1] = work_ready;
    work_lines[2] = work_plane;
    work_lines[3] = work_prism;
    work_lines[4] = work_next;
    work_lines[5] = work_future;
    work_colors[0] = value_color;
    work_colors[1] = value_color;
    work_colors[2] = value_color;
    work_colors[3] = value_color;
    work_colors[4] = label_color;
    work_colors[5] = label_color;

    fill_color = palette.workspace_fill;
    fill_color.a = palette.workspace_fill.a;
    UIPanelSummary_DrawCard(renderer, workspace_rect, fill_color, border_color, accent_color, metrics.accent_h);
    UIPanelCreateSummary_DrawLines(renderer,
                                   font,
                                   workspace_rect,
                                   "Authoring Workspace",
                                   work_lines,
                                   work_colors,
                                   6,
                                   true,
                                   label_color,
                                   accent_color);
}
