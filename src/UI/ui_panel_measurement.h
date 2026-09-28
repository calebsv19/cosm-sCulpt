#pragma once
#include "UI/ui_panel.h"
bool UIPanel_BeginMeasurement(void);
bool UIPanel_MeasurementKey(SDL_Keycode key);
bool UIPanel_MeasurementClick(int x, int y);
void UIPanel_RenderMeasurement(SDL_Renderer* renderer);
