#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void UIPanelSummary_DrawText(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const char* text,
                             int x,
                             int y,
                             SDL_Color color);

void UIPanelSummary_DrawTextClipped(SDL_Renderer* renderer,
                                    TTF_Font* font,
                                    const char* text,
                                    int x,
                                    int y,
                                    int max_width,
                                    int clip_height,
                                    SDL_Color color);

void UIPanelSummary_DrawCard(SDL_Renderer* renderer,
                             SDL_Rect rect,
                             SDL_Color fill_color,
                             SDL_Color border_color,
                             SDL_Color accent_color,
                             int accent_height);

void UIPanelSummary_DrawDivider(SDL_Renderer* renderer,
                                SDL_Rect rect,
                                int y,
                                int inset_x,
                                SDL_Color color,
                                Uint8 alpha);
