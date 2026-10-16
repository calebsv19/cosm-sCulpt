#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
bool MeshShutdownProof_Run(AppContext *app, AppCallbacks *callbacks,
                           const char *layout, const char *capture,
                           void (*shutdown)(AppContext *));
