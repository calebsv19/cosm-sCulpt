// src/Input/input_handler.h
#pragma once
#include "Core/SDLApp/sdl_app_framework.h"

void Input_Handle(AppContext* ctx, SDL_Event* event);


/* End native window capture without clearing authoring/save drafts. */
void Input_CancelWindowCapture(void);
