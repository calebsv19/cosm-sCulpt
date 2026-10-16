#pragma once
#include "Core/global_state.h"
#include "UI/shared_controls.h"
bool LDOverlay_Blocked(void);
void LDOverlay_Cancel(void);
bool LDOverlay_AuthoringEvent(GlobalState *, const SDL_Event *);
void LDOverlay_AuthoringDraw(SDL_Renderer *, const GlobalState *);
bool LineDrawingWorkspaceAuthoringHost_Click(GlobalState *, int, int);
bool LDOverlay_NoticeRect(const GlobalState *, SDL_Rect *);
void LDOverlay_NoticeDraw(SDL_Renderer *, const GlobalState *);
void LDOverlay_CollectHud(void);

#include "kit_workspace_authoring_ui.h"
bool LDOverlay_FontThemeLayout(const GlobalState *, KitWorkspaceAuthoringFontThemeLayout *);
