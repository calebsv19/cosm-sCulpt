#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
enum {
    UI_PATH_SHOW=999, UI_PATH_VIEW=1000, UI_PATH_REVERT, UI_PATH_APPLY, UI_PATH_ADD, UI_PATH_NEW,
    UI_PATH_PREVIOUS, UI_PATH_NEXT, UI_PATH_PLAY, UI_PATH_LOOP, UI_PATH_SCRUB,
    UI_PATH_ACTIONS, UI_PATH_SECONDS, UI_PATH_EARLIER, UI_PATH_LATER, UI_PATH_REMOVE,
    UI_PATH_CLOSED, UI_PATH_POINT_BASE=1100
};
int UIPanel_CameraPathHeight(void);
void UIPanel_CameraPathRender(SDL_Renderer *renderer,int y);
bool UIPanel_CameraPathRect(int action,SDL_Rect *rect,int y);
bool UIPanel_CameraPathClick(int x,int y,int base_y);
bool UIPanel_CameraPathEvent(const SDL_Event *event,int y);
bool UIPanel_CameraPathCapturing(void);
void UIPanel_CameraPathReset(void);
