#pragma once

#include <stdbool.h>

#include "UI/ui_panel.h"

void UIPanel_UpdateCreatePaneLayout(UIPanelState* ui);
bool UIPanel_GetCreatePaneRects(const UIPanelState* ui,
                                SDL_Rect* out_summary_rect,
                                SDL_Rect* out_workspace_rect,
                                SDL_Rect* out_primitives_rect,
                                SDL_Rect* out_construction_rect);
