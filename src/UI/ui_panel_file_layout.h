#pragma once

#include <stdbool.h>

#include "UI/ui_panel.h"

void UIPanel_UpdateFilePaneLayout(UIPanelState* ui);
bool UIPanel_GetFilePaneRects(const UIPanelState* ui,
                              SDL_Rect* out_summary_rect,
                              SDL_Rect* out_file_actions_rect,
                              SDL_Rect* out_root_paths_rect,
                              SDL_Rect* out_browser_rect);
