#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include "UI/text_draw.h"

void UIPanelSummary_DrawText(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const char* text,
                             int x,
                             int y,
                             SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

void UIPanelSummary_DrawTextClipped(SDL_Renderer* renderer,
                                    TTF_Font* font,
                                    const char* text,
                                    int x,
                                    int y,
                                    int max_width,
                                    int clip_height,
                                    SDL_Color color) {
    int width = 0;
    SDL_Rect clip = { x, y - 2, max_width, clip_height };
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool had_clip = SDL_FALSE;

    if (!renderer || !font || !text || !text[0] || max_width <= 0 || clip_height <= 0) return;
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) &&
        width <= max_width) {
        UIPanelSummary_DrawText(renderer, font, text, x, y, color);
        return;
    }

    had_clip = SDL_RenderIsClipEnabled(renderer);
    if (had_clip) {
        (void)SDL_RenderGetClipRect(renderer, &previous_clip);
    }
    SDL_RenderSetClipRect(renderer, &clip);
    UIPanelSummary_DrawText(renderer, font, text, x, y, color);
    SDL_RenderSetClipRect(renderer, had_clip ? &previous_clip : NULL);
}

void UIPanelSummary_DrawCard(SDL_Renderer* renderer,
                             SDL_Rect rect,
                             SDL_Color fill_color,
                             SDL_Color border_color,
                             SDL_Color accent_color,
                             int accent_height) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    UIPanelVisual_DrawFrame(renderer, rect, fill_color, border_color, 70);
    if (accent_height > 0) {
        UIPanelVisual_DrawAccentBand(renderer, rect, accent_color, 1, accent_height, 220);
    }
}

void UIPanelSummary_DrawDivider(SDL_Renderer* renderer,
                                SDL_Rect rect,
                                int y,
                                int inset_x,
                                SDL_Color color,
                                Uint8 alpha) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    if (inset_x < 0) inset_x = 0;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
    SDL_RenderDrawLine(renderer,
                       rect.x + inset_x,
                       y,
                       rect.x + rect.w - inset_x,
                       y);
}
