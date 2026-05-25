#include "UI/ui_panel_visual_style.h"

#include "UI/shared_theme_font_adapter.h"

static Uint8 UIPanelVisual_ClampColorChannel(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (Uint8)value;
}

UIPanelVisualMetrics UIPanelVisual_MakeMetrics(TTF_Font* font) {
    UIPanelVisualMetrics metrics = {0};
    int font_h = 14;

    if (font) {
        font_h = TTF_FontHeight(font);
    }
    if (font_h < 12) font_h = 12;

    metrics.pad_x = 8;
    metrics.pad_y = 7;
    metrics.section_gap = 6;
    metrics.line_h = font_h + 4;
    metrics.row_h = font_h + 10;
    if (metrics.row_h < 24) metrics.row_h = 24;
    metrics.tab_h = font_h + 10;
    if (metrics.tab_h < 26) metrics.tab_h = 26;
    metrics.tab_gap = 6;
    metrics.row_text_y = (metrics.row_h - font_h) / 2;
    if (metrics.row_text_y < 3) metrics.row_text_y = 3;
    metrics.tab_text_y = (metrics.tab_h - font_h) / 2;
    if (metrics.tab_text_y < 3) metrics.tab_text_y = 3;
    metrics.chip_h = font_h + 4;
    metrics.accent_h = 4;
    return metrics;
}

bool UIPanelVisual_ResolvePalette(UIPanelVisualPalette* out_palette) {
    LineDrawing3dThemePalette palette = {0};
    UIPanelVisualPalette resolved = {
        .pane_fill = {18, 20, 25, 215},
        .workspace_fill = {14, 16, 20, 222},
        .pane_border = {78, 90, 108, 220},
        .pane_divider = {62, 72, 88, 210},
        .button_fill = {70, 70, 70, 200},
        .button_fill_hover = {82, 82, 82, 220},
        .button_fill_active = {94, 98, 108, 225},
        .button_border = {180, 180, 180, 255},
        .text_primary = {255, 255, 255, 255},
        .text_muted = {200, 200, 210, 255},
        .accent = {140, 170, 210, 255}
    };

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        resolved.pane_fill = palette.panel_fill;
        resolved.pane_fill.a = 215;
        resolved.workspace_fill = UIPanelVisual_BlendColor(palette.panel_fill,
                                                           (SDL_Color){0, 0, 0, 255},
                                                           72);
        resolved.workspace_fill.a = 222;
        resolved.pane_border = palette.panel_border;
        resolved.pane_divider = UIPanelVisual_AdjustColor(palette.panel_border, -10, -20);
        resolved.button_fill = palette.button_fill;
        resolved.button_fill_hover = UIPanelVisual_AdjustColor(palette.button_fill, 12, 20);
        resolved.button_fill_active = UIPanelVisual_BlendColor(palette.button_fill, palette.button_border, 54);
        resolved.button_border = palette.button_border;
        resolved.text_primary = palette.button_text;
        resolved.text_muted = palette.text_muted;
        resolved.accent = palette.button_border;
    }

    if (UIPanelVisual_ColorLuma(resolved.workspace_fill) >= UIPanelVisual_ColorLuma(resolved.pane_fill)) {
        resolved.workspace_fill = UIPanelVisual_AdjustColor(resolved.pane_fill, -6, 8);
    }

    if (out_palette) *out_palette = resolved;
    return true;
}

SDL_Color UIPanelVisual_AdjustColor(SDL_Color color, int delta_rgb, int delta_alpha) {
    color.r = UIPanelVisual_ClampColorChannel((int)color.r + delta_rgb);
    color.g = UIPanelVisual_ClampColorChannel((int)color.g + delta_rgb);
    color.b = UIPanelVisual_ClampColorChannel((int)color.b + delta_rgb);
    color.a = UIPanelVisual_ClampColorChannel((int)color.a + delta_alpha);
    return color;
}

SDL_Color UIPanelVisual_BlendColor(SDL_Color a, SDL_Color b, Uint8 mix_b) {
    const Uint16 mix_a = (Uint16)(255 - mix_b);
    SDL_Color result = {0};
    result.r = (Uint8)(((Uint16)a.r * mix_a + (Uint16)b.r * mix_b) / 255);
    result.g = (Uint8)(((Uint16)a.g * mix_a + (Uint16)b.g * mix_b) / 255);
    result.b = (Uint8)(((Uint16)a.b * mix_a + (Uint16)b.b * mix_b) / 255);
    result.a = (Uint8)(((Uint16)a.a * mix_a + (Uint16)b.a * mix_b) / 255);
    return result;
}

int UIPanelVisual_ColorLuma(SDL_Color color) {
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

void UIPanelVisual_DrawFrame(SDL_Renderer* renderer,
                             SDL_Rect rect,
                             SDL_Color fill,
                             SDL_Color border,
                             Uint8 inner_border_alpha) {
    SDL_Rect inner = rect;
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);

    inner.x += 1;
    inner.y += 1;
    inner.w -= 2;
    inner.h -= 2;
    if (inner_border_alpha > 0 && inner.w > 0 && inner.h > 0) {
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, inner_border_alpha);
        SDL_RenderDrawRect(renderer, &inner);
    }
}

void UIPanelVisual_DrawAccentBand(SDL_Renderer* renderer,
                                  SDL_Rect rect,
                                  SDL_Color accent,
                                  int inset,
                                  int height,
                                  Uint8 alpha) {
    SDL_Rect band = rect;
    if (!renderer || rect.w <= 0 || rect.h <= 0 || height <= 0) return;
    if (inset < 0) inset = 0;
    band.x += inset;
    band.y += inset;
    band.w -= inset * 2;
    band.h = height;
    if (band.w <= 0) return;
    if (band.h > rect.h - inset * 2) band.h = rect.h - inset * 2;
    if (band.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, alpha);
    SDL_RenderFillRect(renderer, &band);
}

void UIPanelVisual_DrawLabelChip(SDL_Renderer* renderer,
                                 SDL_Rect rect,
                                 SDL_Color fill,
                                 SDL_Color border,
                                 Uint8 fill_alpha,
                                 Uint8 border_alpha) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill_alpha);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border_alpha);
    SDL_RenderDrawRect(renderer, &rect);
}

void UIPanelVisual_DrawDividerLine(SDL_Renderer* renderer,
                                   int x0,
                                   int x1,
                                   int y,
                                   SDL_Color color,
                                   Uint8 alpha) {
    if (!renderer || x1 < x0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
    SDL_RenderDrawLine(renderer, x0, y, x1, y);
}

void UIPanelVisual_DrawInteractiveRow(SDL_Renderer* renderer,
                                      SDL_Rect rect,
                                      SDL_Color fill,
                                      SDL_Color border,
                                      SDL_Color accent,
                                      bool hovered,
                                      bool active,
                                      int accent_width,
                                      Uint8 inner_border_alpha) {
    SDL_Rect accent_rect = rect;
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    UIPanelVisual_DrawFrame(renderer, rect, fill, border, inner_border_alpha);
    if (!(hovered || active)) return;
    if (accent_width <= 0) accent_width = 3;
    if (accent_width > rect.w) accent_width = rect.w;
    accent_rect.w = accent_width;
    UIPanelVisual_DrawAccentBand(renderer, accent_rect, accent, 0, accent_rect.h, 235);
}

void UIPanelVisual_DrawScrollbar(SDL_Renderer* renderer,
                                 SDL_Rect track,
                                 SDL_Rect thumb,
                                 SDL_Color track_fill,
                                 SDL_Color track_border,
                                 SDL_Color thumb_fill,
                                 SDL_Color thumb_border,
                                 bool dragging) {
    if (!renderer || track.w <= 0 || track.h <= 0 || thumb.w <= 0 || thumb.h <= 0) return;
    UIPanelVisual_DrawFrame(renderer, track, track_fill, track_border, 0);
    SDL_SetRenderDrawColor(renderer,
                           thumb_fill.r,
                           thumb_fill.g,
                           thumb_fill.b,
                           dragging ? 255 : thumb_fill.a);
    SDL_RenderFillRect(renderer, &thumb);
    SDL_SetRenderDrawColor(renderer,
                           thumb_border.r,
                           thumb_border.g,
                           thumb_border.b,
                           dragging ? 255 : thumb_border.a);
    SDL_RenderDrawRect(renderer, &thumb);
}
