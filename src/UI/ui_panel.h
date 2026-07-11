#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "Math/math_util.h"
#include "core_units.h"

#define MAX_UI_BUTTONS 80

typedef enum {
    UI_PANEL_LEFT,
    UI_PANEL_RIGHT
} UIPanelSide;

typedef enum {
    UI_PANEL_LEFT_TAB_SCENE = 0,
    UI_PANEL_LEFT_TAB_FILE = 1,
    UI_PANEL_LEFT_TAB_COUNT
} UIPanelLeftTab;

typedef enum {
    UI_PANEL_RIGHT_TAB_VIEW = 0,
    UI_PANEL_RIGHT_TAB_CREATE = 1,
    UI_PANEL_RIGHT_TAB_OBJECT = 2,
    UI_PANEL_RIGHT_TAB_EDIT = 3,
    UI_PANEL_RIGHT_TAB_COUNT
} UIPanelRightTab;

typedef enum {
    UI_PANEL_GROUP_NONE = 0,
    UI_PANEL_GROUP_LEFT_SCENE_SELECTION,
    UI_PANEL_GROUP_LEFT_SCENE_BOUNDS,
    UI_PANEL_GROUP_LEFT_FILE_IO,
    UI_PANEL_GROUP_LEFT_ROOT_PATHS,
    UI_PANEL_GROUP_RIGHT_VIEW,
    UI_PANEL_GROUP_RIGHT_MODES,
    UI_PANEL_GROUP_RIGHT_PRIMITIVES,
    UI_PANEL_GROUP_RIGHT_OPERATIONS,
    UI_PANEL_GROUP_RIGHT_CONSTRUCTION,
    UI_PANEL_GROUP_RIGHT_PRISM,
    UI_PANEL_GROUP_RIGHT_GIZMO,
    UI_PANEL_GROUP_RIGHT_TRANSFORM,
    UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS,
    UI_PANEL_GROUP_RIGHT_EDIT_SELECT
} UIPanelGroup;

typedef struct {
    char label[64];
    SDL_Rect bounds;
    UIPanelSide side;
    UIPanelGroup group;
    int id;            // Unique enum or index
    bool hovered;
    bool pressed;
    Uint32 pressedTicks;
} UIButton;

typedef struct {
    SDL_Rect bounds;
    char label[24];
    bool active;
} UIPanelTabButton;

#define UI_BTN_SAVE_JSON 0
#define UI_BTN_LOAD_JSON 1
#define UI_BTN_LOAD_SCENE 2
#define UI_BTN_EXPORT_SHAPE 3
#define UI_BTN_EXPORT_SCENE 4
#define UI_BTN_INPUT_ROOT_EDIT 5
#define UI_BTN_INPUT_ROOT_FOLDER 6
#define UI_BTN_OUTPUT_ROOT_EDIT 7
#define UI_BTN_OUTPUT_ROOT_FOLDER 8
#define UI_BTN_FILE_BROWSER_USE_ACTIVE 9

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
#define UI_BTN_SCENE_CLEAR_SELECTION 39
#define UI_BTN_SCENE_DELETE_SELECTED 40
#define UI_BTN_OBJECT_CLEAR_SELECTION 41
#define UI_BTN_OBJECT_DELETE_SELECTED 42
#define UI_BTN_FILE_BROWSER_CLEAR_REMEMBERED 43
#define UI_BTN_EXTRUDE_ADD 44
#define UI_BTN_EXTRUDE_CUT 45
#define UI_BTN_OBJECT_FACE_SELECT 46
#define UI_BTN_OBJECT_SKETCH_SELECT 47
#define UI_BTN_OBJECT_SKETCH_CLEAR 48
#define UI_BTN_EXTRUDE_DEPTH_DEC 49
#define UI_BTN_EXTRUDE_DEPTH_INC 50
#define UI_BTN_PLACE_MESH_INSTANCE 51
#define UI_BTN_LOAD_MESH_ASSET 52
#define UI_BTN_OBJECT_EDIT_BODY_MODE 53
#define UI_BTN_OBJECT_EDIT_FACE_MODE 54
#define UI_BTN_OBJECT_EDIT_EDGE_MODE 55
#define UI_BTN_OBJECT_EDIT_VERTEX_MODE 56
#define UI_BTN_LOAD_STL 57
#define UI_BTN_CREATE_LIGHT 58
#define UI_BTN_CREATE_CAMERA_PATH 59
#define UI_BTN_CREATE_MATERIAL 60
#define UI_BTN_SCENE_AUTHORING_EDIT_MODE 61
#define UI_BTN_SCENE_AUTHORING_LIGHT_ENABLED 62
#define UI_BTN_SCENE_AUTHORING_LIGHT_KIND 63
#define UI_BTN_SCENE_AUTHORING_LIGHT_PATH 64
#define UI_BTN_SCENE_AUTHORING_PATH_KIND 65
#define UI_BTN_SCENE_AUTHORING_MATERIAL_COLOR 66

#define MAX_CONFIG_FILES 128
#define MAX_CONFIG_PATH 512

typedef enum {
    UI_LOAD_MENU_MODE_NONE = 0,
    UI_LOAD_MENU_MODE_JSON = 1,
    UI_LOAD_MENU_MODE_SCENE = 2,
    UI_LOAD_MENU_MODE_OBJECT = 3,
    UI_LOAD_MENU_MODE_RUNTIME_MESH = 4,
    UI_LOAD_MENU_MODE_STL_IMPORT = 5
} UILoadMenuMode;

typedef enum {
    UI_LOAD_MENU_SELECTION_NONE = 0,
    UI_LOAD_MENU_SELECTION_ACTIVE_SESSION = 1,
    UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY = 2
} UILoadMenuSelectionState;

typedef struct UIPanelFileBrowserRestoreSummary {
    UILoadMenuMode mode;
    bool hasMode;
    bool visible;
    char rootPath[MAX_CONFIG_PATH];
    bool hasActiveSessionPath;
    bool activeSessionPathExists;
    int activeIndex;
    char activeSessionPath[MAX_CONFIG_PATH];
    bool hasRememberedEntryPath;
    bool rememberedEntryExists;
    int rememberedIndex;
    char rememberedEntryPath[MAX_CONFIG_PATH];
} UIPanelFileBrowserRestoreSummary;

typedef enum {
    UI_LOAD_PROGRESS_NONE = 0,
    UI_LOAD_PROGRESS_LOADING = 1,
    UI_LOAD_PROGRESS_COMPLETE = 2,
    UI_LOAD_PROGRESS_FAILED = 3
} UILoadProgressState;

typedef enum {
    UI_ROOT_TARGET_NONE = 0,
    UI_ROOT_TARGET_INPUT = 1,
    UI_ROOT_TARGET_OUTPUT = 2,
    UI_ROOT_TARGET_OBJECT_ASSET = 3
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
    UIPanelLeftTab sceneActiveLeftTab;
    UIPanelRightTab sceneActiveRightTab;
    UIPanelLeftTab objectActiveLeftTab;
    UIPanelRightTab objectActiveRightTab;
    UIPanelLeftTab activeLeftTab;
    UIPanelRightTab activeRightTab;
    UIPanelTabButton leftTabs[UI_PANEL_LEFT_TAB_COUNT];
    UIPanelTabButton rightTabs[UI_PANEL_RIGHT_TAB_COUNT];
    SDL_Rect leftPaneRect;
    SDL_Rect rightPaneRect;
    SDL_Rect leftBodyRect;
    SDL_Rect rightBodyRect;
    struct {
        SDL_Rect summaryRect;
        SDL_Rect fileActionsRect;
        SDL_Rect rootPathsRect;
        SDL_Rect browserRect;
        char actionStatus[160];
        Uint32 actionStatusSetTicks;
    } filePane;
    struct {
        SDL_Rect summaryRect;
        SDL_Rect listRect;
        SDL_Rect selectionRect;
        SDL_Rect boundsRect;
    } scenePane;
    struct {
        SDL_Rect summaryRect;
        SDL_Rect browserRect;
    } objectWorkspacePane;
    struct {
        float operationScrollOffsetPx;
        int hoverOperationIndex;
        bool operationScrollbarDragging;
        int operationScrollbarDragStartY;
        float operationScrollbarDragStartOffsetPx;
    } objectModelTree;
    struct {
        SDL_Rect summaryRect;
        SDL_Rect detailsRect;
        SDL_Rect actionsRect;
        SDL_Rect prismRect;
        SDL_Rect gizmoRect;
        SDL_Rect transformRect;
    } objectPane;
    struct {
        SDL_Rect summaryRect;
        SDL_Rect workspaceRect;
        SDL_Rect primitivesRect;
        SDL_Rect operationsRect;
        SDL_Rect constructionRect;
    } createPane;
    struct {
        SDL_Rect summaryRect;
        SDL_Rect workspaceRect;
        SDL_Rect viewRect;
        SDL_Rect modesRect;
    } viewPane;
    struct {
        SDL_Rect summaryRect;
        SDL_Rect workspaceRect;
        SDL_Rect selectionModeRect;
    } editPane;
    struct {
        float scrollOffsetPx;
        int hoverIndex;
        uint32_t expandedObjectId;
        uint32_t lastClickedObjectId;
        Uint32 lastClickTicks;
        bool scrollbarDragging;
        int scrollbarDragStartY;
        float scrollbarDragStartOffsetPx;
    } sceneList;

    struct {
        bool active;
        char buffer[128];
        size_t length;
        size_t cursor;
    } saveDialog;

    struct {
        bool open;
        bool visible;
        UILoadMenuMode mode;
        int anchorButtonId;
        Uint32 lastModeButtonClickTicks;
        int lastModeButtonId;
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
        UILoadProgressState loadProgressState;
        UILoadMenuMode loadProgressMode;
        Uint32 loadProgressStartedTicks;
        Uint32 loadProgressFinishedTicks;
        int loadProgressPermille;
        char loadProgressPath[MAX_CONFIG_PATH];
        char loadProgressLabel[128];
        char loadProgressDetail[160];
        bool asyncStlActive;
        SDL_Thread* asyncStlThread;
        SDL_atomic_t asyncStlComplete;
        SDL_atomic_t asyncStlProgressPermille;
        SDL_atomic_t asyncStlProgressStage;
        bool asyncStlSucceeded;
        char asyncStlSourcePath[MAX_CONFIG_PATH];
        char asyncStlAssetRoot[MAX_CONFIG_PATH];
        char asyncStlAuthoringPath[MAX_CONFIG_PATH];
        char asyncStlRuntimePath[MAX_CONFIG_PATH];
        char asyncStlDiagnostics[256];
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
    int tab_height_px;
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
bool UIPanel_OpenObjectAssetFolderDialog(void);
bool UIPanel_OpenDirectoryDialogForActiveBrowser(void);
void UIPanel_ExportShape(void);
void UIPanel_ExportScene(void);
void UIPanel_SetFilePaneActionStatus(const char* status);
bool UIPanel_FilePaneActionStatusIsLive(const UIPanelState* ui);
bool UIPanel_ExportObjectRuntimeMesh(void);
bool UIPanel_PlaceLastRuntimeMeshAsSceneInstance(void);
bool UIPanel_PlaceRuntimeMeshAsSceneInstance(const char* runtime_mesh_path);
bool UIPanel_PlaceImportedStlRuntimeMesh(const char* stl_path,
                                         const char* authoring_path,
                                         const char* runtime_path);
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
void UIPanel_LoadFileBrowserMode(UIPanelState* ui);
void UIPanel_SetFileBrowserVisible(UIPanelState* ui, bool visible);
void UIPanel_CloseFileBrowser(UIPanelState* ui);
void UIPanel_TickLoadProgress(void);
void UIPanel_WaitForAsyncStlImport(void);
void UIPanel_ActivateJsonBrowser(void);
void UIPanel_ActivateSceneBrowser(void);
void UIPanel_ActivateObjectAssetBrowser(void);
void UIPanel_ActivateRuntimeMeshBrowser(void);
void UIPanel_ActivateStlImportBrowser(void);
bool UIPanel_LoadStlFromFolderSelection(const char* selected_folder, bool persist_root);
bool UIPanel_OpenStlFolderDialog(void);
bool UIPanel_ImportStlAndPlaceFromPath(const char* stl_path);
bool UIPanel_FocusFileBrowserOnActiveSession(void);
bool UIPanel_ClearRememberedFileBrowserEntry(void);
bool UIPanel_RestorePersistedFileSession(void);
bool UIPanel_GetFileBrowserRestoreSummary(
    const UIPanelState* ui,
    UIPanelFileBrowserRestoreSummary* out_summary);
bool UIPanel_GetFileBrowserSelectionInfo(const UIPanelState* ui,
                                         UILoadMenuSelectionState* out_state,
                                         const char** out_path);
bool UIPanel_GetFileBrowserRowSelectionState(const UIPanelState* ui,
                                             int index,
                                             UILoadMenuSelectionState* out_state);
bool UIPanel_GetFileBrowserStatusText(const UIPanelState* ui,
                                      char* out_text,
                                      size_t out_text_size);
bool UIPanel_GetFileBrowserActionHintText(const UIPanelState* ui,
                                          char* out_text,
                                          size_t out_text_size);
void Render_UIPanelFileBrowser(const UIPanelState* ui, SDL_Renderer* renderer);
void UIPanel_ResetTransientUiState(void);
void UIPanel_HandleMouseMotion(int mouseX, int mouseY);
void UIPanel_BeginInputRootDialog(void);
void UIPanel_BeginOutputRootDialog(void);
void UIPanel_BeginObjectAssetRootDialog(void);
bool UIPanel_BeginPrismWidthDialog(void);
bool UIPanel_BeginPrismHeightDialog(void);
bool UIPanel_BeginPrismDepthDialog(void);
void UIPanel_CycleDisplayUnit(void);
bool UIPanel_SetDisplayUnit(CoreUnitKind unit);
CoreUnitKind UIPanel_GetDisplayUnit(void);
const char* UIPanel_GetDisplayUnitSymbol(void);
bool UIPanel_ToggleObjectGizmoRotateMode(void);
bool UIPanel_IsObjectGizmoRotateMode(void);
bool UIPanel_IsObjectGizmoSizeMode(void);
const char* UIPanel_ObjectGizmoModeLabel(void);
bool UIPanel_ToggleSceneBoundsEnabled(void);
bool UIPanel_ToggleSceneBoundsClampOnEdit(void);
bool UIPanel_BeginSceneBoundsMinDialog(void);
bool UIPanel_BeginSceneBoundsMaxDialog(void);
bool UIPanel_SetConstructionPlaneAxis(ViewPlaneAxis axis);
bool UIPanel_AdjustConstructionPlaneOffset(float delta_world);
bool UIPanel_BeginConstructionPlaneOffsetDialog(void);
bool UIPanel_AdjustObjectExtrudeDepth(int direction);
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
bool UIPanel_CreateSceneAuthoringLight(void);
bool UIPanel_CreateSceneAuthoringCameraPath(void);
bool UIPanel_CreateSceneAuthoringMaterial(void);
bool UIPanel_FitSceneBoundsToSelectedObject(void);
