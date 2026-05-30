#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

bool LineDrawingEditorTopbar_HandleClick(int mouse_x, int mouse_y);
void LineDrawingEditorTopbar_Render(SDL_Renderer* renderer);
