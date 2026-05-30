#include "UI/overlay/ui_panel_overlay_render_internal.h"

#include "UI/font_manager.h"
#include "UI/text_draw.h"

#include <stdio.h>
#include <string.h>

void UIPanelOverlay_DrawText(SDL_Renderer* renderer,
                             const char* text,
                             int x,
                             int y,
                             SDL_Color color) {
    if (!text || !*text) return;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

void UIPanelOverlay_DrawTextClipped(SDL_Renderer* renderer,
                                    const char* text,
                                    int x,
                                    int y,
                                    int max_width,
                                    SDL_Color color) {
    TTF_Font* font = NULL;
    static const char* k_ellipsis = "...";
    char clipped[512];
    size_t len = 0u;
    int text_w = 0;
    int ellipsis_w = 0;

    if (!text || !text[0] || max_width <= 0) return;
    font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

    if (line_drawing_text_measure_utf8(renderer, font, text, &text_w, NULL) &&
        text_w <= max_width) {
        UIPanelOverlay_DrawText(renderer, text, x, y, color);
        return;
    }

    if (!line_drawing_text_measure_utf8(renderer, font, k_ellipsis, &ellipsis_w, NULL) ||
        ellipsis_w >= max_width) {
        return;
    }

    len = strlen(text);
    while (len > 0u) {
        --len;
        if (len + strlen(k_ellipsis) + 1u >= sizeof(clipped)) continue;
        memcpy(clipped, text, len);
        clipped[len] = '\0';
        strcat(clipped, k_ellipsis);
        if (line_drawing_text_measure_utf8(renderer, font, clipped, &text_w, NULL) &&
            text_w <= max_width) {
            UIPanelOverlay_DrawText(renderer, clipped, x, y, color);
            return;
        }
    }
}
