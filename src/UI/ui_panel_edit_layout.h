#pragma once

#include <stdbool.h>

#include "UI/ui_panel.h"

void UIPanel_UpdateEditPaneLayout(UIPanelState* ui);
bool UIPanel_GetEditPaneRects(const UIPanelState* ui,
                              SDL_Rect* out_summary_rect,
                              SDL_Rect* out_workspace_rect,
                              SDL_Rect* out_selection_mode_rect);
