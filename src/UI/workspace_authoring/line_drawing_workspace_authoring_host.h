#pragma once

#include <SDL2/SDL.h>

#include "Core/global_state.h"
#include "core_base.h"

void LineDrawingWorkspaceAuthoringHost_Reset(LineDrawingWorkspaceAuthoringHostState* host);
int LineDrawingWorkspaceAuthoringHost_Active(const GlobalState* state);
int LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(const GlobalState* state);
int LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(const GlobalState* state);
CoreResult LineDrawingWorkspaceAuthoringHost_Enter(GlobalState* state);
CoreResult LineDrawingWorkspaceAuthoringHost_Apply(GlobalState* state);
CoreResult LineDrawingWorkspaceAuthoringHost_Cancel(GlobalState* state);
CoreResult LineDrawingWorkspaceAuthoringHost_CycleOverlay(GlobalState* state);
int LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(GlobalState* state, const SDL_Event* event);
