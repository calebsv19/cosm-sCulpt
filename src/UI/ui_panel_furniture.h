#pragma once
#include "UI/ui_panel.h"
enum {
    UI_FURNITURE_OPEN = 1, UI_FURNITURE_LENGTH, UI_FURNITURE_DEPTH, UI_FURNITURE_HEIGHT,
    UI_FURNITURE_KEEP, UI_FURNITURE_PANELS, UI_FURNITURE_CASE, UI_FURNITURE_BACK,
    UI_FURNITURE_DOOR, UI_FURNITURE_WORKTOP, UI_FURNITURE_APPLY, UI_FURNITURE_CANCEL
};
int UIPanel_FurnitureHeight(void);
bool UIPanel_FurnitureEditing(void);
void UIPanel_FurnitureReset(void);
void UIPanel_RenderFurniture(SDL_Renderer* renderer);
bool UIPanel_FurnitureClick(int x, int y);
bool UIPanel_FurnitureEvent(const SDL_Event* event);
bool UIPanel_FurnitureControlRect(int action, SDL_Rect* rect);
