#include "UI/ui_panel_file_summary.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelFileSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelFileSummary_LineGap(void) {
    int gap = UIPanelFileSummary_FontHeight() / 3;
    if (gap < 4) gap = 4;
    return gap;
}

static int UIPanelFileSummary_PanelPad(void) {
    int pad = UIPanelFileSummary_FontHeight() / 2;
    if (pad < 8) pad = 8;
    return pad;
}

static void UIPanelFileSummary_DrawText(SDL_Renderer* renderer,
                                        TTF_Font* font,
                                        const char* text,
                                        int x,
                                        int y,
                                        SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void UIPanelFileSummary_DrawTextClipped(SDL_Renderer* renderer,
                                               TTF_Font* font,
                                               const char* text,
                                               int x,
                                               int y,
                                               int maxWidth,
                                               SDL_Color color) {
    char clipped[384];
    int width = 0;
    if (!renderer || !font || !text || maxWidth <= 0) return;
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) && width <= maxWidth) {
        UIPanelFileSummary_DrawText(renderer, font, text, x, y, color);
        return;
    }
    snprintf(clipped, sizeof(clipped), "%s", text);
    while (strlen(clipped) > 3) {
        size_t len = strlen(clipped);
        clipped[len - 1] = '\0';
        clipped[len - 2] = '.';
        clipped[len - 3] = '.';
        clipped[len - 4] = '.';
        if (line_drawing_text_measure_utf8(renderer, font, clipped, &width, NULL) && width <= maxWidth) {
            UIPanelFileSummary_DrawText(renderer, font, clipped, x, y, color);
            return;
        }
    }
}

static const char* UIPanelFileSummary_BaseName(const char* path) {
    const char* base = NULL;
    if (!path || !path[0]) return "(unset)";
    base = strrchr(path, '/');
    return base ? (base + 1) : path;
}

int UIPanel_FileSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeLeftTab != UI_PANEL_LEFT_TAB_FILE) return 0;
    font_h = UIPanelFileSummary_FontHeight();
    line_gap = UIPanelFileSummary_LineGap();
    pad = UIPanelFileSummary_PanelPad();
    return (pad * 2) + (font_h * 6) + (line_gap * 5);
}

void Render_UIPanelFileSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect panel = {0, 0, 0, 0};
    int font_h = 0;
    int line_gap = 0;
    int panel_pad = 0;
    int y = 0;
    char line_layout[192];
    char line_scene[192];
    char line_input[384];
    char line_output[384];
    char line_status[128];

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_FILE) return;
    if (ui->leftBodyRect.w <= 0 || ui->leftBodyRect.h <= 0) return;

    panel = ui->leftBodyRect;
    panel.h = UIPanel_FileSummaryReservedHeight(ui);
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

    font_h = UIPanelFileSummary_FontHeight();
    line_gap = UIPanelFileSummary_LineGap();
    panel_pad = UIPanelFileSummary_PanelPad();
    y = panel.y + panel_pad;

    UIPanelFileSummary_DrawText(renderer, font, "File / Session", panel.x + 8, y, label_color);
    y += font_h + line_gap;

    snprintf(line_layout,
             sizeof(line_layout),
             "Layout  %s",
             UIPanelFileSummary_BaseName(Global_GetCurrentConfigPath()));
    snprintf(line_scene,
             sizeof(line_scene),
             "Scene  %s",
             UIPanelFileSummary_BaseName(Global_GetCurrentSceneAuthoringPath()));
    snprintf(line_input,
             sizeof(line_input),
             "Input  %s",
             Global_GetInputRoot() ? Global_GetInputRoot() : "(unset)");
    snprintf(line_output,
             sizeof(line_output),
             "Output  %s",
             Global_GetOutputRoot() ? Global_GetOutputRoot() : "(unset)");
    snprintf(line_status,
             sizeof(line_status),
             "Status  %s",
             state->layoutDirtySinceSave ? "Modified" : "Clean");

    UIPanelFileSummary_DrawTextClipped(renderer, font, line_layout, panel.x + 8, y, panel.w - 16, accent_color);
    y += font_h + line_gap;
    SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, 90);
    SDL_RenderDrawLine(renderer, panel.x + 8, y - (line_gap / 2), panel.x + panel.w - 8, y - (line_gap / 2));
    UIPanelFileSummary_DrawTextClipped(renderer, font, line_scene, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelFileSummary_DrawTextClipped(renderer, font, line_input, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelFileSummary_DrawTextClipped(renderer, font, line_output, panel.x + 8, y, panel.w - 16, value_color);
    y += font_h + line_gap;
    UIPanelFileSummary_DrawTextClipped(renderer, font, line_status, panel.x + 8, y, panel.w - 16, label_color);
}
