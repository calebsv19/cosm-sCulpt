#pragma once

#include "UI/ui_panel.h"

/* STL carries no units. Selection is explicit; legacy metre inputs remain the default. */
CoreUnitKind UIPanel_StlSourceUnit(const UIPanelState* ui);
SDL_Rect UIPanel_StlOptionRect(const UIPanelState* ui, int index);
bool UIPanel_HandleStlOptionsClick(UIPanelState* ui, int x, int y);
void UIPanel_RenderStlOptions(const UIPanelState* ui, SDL_Renderer* renderer);

void UIPanel_RenderStlCollectionPicker(const UIPanelState* ui, SDL_Renderer* renderer);
