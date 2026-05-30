#pragma once

#include "Menu/line_drawing_host_menu.h"

#include <SDL2/SDL.h>

enum {
    LINE_DRAWING_HOST_MENU_CARD_MAX_WIDTH = 1280,
    LINE_DRAWING_HOST_MENU_CARD_MIN_WIDTH = 940,
    LINE_DRAWING_HOST_MENU_CARD_MAX_HEIGHT = 860,
    LINE_DRAWING_HOST_MENU_CARD_MIN_HEIGHT = 640,
    LINE_DRAWING_HOST_MENU_OUTER_MARGIN = 20,
    LINE_DRAWING_HOST_MENU_CARD_PADDING = 18,
    LINE_DRAWING_HOST_MENU_REGION_GAP = 14,
    LINE_DRAWING_HOST_MENU_HEADER_HEIGHT = 96,
    LINE_DRAWING_HOST_MENU_FOOTER_HEIGHT = 94,
    LINE_DRAWING_HOST_MENU_NAV_WIDTH = 194,
    LINE_DRAWING_HOST_MENU_DETAIL_WIDTH = 328,
    LINE_DRAWING_HOST_MENU_NAV_ROW_HEIGHT = 42,
    LINE_DRAWING_HOST_MENU_LIST_HEADER_HEIGHT = 78,
    LINE_DRAWING_HOST_MENU_QUICK_ROW_HEIGHT = 68,
    LINE_DRAWING_HOST_MENU_CATALOG_ROW_HEIGHT = 72,
    LINE_DRAWING_HOST_MENU_ROW_GAP = 8,
    LINE_DRAWING_HOST_MENU_SCROLLBAR_W = 10,
    LINE_DRAWING_HOST_MENU_SCROLLBAR_GUTTER = 8,
    LINE_DRAWING_HOST_MENU_SCROLLBAR_MIN_THUMB_H = 12,
    LINE_DRAWING_HOST_MENU_PREVIEW_HEIGHT = 154,
    LINE_DRAWING_HOST_MENU_ROW_PREVIEW_WIDTH = 92,
    LINE_DRAWING_HOST_MENU_ACCENT_WIDTH = 4
};

typedef enum LineDrawingHostMenuBrowseAction {
    LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT = 0,
    LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_OUTPUT,
    LINE_DRAWING_HOST_MENU_BROWSE_ACTION_COUNT
} LineDrawingHostMenuBrowseAction;

typedef struct LineDrawingHostMenuLayout {
    SDL_Rect card_rect;
    SDL_Rect header_rect;
    SDL_Rect body_rect;
    SDL_Rect footer_rect;
    SDL_Rect nav_rect;
    SDL_Rect list_rect;
    SDL_Rect detail_rect;
    SDL_Rect nav_rows[LINE_DRAWING_HOST_MENU_SECTION_COUNT];
    SDL_Rect list_header_rect;
    SDL_Rect filter_rect;
    SDL_Rect browse_action_rects[LINE_DRAWING_HOST_MENU_BROWSE_ACTION_COUNT];
    SDL_Rect list_view_rect;
    SDL_Rect list_scrollbar_rect;
    SDL_Rect detail_preview_rect;
    SDL_Rect status_rect;
    SDL_Rect keyboard_rect;
    SDL_Rect input_root_rect;
    SDL_Rect output_root_rect;
} LineDrawingHostMenuLayout;

bool line_drawing_host_menu_section_supports_filter(LineDrawingHostMenuSection section);
int line_drawing_host_menu_content_row_height(LineDrawingHostMenuSection section);
int line_drawing_host_menu_content_count(const LineDrawingHostMenuState* state,
                                         LineDrawingHostMenuSection section);
int* line_drawing_host_menu_selected_content_index_ptr(LineDrawingHostMenuState* state,
                                                       LineDrawingHostMenuSection section);
float* line_drawing_host_menu_scroll_ptr(LineDrawingHostMenuState* state,
                                         LineDrawingHostMenuSection section);
void line_drawing_host_menu_layout(LineDrawingHostMenuLayout* out_layout,
                                   int screen_width,
                                   int screen_height);
int line_drawing_host_menu_hit_test_nav(const LineDrawingHostMenuLayout* layout,
                                        int mouse_x,
                                        int mouse_y);
bool line_drawing_host_menu_hit_test_filter(const LineDrawingHostMenuState* state,
                                            const LineDrawingHostMenuLayout* layout,
                                            int mouse_x,
                                            int mouse_y);
int line_drawing_host_menu_hit_test_browse_action(const LineDrawingHostMenuState* state,
                                                  const LineDrawingHostMenuLayout* layout,
                                                  int mouse_x,
                                                  int mouse_y);
void line_drawing_host_menu_clamp_scroll(LineDrawingHostMenuState* state,
                                         const LineDrawingHostMenuLayout* layout,
                                         LineDrawingHostMenuSection section);
bool line_drawing_host_menu_has_scrollbar(const LineDrawingHostMenuState* state,
                                          const LineDrawingHostMenuLayout* layout,
                                          LineDrawingHostMenuSection section);
SDL_Rect line_drawing_host_menu_scrollbar_thumb_rect(const LineDrawingHostMenuState* state,
                                                     const LineDrawingHostMenuLayout* layout,
                                                     LineDrawingHostMenuSection section);
bool line_drawing_host_menu_scrollbar_drag_to(LineDrawingHostMenuState* state,
                                              const LineDrawingHostMenuLayout* layout,
                                              LineDrawingHostMenuSection section,
                                              int mouse_y);
bool line_drawing_host_menu_jump_scrollbar_to(LineDrawingHostMenuState* state,
                                              const LineDrawingHostMenuLayout* layout,
                                              LineDrawingHostMenuSection section,
                                              int mouse_y);
void line_drawing_host_menu_ensure_selected_visible(LineDrawingHostMenuState* state,
                                                    const LineDrawingHostMenuLayout* layout,
                                                    LineDrawingHostMenuSection section);
int line_drawing_host_menu_hit_test_content(const LineDrawingHostMenuState* state,
                                            const LineDrawingHostMenuLayout* layout,
                                            int mouse_x,
                                            int mouse_y);
bool line_drawing_host_menu_hit_test_scrollbar_track(const LineDrawingHostMenuState* state,
                                                     const LineDrawingHostMenuLayout* layout,
                                                     int mouse_x,
                                                     int mouse_y);
const LineDrawingSceneCatalogEntry* line_drawing_host_menu_selected_catalog_entry(
    const LineDrawingHostMenuState* state,
    LineDrawingHostMenuSection section,
    bool* out_is_active);
const LineDrawingRootBrowserEntry* line_drawing_host_menu_selected_browser_entry(
    const LineDrawingHostMenuState* state);
const LineDrawingRecentMenuEntry* line_drawing_host_menu_selected_recent_entry(
    const LineDrawingHostMenuState* state);
const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_entry(
    LineDrawingHostMenuState* state,
    LineDrawingHostMenuSection section,
    const LineDrawingSceneCatalogEntry* entry);
const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_recent_entry(
    LineDrawingHostMenuState* state,
    const LineDrawingRecentMenuEntry* entry);
const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_browser_entry(
    LineDrawingHostMenuState* state,
    const LineDrawingRootBrowserEntry* entry);
