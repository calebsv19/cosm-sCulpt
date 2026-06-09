#pragma once

#include "Core/global_state.h"
#include "UI/ui_panel.h"

void Render_UIPanelObjectWorkspaceSummary(const UIPanelState* ui, SDL_Renderer* renderer);
bool UIPanel_ObjectWorkspaceHandleModelTreeClick(UIPanelState* ui,
                                                GlobalState* state,
                                                int mouse_x,
                                                int mouse_y);
bool UIPanel_ObjectWorkspaceHandleModelTreeWheel(int mouse_x,
                                                int mouse_y,
                                                float wheel_delta);
void UIPanel_ObjectWorkspaceHandleModelTreeMouseUp(void);
void UIPanel_ObjectWorkspaceHandleModelTreeMouseMotion(int mouse_x, int mouse_y);
