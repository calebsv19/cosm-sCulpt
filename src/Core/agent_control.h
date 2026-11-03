#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
/* Local user-only transport; document operations execute on the SDL thread. */
void SculptAgent_Start(void);
bool SculptAgent_Poll(AppContext* app);
void SculptAgent_Stop(void);
char* SculptAgent_Command(AppContext* app,const char* input);
void SculptAgent_SetEnterEditor(void (*enter)(void));
