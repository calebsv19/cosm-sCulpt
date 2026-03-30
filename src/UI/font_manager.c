#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static TTF_Font* fonts[FONT_COUNT] = {0};

bool FontManager_Init(void) {
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    return true;
}

void FontManager_Quit(void) {
    for (int i = 0; i < FONT_COUNT; ++i) {
        if (fonts[i]) {
            TTF_CloseFont(fonts[i]);
            fonts[i] = NULL;
        }
    }
    TTF_Quit();
}

bool FontManager_LoadFonts(void) {
    char shared_font_path[256];
    int shared_point_size = 14;
    const char* default_path = "include/fonts/Lato/Lato-Regular.ttf";
    int default_size = 14;
    if (line_drawing3d_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &shared_point_size)) {
        default_path = shared_font_path;
        default_size = shared_point_size;
    }

    fonts[FONT_DEFAULT] = TTF_OpenFont(default_path, default_size);
    fonts[FONT_MONO]    = TTF_OpenFont("include/fonts/Montserrat/static/Montserrat-Regular.ttf", 14);
    fonts[FONT_SERIF]   = TTF_OpenFont("include/fonts/Montserrat/static/Montserrat-Italic.ttf", 14);

    for (int i = 0; i < FONT_COUNT; ++i) {
        if (!fonts[i]) {
            SDL_Log("FontManager: Failed to load font %d: %s", i, TTF_GetError());
            return false;
        }
    }

    return true;
}

TTF_Font* FontManager_Get(UIFontID id) {
    if (id < 0 || id >= FONT_COUNT) return NULL;
    return fonts[id];
}
