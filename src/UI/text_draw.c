#include "UI/text_draw.h"
#include "UI/text_draw_private.h"
#include "kit_render_external_text.h"
#if USE_VULKAN
#include "vk_renderer.h"
#endif
static SDL_Renderer* measurement_renderer;

/* Bind actual drawable density before measure-only layout. */
void line_drawing_text_bind_renderer(SDL_Renderer* renderer) { measurement_renderer = renderer; }

void line_drawing_text_register_font_source(TTF_Font* font, const char* path,
                                           int logical, int loaded, int kerning) {
    line_drawing_sdl_font_register(font, path, logical, kerning);
    kit_render_external_text_register_font_source_native_pixels(font, path, logical, loaded, kerning);
}

void line_drawing_text_unregister_font_source(TTF_Font* font) {
    line_drawing_fallback_reset(NULL, font);
    line_drawing_sdl_font_unregister(font);
    kit_render_external_text_unregister_font_source(font);
}

void line_drawing_text_reset_renderer(SDL_Renderer* renderer) {
    if (measurement_renderer == renderer) measurement_renderer = NULL;
#if USE_VULKAN
    if (renderer) vk_renderer_wait_idle((VkRenderer*)renderer);
#endif
    line_drawing_fallback_reset(renderer, NULL);
    kit_render_external_text_reset_renderer(renderer);
}

int line_drawing_text_measure_utf8(SDL_Renderer* renderer, TTF_Font* font,
                                  const char* text, int* width, int* height) {
    if (!renderer) renderer = measurement_renderer;
#if USE_VULKAN
    int fallback = line_drawing_fallback_measure(renderer, font, text, width, height);
    if (fallback) return fallback > 0;
    return kit_render_external_text_measure_utf8(renderer, font, text, width, height);
#else
    return line_drawing_sdl_text_measure(renderer, font, text, width, height);
#endif
}

int line_drawing_text_draw_utf8(SDL_Renderer* renderer, TTF_Font* font,
                               const char* text, SDL_Color color, SDL_Rect* dst) {
#if USE_VULKAN
    int fallback = line_drawing_fallback_draw(renderer, font, text, color, dst);
    if (fallback) return fallback > 0;
    return kit_render_external_text_draw_utf8(renderer, font, text, color, dst);
#else
    return line_drawing_sdl_text_draw(renderer, font, text, color, dst, 0);
#endif
}

int line_drawing_text_draw_utf8_at(SDL_Renderer* renderer, TTF_Font* font,
                                  const char* text, int x, int y, SDL_Color color) {
    SDL_Rect dst = {x, y, 0, 0};
    return line_drawing_text_draw_utf8(renderer, font, text, color, &dst);
}

int line_drawing_text_size_utf8(TTF_Font* font, const char* text, int* width, int* height) {
    return line_drawing_text_measure_utf8(NULL, font, text, width, height) ? 0 : -1;
}

int line_drawing_text_font_height(TTF_Font* font) {
    int height = 0;
    (void)line_drawing_text_measure_utf8(NULL, font, "", NULL, &height);
    return height;
}

int line_drawing_text_draw_utf8_clipped(SDL_Renderer* renderer, TTF_Font* font,
                                      const char* text, SDL_Color ink, SDL_Rect* dst, int width) {
    if (!renderer || !dst || width <= 0) return 0;
#if USE_VULKAN
    SDL_Rect old, clip = {dst->x, 0, width, 100000};
    SDL_bool enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old);
    if (enabled && !SDL_IntersectRect(&clip, &old, &clip)) return 1;
    SDL_RenderSetClipRect(renderer, &clip);
    int result = line_drawing_text_draw_utf8(renderer, font, text, ink, dst);
    SDL_RenderSetClipRect(renderer, enabled ? &old : NULL);
    return result;
#else
    return line_drawing_sdl_text_draw(renderer, font, text, ink, dst, width);
#endif
}
