#pragma once

#include "Core/global_state.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

bool LineDrawingObjectWorkspaceViewportHud_HandleClick(GlobalState* state,
                                                       int mouse_x,
                                                       int mouse_y);
void LineDrawingObjectWorkspaceViewportHud_Render(SDL_Renderer* renderer,
                                                  const GlobalState* state);
