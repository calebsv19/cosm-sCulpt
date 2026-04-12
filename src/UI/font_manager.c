#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static TTF_Font* fonts[FONT_COUNT] = {0};
static char g_font_paths[FONT_COUNT][256];
static int g_font_point_sizes[FONT_COUNT] = {0};
enum { FONT_RENDER_CACHE_CAPACITY = 8 };
static TTF_Font* g_raster_fonts[FONT_COUNT][FONT_RENDER_CACHE_CAPACITY] = {{0}};
static int g_raster_point_sizes[FONT_COUNT][FONT_RENDER_CACHE_CAPACITY] = {{0}};
static int g_font_zoom_step = 0;

static void FontManager_ConfigureFont(TTF_Font* font) {
    if (!font) return;
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    TTF_SetFontKerning(font, 1);
}

static int FontManager_ClampZoomStep(int step) {
    if (step < -2) return -2;
    if (step > 4) return 4;
    return step;
}

static int FontManager_AdjustPointSize(int base_size) {
    int adjusted = base_size + (g_font_zoom_step * 2);
    if (adjusted < 8) adjusted = 8;
    if (adjusted > 48) adjusted = 48;
    return adjusted;
}

static void FontManager_ClearRasterCacheForId(UIFontID id) {
    int i = 0;
    if (id < 0 || id >= FONT_COUNT) return;
    for (i = 0; i < FONT_RENDER_CACHE_CAPACITY; ++i) {
        if (g_raster_fonts[id][i]) {
            TTF_CloseFont(g_raster_fonts[id][i]);
            g_raster_fonts[id][i] = NULL;
        }
        g_raster_point_sizes[id][i] = 0;
    }
}

static void FontManager_ClearRasterCache(void) {
    int id = 0;
    for (id = 0; id < FONT_COUNT; ++id) {
        FontManager_ClearRasterCacheForId((UIFontID)id);
    }
}

static bool FontManager_OpenFontsForCurrentStep(TTF_Font* out_fonts[FONT_COUNT]) {
    char shared_font_path[256];
    int shared_point_size = 14;
    const char* default_path = "include/fonts/Lato/Lato-Regular.ttf";
    int default_size = 14;
    int mono_size = 13;
    int serif_size = 13;
    int i = 0;

    if (!out_fonts) return false;
    for (i = 0; i < FONT_COUNT; ++i) out_fonts[i] = NULL;

    if (line_drawing3d_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &shared_point_size)) {
        default_path = shared_font_path;
        default_size = shared_point_size;
        mono_size = shared_point_size - 1;
        serif_size = shared_point_size - 1;
    }

    default_size = FontManager_AdjustPointSize(default_size);
    mono_size = FontManager_AdjustPointSize(mono_size);
    serif_size = FontManager_AdjustPointSize(serif_size);

    out_fonts[FONT_DEFAULT] = TTF_OpenFont(default_path, default_size);
    out_fonts[FONT_MONO] = TTF_OpenFont("include/fonts/Montserrat/static/Montserrat-Regular.ttf", mono_size);
    out_fonts[FONT_SERIF] = TTF_OpenFont("include/fonts/Montserrat/static/Montserrat-Italic.ttf", serif_size);

    for (i = 0; i < FONT_COUNT; ++i) {
        if (!out_fonts[i]) {
            SDL_Log("FontManager: Failed to load font %d at zoom step %d: %s",
                    i,
                    g_font_zoom_step,
                    TTF_GetError());
            for (int j = 0; j < FONT_COUNT; ++j) {
                if (out_fonts[j]) {
                    TTF_CloseFont(out_fonts[j]);
                    out_fonts[j] = NULL;
                }
            }
            return false;
        }
        FontManager_ConfigureFont(out_fonts[i]);
    }
    return true;
}

static void FontManager_ReplaceFonts(TTF_Font* new_fonts[FONT_COUNT]) {
    int i = 0;
    FontManager_ClearRasterCache();
    for (i = 0; i < FONT_COUNT; ++i) {
        if (fonts[i]) {
            TTF_CloseFont(fonts[i]);
        }
        fonts[i] = new_fonts[i];
    }
}

bool FontManager_Init(void) {
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    return true;
}

void FontManager_Quit(void) {
    FontManager_ClearRasterCache();
    for (int i = 0; i < FONT_COUNT; ++i) {
        if (fonts[i]) {
            TTF_CloseFont(fonts[i]);
            fonts[i] = NULL;
        }
        g_font_paths[i][0] = '\0';
        g_font_point_sizes[i] = 0;
    }
    TTF_Quit();
}

bool FontManager_LoadFonts(void) {
    return FontManager_ReloadFonts();
}

bool FontManager_ReloadFonts(void) {
    TTF_Font* new_fonts[FONT_COUNT] = {0};
    int default_size = 14;
    int mono_size = 13;
    int serif_size = 13;
    char shared_font_path[256];
    const char* default_path = "include/fonts/Lato/Lato-Regular.ttf";
    const char* mono_path = "include/fonts/Montserrat/static/Montserrat-Regular.ttf";
    const char* serif_path = "include/fonts/Montserrat/static/Montserrat-Italic.ttf";
    if (!FontManager_OpenFontsForCurrentStep(new_fonts)) {
        return false;
    }
    if (line_drawing3d_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &default_size)) {
        default_path = shared_font_path;
        mono_size = default_size - 1;
        serif_size = default_size - 1;
    }
    default_size = FontManager_AdjustPointSize(default_size);
    mono_size = FontManager_AdjustPointSize(mono_size);
    serif_size = FontManager_AdjustPointSize(serif_size);
    FontManager_ReplaceFonts(new_fonts);
    SDL_strlcpy(g_font_paths[FONT_DEFAULT], default_path, sizeof(g_font_paths[FONT_DEFAULT]));
    SDL_strlcpy(g_font_paths[FONT_MONO], mono_path, sizeof(g_font_paths[FONT_MONO]));
    SDL_strlcpy(g_font_paths[FONT_SERIF], serif_path, sizeof(g_font_paths[FONT_SERIF]));
    g_font_point_sizes[FONT_DEFAULT] = default_size;
    g_font_point_sizes[FONT_MONO] = mono_size;
    g_font_point_sizes[FONT_SERIF] = serif_size;
    return true;
}

TTF_Font* FontManager_Get(UIFontID id) {
    if (id < 0 || id >= FONT_COUNT) return NULL;
    return fonts[id];
}

TTF_Font* FontManager_GetUIPanelFont(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (font) return font;
    return FontManager_Get(FONT_MONO);
}

bool FontManager_CopyFontPath(UIFontID id, char* out_path, size_t out_path_size) {
    if (id < 0 || id >= FONT_COUNT || !out_path || out_path_size == 0) {
        return false;
    }
    if (!g_font_paths[id][0]) {
        return false;
    }
    SDL_strlcpy(out_path, g_font_paths[id], out_path_size);
    return true;
}

int FontManager_GetPointSize(UIFontID id) {
    if (id < 0 || id >= FONT_COUNT) {
        return 0;
    }
    return g_font_point_sizes[id];
}

UIFontID FontManager_FindIdForFont(TTF_Font* font) {
    int i = 0;
    if (!font) {
        return FONT_COUNT;
    }
    for (i = 0; i < FONT_COUNT; ++i) {
        if (fonts[i] == font) {
            return (UIFontID)i;
        }
    }
    return FONT_COUNT;
}

TTF_Font* FontManager_GetRasterFontForScale(UIFontID id, float scale, float* out_raster_scale) {
    int point_size = 0;
    int requested_size = 0;
    int i = 0;
    if (out_raster_scale) {
        *out_raster_scale = 1.0f;
    }
    if (id < 0 || id >= FONT_COUNT || !fonts[id]) {
        return NULL;
    }
    point_size = g_font_point_sizes[id];
    if (point_size <= 0) {
        return fonts[id];
    }
    if (scale < 1.0f) {
        scale = 1.0f;
    }
    if (scale > 4.0f) {
        scale = 4.0f;
    }
    requested_size = (int)lroundf((float)point_size * scale);
    if (requested_size < point_size) {
        requested_size = point_size;
    }
    if (requested_size <= point_size || !g_font_paths[id][0]) {
        return fonts[id];
    }
    for (i = 0; i < FONT_RENDER_CACHE_CAPACITY; ++i) {
        if (g_raster_fonts[id][i] && g_raster_point_sizes[id][i] == requested_size) {
            if (out_raster_scale) {
                *out_raster_scale = (float)requested_size / (float)point_size;
            }
            return g_raster_fonts[id][i];
        }
    }
    for (i = 0; i < FONT_RENDER_CACHE_CAPACITY; ++i) {
        if (!g_raster_fonts[id][i]) {
            g_raster_fonts[id][i] = TTF_OpenFont(g_font_paths[id], requested_size);
            if (!g_raster_fonts[id][i]) {
                SDL_Log("FontManager: Failed to load raster font %d size %d: %s",
                        id,
                        requested_size,
                        TTF_GetError());
                return fonts[id];
            }
            FontManager_ConfigureFont(g_raster_fonts[id][i]);
            g_raster_point_sizes[id][i] = requested_size;
            if (out_raster_scale) {
                *out_raster_scale = (float)requested_size / (float)point_size;
            }
            return g_raster_fonts[id][i];
        }
    }
    if (out_raster_scale) {
        *out_raster_scale = (float)g_raster_point_sizes[id][0] / (float)point_size;
    }
    return g_raster_fonts[id][0] ? g_raster_fonts[id][0] : fonts[id];
}

int FontManager_GetZoomStep(void) {
    return g_font_zoom_step;
}

bool FontManager_SetZoomStep(int step) {
    int clamped = FontManager_ClampZoomStep(step);
    int previous = g_font_zoom_step;
    if (clamped == previous) {
        return true;
    }
    g_font_zoom_step = clamped;
    if (!FontManager_ReloadFonts()) {
        g_font_zoom_step = previous;
        (void)FontManager_ReloadFonts();
        return false;
    }
    return true;
}

bool FontManager_AdjustZoomStep(int delta) {
    return FontManager_SetZoomStep(g_font_zoom_step + delta);
}
