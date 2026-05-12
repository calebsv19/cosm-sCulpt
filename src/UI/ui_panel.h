#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "Math/math_util.h"
#include "core_units.h"

#define MAX_UI_BUTTONS 40

typedef enum {
    UI_PANEL_LEFT,
    UI_PANEL_RIGHT
} UIPanelSide;

typedef enum {
    UI_PANEL_GROUP_NONE = 0,
    UI_PANEL_GROUP_LEFT_FILE_IO,
    UI_PANEL_GROUP_LEFT_ROOT_PATHS,
    UI_PANEL_GROUP_RIGHT_VIEW,
    UI_PANEL_GROUP_RIGHT_MODES,
    UI_PANEL_GROUP_RIGHT_PRIMITIVES,
    UI_PANEL_GROUP_RIGHT_CONSTRUCTION,
    UI_PANEL_GROUP_RIGHT_PRISM,
    UI_PANEL_GROUP_RIGHT_GIZMO,
    UI_PANEL_GROUP_RIGHT_TRANSFORM,
    UI_PANEL_GROUP_RIGHT_BOUNDS
} UIPanelGroup;

typedef struct {
    char label[64];
    SDL_Rect bounds;
    UIPanelSide side;
    UIPanelGroup group;
    int id;            // Unique enum or index
    bool hovered;
    bool pressed;
} UIButton;

#define UI_BTN_SAVE_JSON 0
#define UI_BTN_LOAD_JSON 1
#define UI_BTN_LOAD_SCENE 2
#define UI_BTN_EXPORT_SHAPE 3
#define UI_BTN_EXPORT_SCENE 4
#define UI_BTN_INPUT_ROOT_EDIT 5
#define UI_BTN_INPUT_ROOT_FOLDER 6
#define UI_BTN_OUTPUT_ROOT_EDIT 7
#define UI_BTN_OUTPUT_ROOT_FOLDER 8

#define UI_BTN_RESET_ORIGIN 10
#define UI_BTN_ZOOM_IN 11
#define UI_BTN_ZOOM_OUT 12
#define UI_BTN_TOGGLE_DELETE 13
#define UI_BTN_PIN_ANCHOR 14
#define UI_BTN_LINK_HANDLES 15
#define UI_BTN_TOGGLE_SPACE_MODE 16
#define UI_BTN_CREATE_PLANE 17
#define UI_BTN_CREATE_RECT_PRISM 18
#define UI_BTN_EDIT_PRISM_WIDTH 19
#define UI_BTN_EDIT_PRISM_HEIGHT 20
#define UI_BTN_EDIT_PRISM_DEPTH 21
#define UI_BTN_CYCLE_DISPLAY_UNITS 22
#define UI_BTN_TOGGLE_OBJECT_GIZMO_MODE 23
#define UI_BTN_TOGGLE_SCENE_BOUNDS 24
#define UI_BTN_TOGGLE_SCENE_BOUNDS_CLAMP 25
#define UI_BTN_EDIT_SCENE_BOUNDS_MIN 26
#define UI_BTN_EDIT_SCENE_BOUNDS_MAX 27
#define UI_BTN_SET_CONSTRUCTION_PLANE_XY 28
#define UI_BTN_SET_CONSTRUCTION_PLANE_YZ 29
#define UI_BTN_SET_CONSTRUCTION_PLANE_XZ 30
#define UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_NEG 31
#define UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_POS 32
#define UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET 33
#define UI_BTN_EDIT_OBJECT_POSITION 34
#define UI_BTN_EDIT_OBJECT_ROTATION_X 35
#define UI_BTN_EDIT_OBJECT_ROTATION_Y 36
#define UI_BTN_EDIT_OBJECT_ROTATION_Z 37
#define UI_BTN_FIT_SCENE_BOUNDS_TO_OBJECT 38

#define MAX_CONFIG_FILES 128
#define MAX_CONFIG_PATH 512

typedef enum {
    UI_LOAD_MENU_MODE_NONE = 0,
    UI_LOAD_MENU_MODE_JSON = 1,
    UI_LOAD_MENU_MODE_SCENE = 2
} UILoadMenuMode;

typedef enum {
    UI_ROOT_TARGET_NONE = 0,
    UI_ROOT_TARGET_INPUT = 1,
    UI_ROOT_TARGET_OUTPUT = 2
} UIRootDialogTarget;

typedef enum {
    UI_PRISM_DIMENSION_TARGET_NONE = 0,
    UI_PRISM_DIMENSION_TARGET_WIDTH = 1,
    UI_PRISM_DIMENSION_TARGET_HEIGHT = 2,
    UI_PRISM_DIMENSION_TARGET_DEPTH = 3
} UIPrismDimensionDialogTarget;

typedef enum {
    UI_SCENE_BOUNDS_TARGET_NONE = 0,
    UI_SCENE_BOUNDS_TARGET_MIN = 1,
    UI_SCENE_BOUNDS_TARGET_MAX = 2
} UISceneBoundsDialogTarget;

typedef enum {
    UI_CONSTRUCTION_PLANE_DIALOG_TARGET_NONE = 0,
    UI_CONSTRUCTION_PLANE_DIALOG_TARGET_OFFSET = 1
} UIConstructionPlaneDialogTarget;

typedef enum {
    UI_OBJECT_TRANSFORM_DIALOG_TARGET_NONE = 0,
    UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION = 1,
    UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_X = 2,
    UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Y = 3,
    UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Z = 4
} UIObjectTransformDialogTarget;

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
        UILoadMenuMode mode;
        int anchorButtonId;
        char rootPath[MAX_CONFIG_PATH];
        char entries[MAX_CONFIG_FILES][128];
        char entryPaths[MAX_CONFIG_FILES][MAX_CONFIG_PATH];
        int count;
        int hoverIndex;
        int activeIndex;
        float scrollOffsetPx;
        bool scrollbarDragging;
        int scrollbarDragStartY;
        float scrollbarDragStartOffsetPx;
    } loadMenu;

    struct {
        bool active;
        UIRootDialogTarget target;
        char buffer[256];
        size_t length;
        size_t cursor;
    } rootDialog;

    struct {
        bool active;
        UIPrismDimensionDialogTarget target;
        uint32_t objectId;
        char buffer[64];
        size_t length;
        size_t cursor;
    } prismDimensionDialog;

    struct {
        bool active;
        UISceneBoundsDialogTarget target;
        char buffer[128];
        size_t length;
        size_t cursor;
    } sceneBoundsDialog;

    struct {
        bool active;
        UIConstructionPlaneDialogTarget target;
        char buffer[64];
        size_t length;
        size_t cursor;
    } constructionPlaneDialog;

    struct {
        bool active;
        UIObjectTransformDialogTarget target;
        uint32_t objectId;
        char buffer[128];
        size_t length;
        size_t cursor;
    } objectTransformDialog;

    CoreUnitKind displayUnit;
} UIPanelState;

typedef struct {
    SDL_Rect menuBounds;
    int itemHeight;
} UIPanelLayoutCache;

typedef struct {
    int button_text_pad_px;
    int overlay_height_px;
    int top_offset_px;
    int pane_padding_px;
    int button_spacing_px;
    int button_height_px;
    int group_header_height_px;
    int group_gap_px;
    int compact_row_gap_px;
    int left_button_width_px;
    int right_button_width_px;
    int desired_top_pane_height_px;
    int desired_left_pane_width_px;
    int desired_right_pane_width_px;
} UIPanelLayoutMetrics;

void UIPanel_GetLayoutMetrics(UIPanelLayoutMetrics* out_metrics);
void UIPanel_Init(int screenW, int screenH);
void UIPanel_OnWindowResized(int screenW, int screenH);
const UIButton* UIPanel_GetButtons(UIPanelState* ui, int* outCount);

UIPanelState* UIPanel_Get(void);
void UIPanel_RefreshConfigList(void);
void UIPanel_BeginSaveDialog(void);
bool UIPanel_OpenJsonFolderDialog(void);
bool UIPanel_OpenSceneFolderDialog(void);
void UIPanel_ExportShape(void);
void UIPanel_ExportScene(void);
bool UIPanel_IsSaveDialogActive(void);
bool UIPanel_IsRootDialogActive(void);
bool UIPanel_IsPrismDimensionDialogActive(void);
bool UIPanel_IsSceneBoundsDialogActive(void);
bool UIPanel_IsConstructionPlaneDialogActive(void);
bool UIPanel_IsObjectTransformDialogActive(void);
bool UIPanel_HandleTextInput(const char* text);
bool UIPanel_HandleKeyEvent(const SDL_Event* event);
bool UIPanel_IsCapturingKeyboard(void);
void UIPanel_RenderOverlays(SDL_Renderer* renderer);
bool UIPanel_HandleLoadMenuClick(int mouseX, int mouseY);
bool UIPanel_HandleLoadMenuWheel(int mouseX, int mouseY, float wheel_delta);
void UIPanel_ToggleLoadMenu(void);
bool UIPanel_IsLoadMenuOpen(void);
void UIPanel_HandleMouseMotion(int mouseX, int mouseY);
void UIPanel_BeginInputRootDialog(void);
void UIPanel_BeginOutputRootDialog(void);
bool UIPanel_BeginPrismWidthDialog(void);
bool UIPanel_BeginPrismHeightDialog(void);
bool UIPanel_BeginPrismDepthDialog(void);
void UIPanel_CycleDisplayUnit(void);
bool UIPanel_SetDisplayUnit(CoreUnitKind unit);
CoreUnitKind UIPanel_GetDisplayUnit(void);
const char* UIPanel_GetDisplayUnitSymbol(void);
bool UIPanel_ToggleObjectGizmoRotateMode(void);
bool UIPanel_IsObjectGizmoRotateMode(void);
bool UIPanel_ToggleSceneBoundsEnabled(void);
bool UIPanel_ToggleSceneBoundsClampOnEdit(void);
bool UIPanel_BeginSceneBoundsMinDialog(void);
bool UIPanel_BeginSceneBoundsMaxDialog(void);
bool UIPanel_SetConstructionPlaneAxis(ViewPlaneAxis axis);
bool UIPanel_AdjustConstructionPlaneOffset(float delta_world);
bool UIPanel_BeginConstructionPlaneOffsetDialog(void);
bool UIPanel_BeginObjectPositionDialog(void);
bool UIPanel_BeginObjectRotationXDialog(void);
bool UIPanel_BeginObjectRotationYDialog(void);
bool UIPanel_BeginObjectRotationZDialog(void);
bool UIPanel_ConvertWorldToDisplay(double world_value, double* out_display_value);
bool UIPanel_ConvertDisplayToWorld(double display_value, double* out_world_value);
bool UIPanel_OpenInputRootFolderDialog(void);
bool UIPanel_OpenOutputRootFolderDialog(void);
bool UIPanel_CreatePlanePrimitiveFromActiveContext(bool disable_bounds_lock);
bool UIPanel_CreateRectPrismPrimitiveFromActiveContext(bool disable_bounds_lock);
bool UIPanel_FitSceneBoundsToSelectedObject(void);
