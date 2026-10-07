#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
bool CameraPathProof_Prepare(const char *mode);
bool CameraPathProof_Check(void);
bool CameraPathProof_Idle(AppContext *app,const AppCallbacks *callbacks);
