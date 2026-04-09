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

#define UI_BTN_SAVE_JSON 0
#define UI_BTN_LOAD_JSON 1
#define UI_BTN_EXPORT_SHAPE 2
#define UI_BTN_INPUT_ROOT_EDIT 3
#define UI_BTN_INPUT_ROOT_FOLDER 4
#define UI_BTN_OUTPUT_ROOT_EDIT 5
#define UI_BTN_OUTPUT_ROOT_FOLDER 6

#define UI_BTN_RESET_ORIGIN 10
#define UI_BTN_ZOOM_IN 11
#define UI_BTN_ZOOM_OUT 12
#define UI_BTN_TOGGLE_DELETE 13
#define UI_BTN_PIN_ANCHOR 14
#define UI_BTN_LINK_HANDLES 15
#define UI_BTN_TOGGLE_SPACE_MODE 16

#define MAX_CONFIG_FILES 32
#define MAX_CONFIG_PATH 256

typedef enum {
    UI_ROOT_TARGET_NONE = 0,
    UI_ROOT_TARGET_INPUT = 1,
    UI_ROOT_TARGET_OUTPUT = 2
} UIRootDialogTarget;

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
        char entryPaths[MAX_CONFIG_FILES][MAX_CONFIG_PATH];
        int count;
        int hoverIndex;
    } loadMenu;

    struct {
        bool active;
        UIRootDialogTarget target;
        char buffer[256];
        size_t length;
        size_t cursor;
    } rootDialog;
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
void UIPanel_ExportShape(void);
bool UIPanel_IsSaveDialogActive(void);
bool UIPanel_IsRootDialogActive(void);
bool UIPanel_HandleTextInput(const char* text);
bool UIPanel_HandleKeyEvent(const SDL_Event* event);
bool UIPanel_IsCapturingKeyboard(void);
void UIPanel_RenderOverlays(SDL_Renderer* renderer);
bool UIPanel_HandleLoadMenuClick(int mouseX, int mouseY);
void UIPanel_ToggleLoadMenu(void);
bool UIPanel_IsLoadMenuOpen(void);
void UIPanel_HandleMouseMotion(int mouseX, int mouseY);
void UIPanel_BeginInputRootDialog(void);
void UIPanel_BeginOutputRootDialog(void);
bool UIPanel_OpenInputRootFolderDialog(void);
bool UIPanel_OpenOutputRootFolderDialog(void);
