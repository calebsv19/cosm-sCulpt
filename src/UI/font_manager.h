#pragma once
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

typedef enum {
    FONT_DEFAULT,
    FONT_MONO,
    FONT_SERIF,
    FONT_COUNT
} UIFontID;

// Call once at startup
bool FontManager_Init(void);

// Call on shutdown
void FontManager_Quit(void);

// Load fonts from project folder
bool FontManager_LoadFonts(void);

// Get font by ID
TTF_Font* FontManager_Get(UIFontID id);

