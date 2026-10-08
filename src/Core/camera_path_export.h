#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
/* Runs only for the explicit offline export environment route. */
bool CameraPathExport_Run(AppContext *app,const AppCallbacks *callbacks,const char *manifest_path);
