#pragma once

#include "Core/SDLApp/sdl_app_framework.h"
#include "Menu/line_drawing_catalog_preview.h"
#include "Menu/line_drawing_recent_contexts.h"
#include "Menu/line_drawing_root_browser.h"
#include "Menu/line_drawing_scene_catalog.h"

#include <stdbool.h>

typedef enum LineDrawingHostMenuItemId {
    LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR = 0,
    LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT,
    LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE,
    LINE_DRAWING_HOST_MENU_ITEM_QUIT,
    LINE_DRAWING_HOST_MENU_ITEM_COUNT
} LineDrawingHostMenuItemId;

typedef enum LineDrawingHostMenuCommandType {
    LINE_DRAWING_HOST_MENU_COMMAND_NONE = 0,
    LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR,
    LINE_DRAWING_HOST_MENU_COMMAND_QUIT
} LineDrawingHostMenuCommandType;

typedef enum LineDrawingHostMenuSection {
    LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS = 0,
    LINE_DRAWING_HOST_MENU_SECTION_RECENTS,
    LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS,
    LINE_DRAWING_HOST_MENU_SECTION_SCENES,
    LINE_DRAWING_HOST_MENU_SECTION_BROWSE,
    LINE_DRAWING_HOST_MENU_SECTION_COUNT
} LineDrawingHostMenuSection;

typedef enum LineDrawingHostMenuFocusRegion {
    LINE_DRAWING_HOST_MENU_FOCUS_NAV = 0,
    LINE_DRAWING_HOST_MENU_FOCUS_FILTER,
    LINE_DRAWING_HOST_MENU_FOCUS_CONTENT
} LineDrawingHostMenuFocusRegion;

typedef struct LineDrawingHostMenuCommand {
    LineDrawingHostMenuCommandType type;
} LineDrawingHostMenuCommand;

typedef struct LineDrawingHostMenuModel {
    bool item_enabled[LINE_DRAWING_HOST_MENU_ITEM_COUNT];
} LineDrawingHostMenuModel;

typedef struct LineDrawingHostMenuState {
    int selected_index;
    LineDrawingHostMenuSection selected_section;
    LineDrawingHostMenuFocusRegion focus_region;
    int hovered_section_index;
    int hovered_content_index;
    int hovered_browse_action_index;
    bool hovered_filter;
    bool scrollbar_dragging;
    int scrollbar_drag_start_y;
    float scrollbar_drag_start_offset_px;
    int selected_recent_index;
    int selected_layout_index;
    int selected_scene_index;
    int selected_browser_index;
    float recent_scroll_px;
    float layout_scroll_px;
    float scene_scroll_px;
    float browser_scroll_px;
    int filtered_layout_count;
    int filtered_scene_count;
    int filtered_layout_indices[MAX_CONFIG_FILES];
    int filtered_scene_indices[MAX_CONFIG_FILES];
    LineDrawingRecentMenuList recent_entries;
    LineDrawingSceneCatalog catalog;
    LineDrawingRootBrowser browser;
    LineDrawingCatalogPreviewCache preview_cache;
    char filter_query[96];
    bool filter_editing;
    char status_text[256];
    bool status_is_error;
} LineDrawingHostMenuState;

void LineDrawingHostMenu_Init(LineDrawingHostMenuState* state);
void LineDrawingHostMenu_BuildModel(LineDrawingHostMenuModel* out_model);
int LineDrawingHostMenu_FirstSelectableIndex(const LineDrawingHostMenuModel* model);
int LineDrawingHostMenu_MoveSelection(const LineDrawingHostMenuModel* model,
                                      int current_index,
                                      int direction);
bool LineDrawingHostMenu_HandleEvent(LineDrawingHostMenuState* state,
                                     AppContext* ctx,
                                     const SDL_Event* event,
                                     LineDrawingHostMenuCommand* out_command);
void LineDrawingHostMenu_Render(LineDrawingHostMenuState* state, AppContext* ctx);
