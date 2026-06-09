#pragma once

#include <stdbool.h>

#include "UI/ui_panel.h"

void UIPanel_InitShellState(UIPanelState* ui);
void UIPanel_UpdateTabLayout(UIPanelState* ui,
                             const SDL_Rect* leftPaneRect,
                             const SDL_Rect* rightPaneRect,
                             const UIPanelLayoutMetrics* metrics);
bool UIPanel_HandleTabClick(UIPanelState* ui, int mouseX, int mouseY);
bool UIPanel_ShouldShowGroup(const UIPanelState* ui, UIPanelGroup group);
bool UIPanel_ShouldRenderRootSummary(const UIPanelState* ui);
bool UIPanel_ShouldRenderObjectSummary(const UIPanelState* ui);
const char* UIPanel_LeftTabLabel(UIPanelLeftTab tab);
const char* UIPanel_RightTabLabel(UIPanelRightTab tab);
UIPanelLeftTab UIPanel_GetActiveLeftTab(const UIPanelState* ui);
UIPanelRightTab UIPanel_GetActiveRightTab(const UIPanelState* ui);
void UIPanel_SetActiveLeftTab(UIPanelState* ui, UIPanelLeftTab tab);
void UIPanel_SetActiveRightTab(UIPanelState* ui, UIPanelRightTab tab);
void UIPanel_SyncWorkspaceTabState(UIPanelState* ui);
void UIPanel_FocusObjectAuthoringTab(UIPanelState* ui);
void UIPanel_FocusObjectInspectorTab(UIPanelState* ui);
void UIPanel_FocusObjectEditTab(UIPanelState* ui);
