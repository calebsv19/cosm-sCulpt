#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
/* Shared observation/mapping, app-owned Vulkan resources and cancellation. */
bool SDLAppWindow_Refresh(AppContext* ctx);
bool SDLAppWindow_Filter(AppContext* ctx, SDL_Event* event);
int SDLAppWindow_Probe(AppContext* ctx);
void SDLAppWindow_Shutdown(AppContext* ctx);
