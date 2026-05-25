#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

typedef struct UIPanelVisualMetrics {
    int pad_x;
    int pad_y;
    int section_gap;
    int line_h;
    int row_h;
    int tab_h;
    int tab_gap;
    int row_text_y;
    int tab_text_y;
    int chip_h;
    int accent_h;
} UIPanelVisualMetrics;

typedef struct UIPanelVisualPalette {
    SDL_Color pane_fill;
    SDL_Color pane_border;
    SDL_Color pane_divider;
    SDL_Color button_fill;
    SDL_Color button_fill_hover;
    SDL_Color button_fill_active;
    SDL_Color button_border;
    SDL_Color text_primary;
    SDL_Color text_muted;
    SDL_Color accent;
} UIPanelVisualPalette;

UIPanelVisualMetrics UIPanelVisual_MakeMetrics(TTF_Font* font);
bool UIPanelVisual_ResolvePalette(UIPanelVisualPalette* out_palette);
SDL_Color UIPanelVisual_AdjustColor(SDL_Color color, int delta_rgb, int delta_alpha);
SDL_Color UIPanelVisual_BlendColor(SDL_Color a, SDL_Color b, Uint8 mix_b);
int UIPanelVisual_ColorLuma(SDL_Color color);
void UIPanelVisual_DrawFrame(SDL_Renderer* renderer,
                             SDL_Rect rect,
                             SDL_Color fill,
                             SDL_Color border,
                             Uint8 inner_border_alpha);
void UIPanelVisual_DrawAccentBand(SDL_Renderer* renderer,
                                  SDL_Rect rect,
                                  SDL_Color accent,
                                  int inset,
                                  int height,
                                  Uint8 alpha);
void UIPanelVisual_DrawLabelChip(SDL_Renderer* renderer,
                                 SDL_Rect rect,
                                 SDL_Color fill,
                                 SDL_Color border,
                                 Uint8 fill_alpha,
                                 Uint8 border_alpha);
void UIPanelVisual_DrawDividerLine(SDL_Renderer* renderer,
                                   int x0,
                                   int x1,
                                   int y,
                                   SDL_Color color,
                                   Uint8 alpha);
void UIPanelVisual_DrawInteractiveRow(SDL_Renderer* renderer,
                                      SDL_Rect rect,
                                      SDL_Color fill,
                                      SDL_Color border,
                                      SDL_Color accent,
                                      bool hovered,
                                      bool active,
                                      int accent_width,
                                      Uint8 inner_border_alpha);
void UIPanelVisual_DrawScrollbar(SDL_Renderer* renderer,
                                 SDL_Rect track,
                                 SDL_Rect thumb,
                                 SDL_Color track_fill,
                                 SDL_Color track_border,
                                 SDL_Color thumb_fill,
                                 SDL_Color thumb_border,
                                 bool dragging);
