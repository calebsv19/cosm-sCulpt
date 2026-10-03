#pragma once
#include "UI/ui_panel_spatial.h"
void UIPanel_MotionInspectionBuild(PartsPane* pane, const LayoutSpatialResult* result);
bool UIPanel_MotionInspectionClick(int action, const LayoutSpatialResult* result);
void UIPanel_RenderMotionPreview(SDL_Renderer* renderer);
