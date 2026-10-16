#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>
enum {UI_VIS_SCENE=40,UI_VIS_LAYERS,UI_VIS_VIEWS,UI_VIS_EDIT=44,UI_VIS_REVEAL=46,UI_VIS_INSPECT=47};
void UIPanel_VisibilityOpen(void);
void UIPanel_VisibilityClose(void);
bool UIPanel_VisibilityEvent(const SDL_Event* event);
bool UIPanel_VisibilityClick(int x,int y);
void UIPanel_VisibilityRender(SDL_Renderer* renderer);
bool UIPanel_VisibilityRect(int action,SDL_Rect* out);
size_t UIPanel_VisibilityCount(void);

void UIPanel_VisibilityReset(void);
