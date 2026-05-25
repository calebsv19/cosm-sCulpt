#pragma once

#include <stdbool.h>

#include "UI/ui_panel.h"

void UIPanel_UpdateScenePaneLayout(UIPanelState* ui);
bool UIPanel_GetScenePaneRects(const UIPanelState* ui,
                               SDL_Rect* out_summary_rect,
                               SDL_Rect* out_list_rect,
                               SDL_Rect* out_selection_rect,
                               SDL_Rect* out_bounds_rect);
