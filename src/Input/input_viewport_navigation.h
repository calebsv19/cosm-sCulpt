#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

bool InputViewportNavigation_HandleMouseButton(const SDL_MouseButtonEvent* button);
bool InputViewportNavigation_HandleMouseMotion(const SDL_MouseMotionEvent* motion);
void InputViewportNavigation_ResetGesture(void);
