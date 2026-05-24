#include "UI/ui_panel_create_summary.h"

#include "Core/global_state.h"
#include "Editor/primitive_placement_preview.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_internal.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelCreateSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelCreateSummary_LineGap(void) {
    int gap = UIPanelCreateSummary_FontHeight() / 3;
    if (gap < 4) gap = 4;
    return gap;
}

static int UIPanelCreateSummary_PanelPad(void) {
    int pad = UIPanelCreateSummary_FontHeight() / 2;
    if (pad < 8) pad = 8;
    return pad;
}

static void UIPanelCreateSummary_DrawText(SDL_Renderer* renderer,
                                          TTF_Font* font,
                                          const char* text,
                                          int x,
                                          int y,
                                          SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void UIPanelCreateSummary_DrawTextClipped(SDL_Renderer* renderer,
                                                 TTF_Font* font,
                                                 const char* text,
                                                 int x,
                                                 int y,
                                                 int maxWidth,
                                                 SDL_Color color) {
    char clipped[256];
    int width = 0;
    if (!renderer || !font || !text || maxWidth <= 0) return;
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) &&
        width <= maxWidth) {
        UIPanelCreateSummary_DrawText(renderer, font, text, x, y, color);
        return;
    }
    snprintf(clipped, sizeof(clipped), "%s", text);
    while (strlen(clipped) > 4) {
        size_t len = strlen(clipped);
        clipped[len - 1] = '\0';
        clipped[len - 2] = '.';
        clipped[len - 3] = '.';
        clipped[len - 4] = '.';
        if (line_drawing_text_measure_utf8(renderer, font, clipped, &width, NULL) &&
            width <= maxWidth) {
            UIPanelCreateSummary_DrawText(renderer, font, clipped, x, y, color);
            return;
        }
    }
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

int UIPanel_CreateSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    const int lines = 6;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_CREATE) return 0;
    font_h = UIPanelCreateSummary_FontHeight();
    line_gap = UIPanelCreateSummary_LineGap();
    pad = UIPanelCreateSummary_PanelPad();
    return (pad * 2) + (font_h * lines) + (line_gap * (lines - 1));
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
    LineDrawing3dThemePalette palette = {0};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect panel = {0, 0, 0, 0};
    char line_space[128];
    char line_plane[128];
    char line_grid[128];
    char line_plane_preview[128];
    char line_prism_preview[128];
    int font_h = 0;
    int line_gap = 0;
    int panel_pad = 0;
    int y = 0;

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_CREATE) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    panel = ui->rightBodyRect;
    panel.h = UIPanel_CreateSummaryReservedHeight(ui);
    if (panel.h <= 0) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        label_color = palette.text_muted;
        value_color = palette.text_primary;
        accent_color = palette.button_border;
    }

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, 210);
        SDL_RenderDrawRect(renderer, &panel);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 200);
        SDL_RenderDrawRect(renderer, &panel);
    }
    {
        SDL_Rect accent_band = { panel.x + 1, panel.y + 1, panel.w - 2, 4 };
        SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, 220);
        SDL_RenderFillRect(renderer, &accent_band);
    }

    font_h = UIPanelCreateSummary_FontHeight();
    line_gap = UIPanelCreateSummary_LineGap();
    panel_pad = UIPanelCreateSummary_PanelPad();
    y = panel.y + panel_pad;
    plane = UIPanel_CurrentConstructionViewPlane(state);

    UIPanelCreateSummary_DrawText(renderer, font, "Create / Construction", panel.x + 8, y, label_color);
    y += font_h + line_gap;

    snprintf(line_space,
             sizeof(line_space),
             "Space  %s",
             Global_GetSpaceModeLabel(state->spaceMode));
    snprintf(line_plane,
             sizeof(line_plane),
             "Plane  %s (%s=%.2f)",
             UIPanel_ViewPlaneAxisLabel(plane.axis),
             UIPanel_ViewPlaneCoordinateLabel(plane.axis),
             plane.offset);

    {
        char grid_text[32];
        UIPanelCreateSummary_FormatDimension(state->grid.gridSize, grid_text, sizeof(grid_text));
        snprintf(line_grid, sizeof(line_grid), "Grid  %s per step", grid_text);
    }

    if (plane_ready) {
        char w_text[32];
        char h_text[32];
        UIPanelCreateSummary_FormatDimension(plane_preview.width, w_text, sizeof(w_text));
        UIPanelCreateSummary_FormatDimension(plane_preview.height, h_text, sizeof(h_text));
        snprintf(line_plane_preview,
                 sizeof(line_plane_preview),
                 "Plane  %s x %s ready",
                 w_text,
                 h_text);
    } else {
        snprintf(line_plane_preview,
                 sizeof(line_plane_preview),
                 "Plane  unavailable in current mode");
    }

    if (prism_ready) {
        char w_text[32];
        char h_text[32];
        char d_text[32];
        UIPanelCreateSummary_FormatDimension(prism_preview.width, w_text, sizeof(w_text));
        UIPanelCreateSummary_FormatDimension(prism_preview.height, h_text, sizeof(h_text));
        UIPanelCreateSummary_FormatDimension(prism_preview.depth, d_text, sizeof(d_text));
        snprintf(line_prism_preview,
                 sizeof(line_prism_preview),
                 "Prism  %s x %s x %s ready",
                 w_text,
                 h_text,
                 d_text);
    } else {
        snprintf(line_prism_preview,
                 sizeof(line_prism_preview),
                 "Prism  switch to 3D to author primitives");
    }

    UIPanelCreateSummary_DrawTextClipped(renderer, font, line_space, panel.x + 8, y, panel.w - 16, accent_color);
    y += font_h + line_gap;
    SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, 90);
    SDL_RenderDrawLine(renderer, panel.x + 8, y - (line_gap / 2), panel.x + panel.w - 8, y - (line_gap / 2));
    UIPanelCreateSummary_DrawTextClipped(renderer, font, line_plane, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelCreateSummary_DrawTextClipped(renderer, font, line_grid, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelCreateSummary_DrawTextClipped(renderer, font, line_plane_preview, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelCreateSummary_DrawTextClipped(renderer, font, line_prism_preview, panel.x + 8, y, panel.w - 16, label_color);
}
