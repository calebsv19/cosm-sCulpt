#include "UI/text_draw.h"

#include "kit_render_external_text.h"

#include <SDL2/SDL.h>

void line_drawing_text_register_font_source(TTF_Font* font,
                                            const char* path,
                                            int logical_point_size,
                                            int loaded_point_size,
                                            int kerning_enabled) {
    kit_render_external_text_register_font_source(font,
                                                  path,
                                                  logical_point_size,
                                                  loaded_point_size,
                                                  kerning_enabled);
}

void line_drawing_text_unregister_font_source(TTF_Font* font) {
    kit_render_external_text_unregister_font_source(font);
}

void line_drawing_text_reset_renderer(SDL_Renderer* renderer) {
    kit_render_external_text_reset_renderer(renderer);
}

int line_drawing_text_measure_utf8(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const char* text,
                                   int* out_w,
                                   int* out_h) {
#if USE_VULKAN
    return kit_render_external_text_measure_utf8(renderer, font, text, out_w, out_h);
#else
    int width = 0;
    int height = 0;
    (void)renderer;
    if (!font || !text) {
        return 0;
    }
    if (text[0] == '\0') {
        height = TTF_FontHeight(font);
        if (height <= 0) {
            return 0;
        }
        if (out_w) *out_w = 0;
        if (out_h) *out_h = height;
        return 1;
    }
    if (TTF_SizeUTF8(font, text, &width, &height) != 0) {
        return 0;
    }
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    return 1;
#endif
}

int line_drawing_text_draw_utf8(SDL_Renderer* renderer,
                                TTF_Font* font,
                                const char* text,
                                SDL_Color color,
                                SDL_Rect* io_dst) {
#if USE_VULKAN
    return kit_render_external_text_draw_utf8(renderer, font, text, color, io_dst);
#else
    SDL_Surface* surf = NULL;
    SDL_Texture* tex = NULL;
    SDL_Rect dst = {0, 0, 0, 0};

    if (!renderer || !font || !text || !text[0] || !io_dst) {
        return 0;
    }

    surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) {
        return 0;
    }

    tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return 0;
    }

    dst = *io_dst;
    if (dst.w <= 0) {
        dst.w = surf->w;
    }
    if (dst.h <= 0) {
        dst.h = surf->h;
    }
    if (dst.w < 1) {
        dst.w = 1;
    }
    if (dst.h < 1) {
        dst.h = 1;
    }

    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
    *io_dst = dst;
    return 1;
#endif
}

int line_drawing_text_draw_utf8_at(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const char* text,
                                   int x,
                                   int y,
                                   SDL_Color color) {
    return kit_render_external_text_draw_utf8_at(renderer, font, text, x, y, color);
}
