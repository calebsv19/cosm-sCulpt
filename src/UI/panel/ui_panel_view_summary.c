#include "UI/ui_panel_view_summary.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

enum {
    UI_VIEW_SUMMARY_HEADER_LINES = 5,
    UI_VIEW_CARD_ACCENT_HEIGHT = 4
};

static int UIPanelViewSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelViewSummary_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelViewSummary_PanelPad(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
}

static int UIPanelViewSummary_DrawLines(SDL_Renderer* renderer,
                                        TTF_Font* font,
                                        SDL_Rect rect,
                                        const char* title,
                                        const char* const* lines,
                                        const SDL_Color* line_colors,
                                        int line_count,
                                        bool wrap_lines,
                                        SDL_Color title_color,
                                        SDL_Color divider_color) {
    int font_h = UIPanelViewSummary_FontHeight();
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int line_gap = metrics.section_gap;
    int panel_pad = metrics.pad_y;
    int max_width = rect.w - (metrics.pad_x * 2);
    int content_bottom = rect.y + rect.h - panel_pad;
    int y = rect.y + panel_pad;

    if (!renderer || !font || rect.w <= 0 || rect.h <= 0) return 0;

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

static const char* UIPanelViewSummary_DeleteModeLabel(DeleteMode mode) {
    return mode == DELETE_MODE_AUTO_PRUNE ? "Auto prune" : "Safe";
}

static void UIPanelViewSummary_BuildSelectionLine(const GlobalState* state,
                                                  char* out,
                                                  size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!state) {
        snprintf(out, out_size, "Selection  none");
        return;
    }
    if (state->editor.selectedObject3DId != 0u) {
        if (Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT &&
            state->editor.selectedObjectAssetBodyId != 0u) {
            snprintf(out,
                     out_size,
                     "Selection  body #%u   face %s",
                     state->editor.selectedObjectAssetBodyId,
                     Layout_Object3DFaceKind_Label(state->editor.selectedObjectAssetFace));
        } else {
            snprintf(out, out_size, "Selection  object #%u", state->editor.selectedObject3DId);
        }
        return;
    }
    if (state->editor.selectedAnchorIndex >= 0) {
        snprintf(out, out_size, "Selection  anchor #%d", state->editor.selectedAnchorIndex);
        return;
    }
    if (state->editor.selectedWallIndex >= 0) {
        snprintf(out, out_size, "Selection  wall #%d", state->editor.selectedWallIndex);
        return;
    }
    snprintf(out, out_size, "Selection  none");
}

int UIPanel_ViewSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_VIEW) return 0;
    font_h = UIPanelViewSummary_FontHeight();
    line_gap = UIPanelViewSummary_LineGap();
    pad = UIPanelViewSummary_PanelPad();
    return (pad * 2) + (font_h * UI_VIEW_SUMMARY_HEADER_LINES) +
           (line_gap * (UI_VIEW_SUMMARY_HEADER_LINES - 1));
}

void Render_UIPanelViewSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f };
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
    char line_mode[128];
    char line_zoom[128];
    char line_plane[128];
    char line_delete[128];
    char line_selection[128];
    const char* summary_lines[5];
    SDL_Color summary_colors[5];
    char work_camera[128];
    char work_mode[128];
    char work_plane[128];
    char work_delete[128];
    char work_selection[128];
    char work_future[128];
    const char* work_lines[6];
    SDL_Color work_colors[6];
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_VIEW) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    summary_rect = ui->viewPane.summaryRect;
    workspace_rect = ui->viewPane.workspaceRect;
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

    snprintf(line_mode,
             sizeof(line_mode),
             "Mode  %s",
             Global_GetSpaceModeLabel(state->spaceMode));
    snprintf(line_zoom,
             sizeof(line_zoom),
             "Preview  %s   Zoom %.2fx   Grid %.2f",
             Global_GetPreviewModeLabel(Global_GetPreviewMode()),
             state->grid.scale,
             state->grid.gridSize);
    snprintf(line_plane,
             sizeof(line_plane),
             "Plane  %s   %s=%.2f",
             UIPanel_ViewPlaneAxisLabel(plane.axis),
             UIPanel_ViewPlaneCoordinateLabel(plane.axis),
             plane.offset);
    snprintf(line_delete,
             sizeof(line_delete),
             "Delete  %s mode",
             UIPanelViewSummary_DeleteModeLabel(state->editor.deleteMode));
    UIPanelViewSummary_BuildSelectionLine(state, line_selection, sizeof(line_selection));

    summary_lines[0] = line_mode;
    summary_lines[1] = line_zoom;
    summary_lines[2] = line_plane;
    summary_lines[3] = line_delete;
    summary_lines[4] = line_selection;
    summary_colors[0] = accent_color;
    summary_colors[1] = value_color;
    summary_colors[2] = value_color;
    summary_colors[3] = value_color;
    summary_colors[4] = label_color;

    UIPanelSummary_DrawCard(renderer, summary_rect, fill_color, border_color, accent_color, metrics.accent_h);
    UIPanelViewSummary_DrawLines(renderer,
                                 font,
                                 summary_rect,
                                 object_mode ? "View Actions" : "View",
                                 summary_lines,
                                 summary_colors,
                                 5,
                                 false,
                                 label_color,
                                 accent_color);

    if (workspace_rect.w <= 0 || workspace_rect.h <= 0) return;

    snprintf(work_camera,
             sizeof(work_camera),
             object_mode
                 ? "Buttons  O reset, + zoom in, - zoom out"
                 : "Viewport  origin reset and zoom controls stay in the bottom lane");
    snprintf(work_mode,
             sizeof(work_mode),
             object_mode ? "Mode  %s / %s / %s" : "Editing mode  %s with %s space active",
             UIPanelViewSummary_DeleteModeLabel(state->editor.deleteMode),
             Global_GetSpaceModeLabel(state->spaceMode),
             Global_GetPreviewModeLabel(Global_GetPreviewMode()));
    snprintf(work_plane,
             sizeof(work_plane),
             "Construction plane  %s at %s=%.2f",
             UIPanel_ViewPlaneAxisLabel(plane.axis),
             UIPanel_ViewPlaneCoordinateLabel(plane.axis),
             plane.offset);
    snprintf(work_delete,
             sizeof(work_delete),
             object_mode ? "Grid  %.2fx / %.2f step" : "Grid  scale %.2fx with step %.2f",
             state->grid.scale,
             state->grid.gridSize);
    if (object_mode) {
        work_selection[0] = '\0';
        work_future[0] = '\0';
    } else {
        snprintf(work_selection,
                 sizeof(work_selection),
                 "Selection context  %s",
                 (state->editor.selectedObject3DId != 0u) ? "object-local editing is available" :
                 (state->editor.selectedAnchorIndex >= 0 || state->editor.selectedWallIndex >= 0) ? "2D edit focus is active" :
                 "nothing selected");
        snprintf(work_future,
                 sizeof(work_future),
                 "Display toggles, framing, and camera helpers will live here");
    }

    work_lines[0] = work_camera;
    work_lines[1] = work_mode;
    work_lines[2] = work_plane;
    work_lines[3] = work_delete;
    work_lines[4] = work_selection;
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
    UIPanelViewSummary_DrawLines(renderer,
                                 font,
                                 workspace_rect,
                                 object_mode ? "Viewport Controls" : "Viewport Workspace",
                                 work_lines,
                                 work_colors,
                                 6,
                                 true,
                                 label_color,
                                 accent_color);
}
