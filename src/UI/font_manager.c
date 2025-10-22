#include "UI/font_manager.h"
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
    fonts[FONT_DEFAULT] = TTF_OpenFont("include/fonts/Lato/Lato-Regular.ttf", 14);
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

