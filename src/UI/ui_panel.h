#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

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

#define MAX_CONFIG_FILES 32

typedef struct {
    UIButton buttons[MAX_UI_BUTTONS];
    int count;

    struct {
        bool active;
        char buffer[128];
        size_t length;
        size_t cursor;
    } saveDialog;

    struct {
        bool open;
        char entries[MAX_CONFIG_FILES][128];
        int count;
        int hoverIndex;
    } loadMenu;
} UIPanelState;

typedef struct {
    SDL_Rect menuBounds;
    int itemHeight;
} UIPanelLayoutCache;

void UIPanel_Init(int screenW, int screenH);
const UIButton* UIPanel_GetButtons(UIPanelState* ui, int* outCount);

UIPanelState* UIPanel_Get(void);
void UIPanel_RefreshConfigList(void);
void UIPanel_BeginSaveDialog(void);
bool UIPanel_IsSaveDialogActive(void);
bool UIPanel_HandleTextInput(const char* text);
bool UIPanel_HandleKeyEvent(const SDL_Event* event);
bool UIPanel_IsCapturingKeyboard(void);
void UIPanel_RenderOverlays(SDL_Renderer* renderer);
bool UIPanel_HandleLoadMenuClick(int mouseX, int mouseY);
void UIPanel_ToggleLoadMenu(void);
bool UIPanel_IsLoadMenuOpen(void);
void UIPanel_HandleMouseMotion(int mouseX, int mouseY);
