#pragma once

#include <stdbool.h>

#include "UI/ui_panel.h"

void UIPanel_UpdateViewPaneLayout(UIPanelState* ui);
bool UIPanel_GetViewPaneRects(const UIPanelState* ui,
                              SDL_Rect* out_summary_rect,
                              SDL_Rect* out_workspace_rect,
                              SDL_Rect* out_view_rect,
                              SDL_Rect* out_modes_rect);
