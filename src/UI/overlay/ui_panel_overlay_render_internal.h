#pragma once

#include "UI/ui_panel.h"

void UIPanelOverlay_DrawText(SDL_Renderer* renderer,
                             const char* text,
                             int x,
                             int y,
                             SDL_Color color);
void UIPanelOverlay_DrawTextClipped(SDL_Renderer* renderer,
                                    const char* text,
                                    int x,
                                    int y,
                                    int max_width,
                                    SDL_Color color);

void UIPanelOverlay_RenderFileDialogs(SDL_Renderer* renderer, const UIPanelState* ui);
void UIPanelOverlay_RenderEditDialogs(SDL_Renderer* renderer, const UIPanelState* ui);
