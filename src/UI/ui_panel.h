#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>

#define MAX_UI_BUTTONS 32

typedef enum {
    UI_PANEL_LEFT,
    UI_PANEL_RIGHT
} UIPanelSide;

typedef struct {
    char label[64];
    SDL_Rect bounds;
    UIPanelSide side;
    int id;            // Unique enum or index
    bool hovered;
    bool pressed;
} UIButton;

typedef struct {
    UIButton buttons[MAX_UI_BUTTONS];
    int count;
} UIPanelState;

void UIPanel_Init(int screenW, int screenH);
const UIButton* UIPanel_GetButtons(UIPanelState* ui, int* outCount);

UIPanelState* UIPanel_Get(void);
