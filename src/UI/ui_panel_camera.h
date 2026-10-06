#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
enum {
    UI_CAMERA_PREVIOUS = 1,
    UI_CAMERA_NEXT,
    UI_CAMERA_EXPLORE,
    UI_CAMERA_ENTER,
    UI_CAMERA_EXIT,
    UI_CAMERA_SAVE,
    UI_CAMERA_UPDATE,
    UI_CAMERA_DETAILS,
    UI_CAMERA_LOOK,
    UI_CAMERA_ROLL,
    UI_CAMERA_LEVEL,
    UI_CAMERA_FORWARD,
    UI_CAMERA_BACK,
    UI_CAMERA_LEFT,
    UI_CAMERA_RIGHT,
    UI_CAMERA_DOWN,
    UI_CAMERA_UP,
    UI_CAMERA_X,
    UI_CAMERA_Y,
    UI_CAMERA_Z,
    UI_CAMERA_YAW,
    UI_CAMERA_PITCH,
    UI_CAMERA_ROLL_VALUE,
    UI_CAMERA_FOV,
    UI_CAMERA_SPEED,
    UI_CAMERA_NEAR,
    UI_CAMERA_FAR,
    UI_CAMERA_INSPECTION,
    UI_CAMERA_FACING_DETAILS,
    UI_CAMERA_FACING_SCOPE,
    UI_CAMERA_FACING_INHERIT,
    UI_CAMERA_FACING_SOLID,
    UI_CAMERA_FACING_POS_X,
    UI_CAMERA_FACING_NEG_X,
    UI_CAMERA_FACING_POS_Y,
    UI_CAMERA_FACING_NEG_Y,
    UI_CAMERA_FACING_POS_Z,
    UI_CAMERA_FACING_NEG_Z
};
int UIPanel_CameraHeight(void);
bool UIPanel_CameraCapturingKeyboard(void);
void UIPanel_CameraReset(void);
void UIPanel_RenderCamera(SDL_Renderer *renderer);
bool UIPanel_CameraEvent(const SDL_Event *event);
bool UIPanel_CameraClick(int x, int y);
bool UIPanel_CameraControlRect(int action, SDL_Rect *out);
