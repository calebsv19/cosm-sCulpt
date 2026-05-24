#include "UI/ui_panel_view_summary.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_internal.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelViewSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelViewSummary_LineGap(void) {
    int gap = UIPanelViewSummary_FontHeight() / 3;
    if (gap < 4) gap = 4;
    return gap;
}

static int UIPanelViewSummary_PanelPad(void) {
    int pad = UIPanelViewSummary_FontHeight() / 2;
    if (pad < 8) pad = 8;
    return pad;
}

static void UIPanelViewSummary_DrawText(SDL_Renderer* renderer,
                                        TTF_Font* font,
                                        const char* text,
                                        int x,
                                        int y,
                                        SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void UIPanelViewSummary_DrawTextClipped(SDL_Renderer* renderer,
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
        UIPanelViewSummary_DrawText(renderer, font, text, x, y, color);
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
            UIPanelViewSummary_DrawText(renderer, font, clipped, x, y, color);
            return;
        }
    }
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
        snprintf(out, out_size, "Selection  Object #%u", state->editor.selectedObject3DId);
        return;
    }
    if (state->editor.selectedAnchorIndex >= 0) {
        snprintf(out, out_size, "Selection  Anchor #%d", state->editor.selectedAnchorIndex);
        return;
    }
    if (state->editor.selectedWallIndex >= 0) {
        snprintf(out, out_size, "Selection  Wall #%d", state->editor.selectedWallIndex);
        return;
    }
    snprintf(out, out_size, "Selection  none");
}

int UIPanel_ViewSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    const int lines = 6;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_VIEW) return 0;
    font_h = UIPanelViewSummary_FontHeight();
    line_gap = UIPanelViewSummary_LineGap();
    pad = UIPanelViewSummary_PanelPad();
    return (pad * 2) + (font_h * lines) + (line_gap * (lines - 1));
}

void Render_UIPanelViewSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect panel = {0, 0, 0, 0};
    char line_mode[128];
    char line_zoom[128];
    char line_plane[128];
    char line_delete[128];
    char line_selection[128];
    int font_h = 0;
    int line_gap = 0;
    int panel_pad = 0;
    int y = 0;

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_VIEW) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    panel = ui->rightBodyRect;
    panel.h = UIPanel_ViewSummaryReservedHeight(ui);
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

    font_h = UIPanelViewSummary_FontHeight();
    line_gap = UIPanelViewSummary_LineGap();
    panel_pad = UIPanelViewSummary_PanelPad();
    y = panel.y + panel_pad;

    UIPanelViewSummary_DrawText(renderer, font, "View / Editing", panel.x + 8, y, label_color);
    y += font_h + line_gap;

    snprintf(line_mode,
             sizeof(line_mode),
             "Space  %s",
             Global_GetSpaceModeLabel(state->spaceMode));
    snprintf(line_zoom,
             sizeof(line_zoom),
             "Zoom  %.2fx   Grid %.2f",
             state->grid.scale,
             state->grid.gridSize);
    snprintf(line_plane,
             sizeof(line_plane),
             "Plane  %s (%s=%.2f)",
             UIPanel_ViewPlaneAxisLabel(UIPanel_CurrentConstructionViewPlane(state).axis),
             UIPanel_ViewPlaneCoordinateLabel(UIPanel_CurrentConstructionViewPlane(state).axis),
             UIPanel_CurrentConstructionViewPlane(state).offset);
    snprintf(line_delete,
             sizeof(line_delete),
             "Delete  %s",
             UIPanelViewSummary_DeleteModeLabel(state->editor.deleteMode));
    UIPanelViewSummary_BuildSelectionLine(state, line_selection, sizeof(line_selection));

    UIPanelViewSummary_DrawTextClipped(renderer, font, line_mode, panel.x + 8, y, panel.w - 16, accent_color);
    y += font_h + line_gap;
    SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, 90);
    SDL_RenderDrawLine(renderer, panel.x + 8, y - (line_gap / 2), panel.x + panel.w - 8, y - (line_gap / 2));
    UIPanelViewSummary_DrawTextClipped(renderer, font, line_zoom, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelViewSummary_DrawTextClipped(renderer, font, line_plane, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelViewSummary_DrawTextClipped(renderer, font, line_delete, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelViewSummary_DrawTextClipped(renderer, font, line_selection, panel.x + 8, y, panel.w - 16, label_color);
}
