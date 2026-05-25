#pragma once

#include "UI/ui_panel.h"

void UIPanel_UpdateObjectPaneLayout(UIPanelState* ui);
bool UIPanel_GetObjectPaneRects(const UIPanelState* ui,
                                SDL_Rect* out_summary_rect,
                                SDL_Rect* out_details_rect,
                                SDL_Rect* out_actions_rect,
                                SDL_Rect* out_prism_rect,
                                SDL_Rect* out_gizmo_rect,
                                SDL_Rect* out_transform_rect);
