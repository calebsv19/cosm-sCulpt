#pragma once
#include "UI/ui_panel_parts_surface.h"
void UIPanel_SpatialBuild(PartsPane* pane);
void UIPanel_SpatialRefresh(void);
void UIPanel_SpatialEnterVolumes(void);
char* UIPanel_SpatialInputBuffer(size_t* capacity);
bool UIPanel_SpatialClick(int action, int chooser);
void UIPanel_SpatialRunChecks(void);
