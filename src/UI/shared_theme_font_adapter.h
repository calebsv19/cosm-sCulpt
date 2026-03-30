#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct LineDrawing3dThemePalette {
    SDL_Color background_fill;
    SDL_Color panel_fill;
    SDL_Color panel_border;
    SDL_Color button_fill;
    SDL_Color button_border;
    SDL_Color button_text;
    SDL_Color text_primary;
    SDL_Color text_muted;
    SDL_Color menu_highlight;
    SDL_Color modal_scrim;
} LineDrawing3dThemePalette;

bool line_drawing3d_shared_theme_resolve_palette(LineDrawing3dThemePalette* out_palette);
bool line_drawing3d_shared_font_resolve_ui_regular(char* out_path, size_t out_path_size, int* out_point_size);
bool line_drawing3d_shared_theme_cycle_next(void);
bool line_drawing3d_shared_theme_cycle_prev(void);
bool line_drawing3d_shared_theme_set_preset(const char* preset_name);
bool line_drawing3d_shared_theme_current_preset(char* out_name, size_t out_name_size);
bool line_drawing3d_shared_theme_load_persisted(void);
bool line_drawing3d_shared_theme_save_persisted(void);
