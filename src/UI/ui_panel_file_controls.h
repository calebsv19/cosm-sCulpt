#pragma once

#include "UI/ui_panel.h"

int UIPanel_FileControlsButtonHeightPx(const UIPanelLayoutMetrics* metrics);
int UIPanel_FileControlsSectionHeight(const UIPanelLayoutMetrics* metrics, UIPanelGroup group);
void UIPanel_LayoutFilePaneButtons(UIPanelState* ui,
                                   const UIPanelLayoutMetrics* metrics,
                                   int text_pad_x);
