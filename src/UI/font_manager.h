#pragma once
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>

#include "core_font.h"

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
bool FontManager_ReloadFonts(void);

// Get font by ID
TTF_Font* FontManager_Get(UIFontID id);
TTF_Font* FontManager_GetUIPanelFont(void);
bool FontManager_CopyFontPath(UIFontID id, char* out_path, size_t out_path_size);
int FontManager_GetPointSize(UIFontID id);
UIFontID FontManager_FindIdForFont(TTF_Font* font);
TTF_Font* FontManager_GetRasterFontForScale(UIFontID id, float scale, float* out_raster_scale);

int FontManager_GetZoomStep(void);
bool FontManager_SetZoomStep(int step);
bool FontManager_AdjustZoomStep(int delta);
CoreFontPresetId FontManager_GetSharedFontPresetId(void);
const char* FontManager_GetSharedFontPresetName(void);
bool FontManager_SetSharedFontPresetId(CoreFontPresetId preset_id);
bool FontManager_SetSharedFontPresetName(const char* preset_name);
