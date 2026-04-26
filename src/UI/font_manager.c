#include "UI/font_manager.h"

#include "UI/font_bridge.h"
#include "UI/text_draw.h"

#include <SDL2/SDL.h>
#include <string.h>

static TTF_Font* fonts[FONT_COUNT] = {0};
static char g_font_paths[FONT_COUNT][384];
static int g_font_point_sizes[FONT_COUNT] = {0};
static int g_font_kerning_enabled[FONT_COUNT] = {0};
static int g_font_zoom_step = 0;

static int FontManager_ClampZoomStep(int step) {
    if (step < -2) return -2;
    if (step > 4) return 4;
    return step;
}

static void FontManager_ConfigureFont(TTF_Font* font, int kerning_enabled) {
    if (!font) return;
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    TTF_SetFontKerning(font, kerning_enabled ? 1 : 0);
}

static void FontManager_ClearLoadedFonts(void) {
    int i = 0;
    for (i = 0; i < FONT_COUNT; ++i) {
        if (fonts[i]) {
            line_drawing_text_unregister_font_source(fonts[i]);
            TTF_CloseFont(fonts[i]);
            fonts[i] = NULL;
        }
        g_font_paths[i][0] = '\0';
        g_font_point_sizes[i] = 0;
        g_font_kerning_enabled[i] = 0;
    }
}

static LineDrawingFontSlot FontManager_SlotForId(UIFontID id) {
    switch (id) {
        case FONT_MONO:
            return LINE_DRAWING_FONT_SLOT_MONO;
        case FONT_SERIF:
            return LINE_DRAWING_FONT_SLOT_SERIF;
        case FONT_DEFAULT:
        default:
            return LINE_DRAWING_FONT_SLOT_DEFAULT;
    }
}

static bool FontManager_LoadSlot(UIFontID id,
                                 TTF_Font** out_font,
                                 char* out_path,
                                 size_t out_path_size,
                                 int* out_point_size,
                                 int* out_kerning_enabled) {
    LineDrawingResolvedFont resolved;
    TTF_Font* font = NULL;
    CoreResult result;

    if (!out_font || !out_path || out_path_size == 0 || !out_point_size || !out_kerning_enabled) {
        return false;
    }

    result = line_drawing_font_bridge_resolve(FontManager_SlotForId(id),
                                              g_font_zoom_step,
                                              &resolved);
    if (result.code != CORE_OK) {
        SDL_Log("FontManager: bridge resolve failed for slot %d: %s",
                (int)id,
                result.message ? result.message : "unknown");
        return false;
    }

    font = TTF_OpenFont(resolved.resolved_path, resolved.text_run.logical_point_size);
    if (!font) {
        SDL_Log("FontManager: failed to load font %s (%dpt): %s",
                resolved.resolved_path,
                resolved.text_run.logical_point_size,
                TTF_GetError());
        return false;
    }

    FontManager_ConfigureFont(font, resolved.text_run.kerning_enabled);
    SDL_strlcpy(out_path, resolved.resolved_path, out_path_size);
    *out_font = font;
    *out_point_size = resolved.text_run.logical_point_size;
    *out_kerning_enabled = resolved.text_run.kerning_enabled;
    return true;
}

bool FontManager_Init(void) {
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    return true;
}

void FontManager_Quit(void) {
    FontManager_ClearLoadedFonts();
    TTF_Quit();
}

bool FontManager_LoadFonts(void) {
    return FontManager_ReloadFonts();
}

bool FontManager_ReloadFonts(void) {
    TTF_Font* new_fonts[FONT_COUNT] = {0};
    char new_paths[FONT_COUNT][384];
    int new_point_sizes[FONT_COUNT] = {0};
    int new_kerning_enabled[FONT_COUNT] = {0};
    int i = 0;

    memset(new_paths, 0, sizeof(new_paths));

    for (i = 0; i < FONT_COUNT; ++i) {
        if (!FontManager_LoadSlot((UIFontID)i,
                                  &new_fonts[i],
                                  new_paths[i],
                                  sizeof(new_paths[i]),
                                  &new_point_sizes[i],
                                  &new_kerning_enabled[i])) {
            int j = 0;
            for (j = 0; j < FONT_COUNT; ++j) {
                if (new_fonts[j]) {
                    TTF_CloseFont(new_fonts[j]);
                }
            }
            return false;
        }
    }

    FontManager_ClearLoadedFonts();
    for (i = 0; i < FONT_COUNT; ++i) {
        fonts[i] = new_fonts[i];
        SDL_strlcpy(g_font_paths[i], new_paths[i], sizeof(g_font_paths[i]));
        g_font_point_sizes[i] = new_point_sizes[i];
        g_font_kerning_enabled[i] = new_kerning_enabled[i];
        line_drawing_text_register_font_source(fonts[i],
                                               g_font_paths[i],
                                               g_font_point_sizes[i],
                                               g_font_point_sizes[i],
                                               g_font_kerning_enabled[i]);
    }

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
    (void)scale;
    if (out_raster_scale) {
        *out_raster_scale = 1.0f;
    }
    return FontManager_Get(id);
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
