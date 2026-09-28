#pragma once
#include "UI/ui_panel.h"
bool UIPanel_BeginMeasurement(void);
bool UIPanel_MeasurementKey(SDL_Keycode key);
bool UIPanel_MeasurementClick(int x, int y);
void UIPanel_RenderMeasurement(SDL_Renderer* renderer);

bool UIPanel_MeasurementPickAt(int x, int y);
void UIPanel_RenderMeasurementViewport(SDL_Renderer* renderer);

bool UIPanel_MeasurementText(const char* text);
