#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
#include "UI/ui_panel.h"

void Render_UIPanel(const UIPanelState* ui, SDL_Renderer* renderer);
void Render_UIPanelSide(const UIPanelState* ui, SDL_Renderer* renderer, UIPanelSide side);
void Render_UIPanelRootSummary(const UIPanelState* ui, SDL_Renderer* renderer);
void Render_UIPanelObjectSummary(const UIPanelState* ui, SDL_Renderer* renderer);
