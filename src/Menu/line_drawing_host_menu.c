#include "Menu/line_drawing_host_menu.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"

#include <SDL2/SDL.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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

static const char* line_drawing_host_menu_item_label(LineDrawingHostMenuItemId item_id) {
    switch (item_id) {
        case LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR:
            return "Resume Editor";
        case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT:
            return "Reopen Last Layout";
        case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE:
            return "Reopen Last Scene";
        case LINE_DRAWING_HOST_MENU_ITEM_QUIT:
            return "Quit";
        default:
            return "";
    }
}

static const char* line_drawing_host_menu_item_description(LineDrawingHostMenuItemId item_id) {
    switch (item_id) {
        case LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR:
            return "Return to the current editing workspace without changing the loaded document.";
        case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT:
            return "Load the most recent layout JSON path tracked by the editor host.";
        case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE:
            return "Load the most recent imported authoring scene from the input root.";
        case LINE_DRAWING_HOST_MENU_ITEM_QUIT:
            return "Close the host from the menu surface.";
        default:
            return "";
    }
}

static const char* line_drawing_host_menu_section_label(LineDrawingHostMenuSection section) {
    switch (section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            return "Quick Actions";
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            return "Recents";
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            return "Layouts";
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            return "Scenes";
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            return "Browse";
        default:
            return "";
    }
}

static bool line_drawing_host_menu_path_exists(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static const char* line_drawing_host_menu_path_basename(const char* path) {
    const char* slash = NULL;
    if (!path || !path[0]) return "";
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static const char* line_drawing_host_menu_browse_action_label(LineDrawingHostMenuBrowseAction action) {
    switch (action) {
        case LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT:
            return "Pick Input Root...";
        case LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_OUTPUT:
            return "Pick Output Root...";
        default:
            return "";
    }
}

static void line_drawing_host_menu_clear_hover(LineDrawingHostMenuState* state);

static int line_drawing_host_menu_font_height(TTF_Font* font) {
    int height = 16;
    if (font) {
        height = TTF_FontHeight(font);
    }
    if (height < 14) height = 14;
    return height;
}

static SDL_Color line_drawing_host_menu_dim(SDL_Color color, Uint8 alpha) {
    SDL_Color out = color;
    out.a = alpha;
    out.r = (Uint8)((out.r * 3u) / 5u);
    out.g = (Uint8)((out.g * 3u) / 5u);
    out.b = (Uint8)((out.b * 3u) / 5u);
    return out;
}

static SDL_Color line_drawing_host_menu_mix(SDL_Color a, SDL_Color b, float t) {
    SDL_Color out = a;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    out.r = (Uint8)((a.r * (1.0f - t)) + (b.r * t));
    out.g = (Uint8)((a.g * (1.0f - t)) + (b.g * t));
    out.b = (Uint8)((a.b * (1.0f - t)) + (b.b * t));
    out.a = (Uint8)((a.a * (1.0f - t)) + (b.a * t));
    return out;
}

static bool line_drawing_host_menu_section_supports_filter(LineDrawingHostMenuSection section) {
    return section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS ||
           section == LINE_DRAWING_HOST_MENU_SECTION_SCENES;
}

static void line_drawing_host_menu_set_status(LineDrawingHostMenuState* state,
                                              const char* text,
                                              bool is_error) {
    if (!state) return;
    if (!text) text = "";
    snprintf(state->status_text, sizeof(state->status_text), "%s", text);
    state->status_is_error = is_error;
}

static void line_drawing_host_menu_draw_text(SDL_Renderer* renderer,
                                             TTF_Font* font,
                                             const char* text,
                                             int x,
                                             int y,
                                             SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void line_drawing_host_menu_build_ellipsized_text(SDL_Renderer* renderer,
                                                         TTF_Font* font,
                                                         const char* text,
                                                         int max_width,
                                                         char* out,
                                                         size_t out_size) {
    static const char* k_ellipsis = "...";
    int width = 0;
    int ellipsis_width = 0;
    size_t len = 0u;
    if (!font || !text || !out || out_size == 0u || max_width <= 0) {
        if (out && out_size > 0u) out[0] = '\0';
        return;
    }
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) && width <= max_width) {
        snprintf(out, out_size, "%s", text);
        return;
    }
    if (!line_drawing_text_measure_utf8(renderer, font, k_ellipsis, &ellipsis_width, NULL) ||
        ellipsis_width >= max_width) {
        out[0] = '\0';
        return;
    }
    len = strlen(text);
    while (len > 0u) {
        --len;
        if (len + strlen(k_ellipsis) + 1u >= out_size) continue;
        memcpy(out, text, len);
        out[len] = '\0';
        strcat(out, k_ellipsis);
        if (line_drawing_text_measure_utf8(renderer, font, out, &width, NULL) && width <= max_width) {
            return;
        }
    }
    out[0] = '\0';
}

static void line_drawing_host_menu_draw_text_clipped(SDL_Renderer* renderer,
                                                     TTF_Font* font,
                                                     const char* text,
                                                     int x,
                                                     int y,
                                                     int max_width,
                                                     SDL_Color color) {
    char clipped[512];
    line_drawing_host_menu_build_ellipsized_text(renderer,
                                                 font,
                                                 text,
                                                 max_width,
                                                 clipped,
                                                 sizeof(clipped));
    line_drawing_host_menu_draw_text(renderer, font, clipped, x, y, color);
}

static void line_drawing_host_menu_draw_panel(SDL_Renderer* renderer,
                                              SDL_Rect rect,
                                              SDL_Color fill_color,
                                              SDL_Color border_color,
                                              SDL_Color shadow_color) {
    SDL_Rect shadow_rect = rect;
    if (!renderer) return;
    shadow_rect.x += 3;
    shadow_rect.y += 4;
    SDL_SetRenderDrawColor(renderer,
                           shadow_color.r,
                           shadow_color.g,
                           shadow_color.b,
                           shadow_color.a);
    SDL_RenderFillRect(renderer, &shadow_rect);
    SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, fill_color.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, border_color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

static void line_drawing_host_menu_draw_accent_bar(SDL_Renderer* renderer,
                                                   SDL_Rect rect,
                                                   SDL_Color accent_color) {
    SDL_Rect accent = rect;
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    accent.w = LINE_DRAWING_HOST_MENU_ACCENT_WIDTH;
    SDL_SetRenderDrawColor(renderer,
                           accent_color.r,
                           accent_color.g,
                           accent_color.b,
                           accent_color.a);
    SDL_RenderFillRect(renderer, &accent);
}

static void line_drawing_host_menu_draw_badge(SDL_Renderer* renderer,
                                              TTF_Font* font,
                                              const char* label,
                                              SDL_Rect rect,
                                              SDL_Color fill_color,
                                              SDL_Color border_color,
                                              SDL_Color text_color) {
    int text_width = 0;
    int text_height = 0;
    SDL_Rect badge_rect = rect;
    if (!renderer || !font || !label || !label[0]) return;
    (void)line_drawing_text_measure_utf8(renderer, font, label, &text_width, &text_height);
    badge_rect.w = text_width + 14;
    if (badge_rect.w < 46) badge_rect.w = 46;
    if (badge_rect.w > rect.w) badge_rect.w = rect.w;
    badge_rect.h = text_height + 8;
    if (badge_rect.h > rect.h) badge_rect.h = rect.h;

    SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, fill_color.a);
    SDL_RenderFillRect(renderer, &badge_rect);
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, border_color.a);
    SDL_RenderDrawRect(renderer, &badge_rect);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             label,
                                             badge_rect.x + 7,
                                             badge_rect.y + 4,
                                             badge_rect.w - 12,
                                             text_color);
}

static int line_drawing_host_menu_content_row_height(LineDrawingHostMenuSection section) {
    return (section == LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS)
               ? LINE_DRAWING_HOST_MENU_QUICK_ROW_HEIGHT
               : LINE_DRAWING_HOST_MENU_CATALOG_ROW_HEIGHT;
}

static int line_drawing_host_menu_section_count_value(const LineDrawingHostMenuState* state,
                                                      LineDrawingHostMenuSection section) {
    if (!state) return 0;
    switch (section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            return LINE_DRAWING_HOST_MENU_ITEM_COUNT;
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            return state->recent_entries.entry_count;
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            return state->catalog.layout_count;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            return state->catalog.scene_count;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            return state->browser.entry_count;
        default:
            return 0;
    }
}

static void line_drawing_host_menu_stop_filter_editing(LineDrawingHostMenuState* state) {
    if (!state) return;
    state->filter_editing = false;
    if (SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
}

static void line_drawing_host_menu_stop_scrollbar_drag(LineDrawingHostMenuState* state) {
    if (!state) return;
    state->scrollbar_dragging = false;
    state->scrollbar_drag_start_y = 0;
    state->scrollbar_drag_start_offset_px = 0.0f;
}

static void line_drawing_host_menu_begin_filter_editing(LineDrawingHostMenuState* state) {
    if (!state) return;
    if (!line_drawing_host_menu_section_supports_filter(state->selected_section)) return;
    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_FILTER;
    state->filter_editing = true;
    if (!SDL_IsTextInputActive()) {
        SDL_StartTextInput();
    }
}

static void line_drawing_host_menu_apply_filter(LineDrawingHostMenuState* state) {
    int i = 0;
    int current_layout_actual = -1;
    int current_scene_actual = -1;
    int preferred_layout_visible = -1;
    int preferred_scene_visible = -1;
    int active_layout_visible = -1;
    int active_scene_visible = -1;

    if (!state) return;
    if (state->selected_layout_index >= 0 && state->selected_layout_index < state->filtered_layout_count) {
        current_layout_actual = state->filtered_layout_indices[state->selected_layout_index];
    }
    if (state->selected_scene_index >= 0 && state->selected_scene_index < state->filtered_scene_count) {
        current_scene_actual = state->filtered_scene_indices[state->selected_scene_index];
    }
    state->filtered_layout_count = 0;
    state->filtered_scene_count = 0;

    for (i = 0; i < state->catalog.layout_count && i < MAX_CONFIG_FILES; ++i) {
        const LineDrawingSceneCatalogEntry* entry = LineDrawingSceneCatalog_GetLayout(&state->catalog, i);
        if (LineDrawingSceneCatalog_EntryMatchesQuery(entry, state->filter_query)) {
            if (i == state->catalog.active_layout_index) {
                active_layout_visible = state->filtered_layout_count;
            }
            if (i == current_layout_actual) {
                preferred_layout_visible = state->filtered_layout_count;
            }
            state->filtered_layout_indices[state->filtered_layout_count++] = i;
        }
    }

    for (i = 0; i < state->catalog.scene_count && i < MAX_CONFIG_FILES; ++i) {
        const LineDrawingSceneCatalogEntry* entry = LineDrawingSceneCatalog_GetScene(&state->catalog, i);
        if (LineDrawingSceneCatalog_EntryMatchesQuery(entry, state->filter_query)) {
            if (i == state->catalog.active_scene_index) {
                active_scene_visible = state->filtered_scene_count;
            }
            if (i == current_scene_actual) {
                preferred_scene_visible = state->filtered_scene_count;
            }
            state->filtered_scene_indices[state->filtered_scene_count++] = i;
        }
    }

    if (state->filtered_layout_count <= 0) {
        state->selected_layout_index = -1;
        state->layout_scroll_px = 0.0f;
    } else if (state->selected_layout_index < 0 ||
               state->selected_layout_index >= state->filtered_layout_count) {
        state->selected_layout_index = preferred_layout_visible >= 0
                                           ? preferred_layout_visible
                                           : (active_layout_visible >= 0 ? active_layout_visible : 0);
    } else if (preferred_layout_visible >= 0) {
        state->selected_layout_index = preferred_layout_visible;
    }

    if (state->filtered_scene_count <= 0) {
        state->selected_scene_index = -1;
        state->scene_scroll_px = 0.0f;
    } else if (state->selected_scene_index < 0 ||
               state->selected_scene_index >= state->filtered_scene_count) {
        state->selected_scene_index = preferred_scene_visible >= 0
                                          ? preferred_scene_visible
                                          : (active_scene_visible >= 0 ? active_scene_visible : 0);
    } else if (preferred_scene_visible >= 0) {
        state->selected_scene_index = preferred_scene_visible;
    }
}

static const LineDrawingSceneCatalogEntry* line_drawing_host_menu_selected_catalog_entry(
    const LineDrawingHostMenuState* state,
    LineDrawingHostMenuSection section,
    bool* out_is_active) {
    int visible_index = -1;
    int actual_index = -1;
    if (out_is_active) *out_is_active = false;
    if (!state) return NULL;

    if (section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS) {
        visible_index = state->selected_layout_index;
        if (visible_index < 0 || visible_index >= state->filtered_layout_count) return NULL;
        actual_index = state->filtered_layout_indices[visible_index];
        if (out_is_active) *out_is_active = (actual_index == state->catalog.active_layout_index);
        return LineDrawingSceneCatalog_GetLayout(&state->catalog, actual_index);
    }
    if (section == LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
        visible_index = state->selected_scene_index;
        if (visible_index < 0 || visible_index >= state->filtered_scene_count) return NULL;
        actual_index = state->filtered_scene_indices[visible_index];
        if (out_is_active) *out_is_active = (actual_index == state->catalog.active_scene_index);
        return LineDrawingSceneCatalog_GetScene(&state->catalog, actual_index);
    }
    return NULL;
}

static const LineDrawingRootBrowserEntry* line_drawing_host_menu_selected_browser_entry(
    const LineDrawingHostMenuState* state) {
    if (!state) return NULL;
    return LineDrawingRootBrowser_GetEntry(&state->browser, state->selected_browser_index);
}

static const LineDrawingRecentMenuEntry* line_drawing_host_menu_selected_recent_entry(
    const LineDrawingHostMenuState* state) {
    if (!state) return NULL;
    return LineDrawingRecentMenuList_GetEntry(&state->recent_entries, state->selected_recent_index);
}

static LineDrawingCatalogPreviewSourceKind line_drawing_host_menu_preview_kind_for_section(
    LineDrawingHostMenuSection section) {
    return (section == LINE_DRAWING_HOST_MENU_SECTION_SCENES)
               ? LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE
               : LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
}

static const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_entry(
    LineDrawingHostMenuState* state,
    LineDrawingHostMenuSection section,
    const LineDrawingSceneCatalogEntry* entry) {
    if (!state || !entry || !entry->path[0]) return NULL;
    if (section != LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS &&
        section != LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
        return NULL;
    }
    return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                              line_drawing_host_menu_preview_kind_for_section(section),
                                              entry->path);
}

static const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_recent_entry(
    LineDrawingHostMenuState* state,
    const LineDrawingRecentMenuEntry* entry) {
    if (!state || !entry || !entry->path[0]) return NULL;
    switch (entry->kind) {
        case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT:
            return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                                      LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT,
                                                      entry->path);
        case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE:
            return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                                      LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE,
                                                      entry->path);
        default:
            return NULL;
    }
}

static const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_browser_entry(
    LineDrawingHostMenuState* state,
    const LineDrawingRootBrowserEntry* entry) {
    if (!state || !entry || !entry->preview_path[0]) return NULL;
    return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                              entry->preview_kind,
                                              entry->preview_path);
}

static void line_drawing_host_menu_format_preview_summary(const LineDrawingCatalogPreviewData* preview,
                                                          char* out,
                                                          size_t out_size) {
    if (!out || out_size == 0u) return;
    if (!preview) {
        out[0] = '\0';
        return;
    }
    snprintf(out,
             out_size,
             "%d obj  %dP %dR  %dW %dA",
             preview->object_count,
             preview->plane_count,
             preview->rect_prism_count,
             preview->wall_count,
             preview->anchor_count);
}

static void line_drawing_host_menu_format_preview_extents(const LineDrawingCatalogPreviewData* preview,
                                                          char* out,
                                                          size_t out_size) {
    if (!out || out_size == 0u) return;
    if (!preview) {
        out[0] = '\0';
        return;
    }
    snprintf(out,
             out_size,
             "%.1f x %.1f x %.1f",
             preview->extent_x,
             preview->extent_y,
             preview->extent_z);
}

static void line_drawing_host_menu_draw_preview(SDL_Renderer* renderer,
                                                SDL_Rect rect,
                                                const LineDrawingCatalogPreviewData* preview,
                                                SDL_Color background_color,
                                                SDL_Color border_color,
                                                SDL_Color line_color,
                                                SDL_Color muted_color) {
    int i = 0;
    if (!renderer) return;

    SDL_SetRenderDrawColor(renderer,
                           background_color.r,
                           background_color.g,
                           background_color.b,
                           background_color.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           border_color.r,
                           border_color.g,
                           border_color.b,
                           border_color.a);
    SDL_RenderDrawRect(renderer, &rect);

    if (!preview || !preview->has_preview || preview->segment_count <= 0) {
        SDL_SetRenderDrawColor(renderer, muted_color.r, muted_color.g, muted_color.b, muted_color.a);
        SDL_RenderDrawLine(renderer,
                           rect.x + 6,
                           rect.y + rect.h - 7,
                           rect.x + rect.w - 6,
                           rect.y + 6);
        return;
    }

    SDL_SetRenderDrawColor(renderer, line_color.r, line_color.g, line_color.b, line_color.a);
    for (i = 0; i < preview->segment_count; ++i) {
        const LineDrawingCatalogPreviewSegment* segment = &preview->segments[i];
        const int x0 = rect.x + (int)(segment->x0 * (float)(rect.w - 1));
        const int y0 = rect.y + (int)((1.0f - segment->y0) * (float)(rect.h - 1));
        const int x1 = rect.x + (int)(segment->x1 * (float)(rect.w - 1));
        const int y1 = rect.y + (int)((1.0f - segment->y1) * (float)(rect.h - 1));
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
}

static void line_drawing_host_menu_refresh_catalog(LineDrawingHostMenuState* state) {
    if (!state) return;
    LineDrawingRecentMenuList_Refresh(&state->recent_entries,
                                      Global_GetRecentContexts(),
                                      Global_GetLastLayoutPath(),
                                      Global_GetLastSceneAuthoringPath(),
                                      Global_GetInputRoot(),
                                      Global_GetOutputRoot());
    LineDrawingSceneCatalog_Refresh(&state->catalog,
                                    Global_GetInputRoot(),
                                    Global_GetCurrentConfigPath(),
                                    Global_GetCurrentSceneAuthoringPath());
    LineDrawingRootBrowser_Refresh(&state->browser,
                                   state->browser.current_path,
                                   Global_GetInputRoot(),
                                   Global_GetOutputRoot());
    {
        LineDrawingHostMenuModel model;
        LineDrawingHostMenu_BuildModel(&model);
        if (state->selected_index < 0 ||
            state->selected_index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT ||
            !model.item_enabled[state->selected_index]) {
            state->selected_index = LineDrawingHostMenu_FirstSelectableIndex(&model);
        }
    }

    if (state->selected_section < 0 || state->selected_section >= LINE_DRAWING_HOST_MENU_SECTION_COUNT) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS;
    }
    if (!line_drawing_host_menu_section_supports_filter(state->selected_section)) {
        line_drawing_host_menu_stop_filter_editing(state);
        if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        }
    }
    line_drawing_host_menu_apply_filter(state);
    if (state->recent_entries.entry_count <= 0) {
        state->selected_recent_index = -1;
        state->recent_scroll_px = 0.0f;
    } else if (state->selected_recent_index < 0 ||
               state->selected_recent_index >= state->recent_entries.entry_count) {
        state->selected_recent_index = 0;
    }
    if (state->browser.entry_count <= 0) {
        state->selected_browser_index = -1;
        state->browser_scroll_px = 0.0f;
    } else if (state->selected_browser_index < 0 ||
               state->selected_browser_index >= state->browser.entry_count) {
        state->selected_browser_index = 0;
    }
}

static int line_drawing_host_menu_content_count(const LineDrawingHostMenuState* state,
                                                LineDrawingHostMenuSection section) {
    if (!state) return 0;
    switch (section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            return LINE_DRAWING_HOST_MENU_ITEM_COUNT;
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            return state->recent_entries.entry_count;
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            return state->filtered_layout_count;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            return state->filtered_scene_count;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            return state->browser.entry_count;
        default:
            return 0;
    }
}

static int* line_drawing_host_menu_selected_content_index_ptr(LineDrawingHostMenuState* state,
                                                              LineDrawingHostMenuSection section) {
    if (!state) return NULL;
    switch (section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            return &state->selected_index;
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            return &state->selected_recent_index;
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            return &state->selected_layout_index;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            return &state->selected_scene_index;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            return &state->selected_browser_index;
        default:
            return NULL;
    }
}

static float* line_drawing_host_menu_scroll_ptr(LineDrawingHostMenuState* state,
                                                LineDrawingHostMenuSection section) {
    if (!state) return NULL;
    switch (section) {
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            return &state->recent_scroll_px;
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            return &state->layout_scroll_px;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            return &state->scene_scroll_px;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            return &state->browser_scroll_px;
        default:
            return NULL;
    }
}

static int line_drawing_host_menu_move_content_selection(const LineDrawingHostMenuState* state,
                                                         const LineDrawingHostMenuModel* model,
                                                         LineDrawingHostMenuSection section,
                                                         int current_index,
                                                         int direction) {
    int count = line_drawing_host_menu_content_count(state, section);
    int index = current_index;
    int steps = 0;
    if (!state) return current_index;
    if (section == LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS) {
        return LineDrawingHostMenu_MoveSelection(model, current_index, direction);
    }
    if (direction == 0 || count <= 0) return current_index;
    if (current_index < 0 || current_index >= count) return 0;
    while (steps < count) {
        index += direction > 0 ? 1 : -1;
        if (index < 0) index = count - 1;
        if (index >= count) index = 0;
        return index;
    }
    return current_index;
}

static void line_drawing_host_menu_layout(LineDrawingHostMenuLayout* out_layout,
                                          int screen_width,
                                          int screen_height) {
    SDL_Rect inner = {0, 0, 0, 0};
    int card_width = 0;
    int card_height = 0;
    int right_width = LINE_DRAWING_HOST_MENU_DETAIL_WIDTH;
    int nav_y = 0;
    int i = 0;

    if (!out_layout) return;
    memset(out_layout, 0, sizeof(*out_layout));

    if (screen_width <= 0) screen_width = 1440;
    if (screen_height <= 0) screen_height = 900;

    card_width = screen_width - (LINE_DRAWING_HOST_MENU_OUTER_MARGIN * 2);
    if (card_width > LINE_DRAWING_HOST_MENU_CARD_MAX_WIDTH) {
        card_width = LINE_DRAWING_HOST_MENU_CARD_MAX_WIDTH;
    }
    if (card_width < LINE_DRAWING_HOST_MENU_CARD_MIN_WIDTH) {
        card_width = LINE_DRAWING_HOST_MENU_CARD_MIN_WIDTH;
    }
    if (card_width > screen_width - 20) {
        card_width = screen_width - 20;
    }

    card_height = screen_height - (LINE_DRAWING_HOST_MENU_OUTER_MARGIN * 2);
    if (card_height > LINE_DRAWING_HOST_MENU_CARD_MAX_HEIGHT) {
        card_height = LINE_DRAWING_HOST_MENU_CARD_MAX_HEIGHT;
    }
    if (card_height < LINE_DRAWING_HOST_MENU_CARD_MIN_HEIGHT) {
        card_height = LINE_DRAWING_HOST_MENU_CARD_MIN_HEIGHT;
    }
    if (card_height > screen_height - 20) {
        card_height = screen_height - 20;
    }

    out_layout->card_rect = (SDL_Rect){
        (screen_width - card_width) / 2,
        (screen_height - card_height) / 2,
        card_width,
        card_height
    };

    out_layout->header_rect = (SDL_Rect){
        out_layout->card_rect.x + LINE_DRAWING_HOST_MENU_CARD_PADDING,
        out_layout->card_rect.y + LINE_DRAWING_HOST_MENU_CARD_PADDING,
        out_layout->card_rect.w - (LINE_DRAWING_HOST_MENU_CARD_PADDING * 2),
        LINE_DRAWING_HOST_MENU_HEADER_HEIGHT
    };

    out_layout->footer_rect = (SDL_Rect){
        out_layout->card_rect.x + LINE_DRAWING_HOST_MENU_CARD_PADDING,
        out_layout->card_rect.y + out_layout->card_rect.h - LINE_DRAWING_HOST_MENU_CARD_PADDING -
            LINE_DRAWING_HOST_MENU_FOOTER_HEIGHT,
        out_layout->card_rect.w - (LINE_DRAWING_HOST_MENU_CARD_PADDING * 2),
        LINE_DRAWING_HOST_MENU_FOOTER_HEIGHT
    };

    out_layout->body_rect = (SDL_Rect){
        out_layout->header_rect.x,
        out_layout->header_rect.y + out_layout->header_rect.h + LINE_DRAWING_HOST_MENU_REGION_GAP,
        out_layout->header_rect.w,
        out_layout->footer_rect.y - (out_layout->header_rect.y + out_layout->header_rect.h) -
            (LINE_DRAWING_HOST_MENU_REGION_GAP * 2)
    };

    inner = out_layout->body_rect;
    if (right_width > inner.w / 3) {
        right_width = inner.w / 3;
    }
    if (right_width < 280) right_width = 280;

    out_layout->nav_rect = (SDL_Rect){
        inner.x,
        inner.y,
        LINE_DRAWING_HOST_MENU_NAV_WIDTH,
        inner.h
    };
    out_layout->detail_rect = (SDL_Rect){
        inner.x + inner.w - right_width,
        inner.y,
        right_width,
        inner.h
    };
    out_layout->list_rect = (SDL_Rect){
        out_layout->nav_rect.x + out_layout->nav_rect.w + LINE_DRAWING_HOST_MENU_REGION_GAP,
        inner.y,
        out_layout->detail_rect.x -
            (out_layout->nav_rect.x + out_layout->nav_rect.w + LINE_DRAWING_HOST_MENU_REGION_GAP) -
            LINE_DRAWING_HOST_MENU_REGION_GAP,
        inner.h
    };

    nav_y = out_layout->nav_rect.y + 12;
    for (i = 0; i < LINE_DRAWING_HOST_MENU_SECTION_COUNT; ++i) {
        out_layout->nav_rows[i] = (SDL_Rect){
            out_layout->nav_rect.x + 10,
            nav_y,
            out_layout->nav_rect.w - 20,
            LINE_DRAWING_HOST_MENU_NAV_ROW_HEIGHT
        };
        nav_y += LINE_DRAWING_HOST_MENU_NAV_ROW_HEIGHT + 8;
    }

    out_layout->list_header_rect = (SDL_Rect){
        out_layout->list_rect.x + 12,
        out_layout->list_rect.y + 12,
        out_layout->list_rect.w - 24,
        LINE_DRAWING_HOST_MENU_LIST_HEADER_HEIGHT
    };
    out_layout->filter_rect = (SDL_Rect){
        out_layout->list_header_rect.x,
        out_layout->list_header_rect.y + 40,
        out_layout->list_header_rect.w,
        30
    };
    out_layout->browse_action_rects[LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT] = (SDL_Rect){
        out_layout->list_header_rect.x,
        out_layout->list_header_rect.y + 40,
        (out_layout->list_header_rect.w - 10) / 2,
        30
    };
    out_layout->browse_action_rects[LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_OUTPUT] = (SDL_Rect){
        out_layout->browse_action_rects[LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT].x +
            out_layout->browse_action_rects[LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT].w + 10,
        out_layout->list_header_rect.y + 40,
        out_layout->list_header_rect.w -
            out_layout->browse_action_rects[LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT].w - 10,
        30
    };
    out_layout->list_scrollbar_rect = (SDL_Rect){
        out_layout->list_rect.x + out_layout->list_rect.w - 12 - LINE_DRAWING_HOST_MENU_SCROLLBAR_W,
        out_layout->list_header_rect.y + out_layout->list_header_rect.h + 8,
        LINE_DRAWING_HOST_MENU_SCROLLBAR_W,
        out_layout->list_rect.h - (out_layout->list_header_rect.h + 22)
    };
    out_layout->list_view_rect = (SDL_Rect){
        out_layout->list_rect.x + 12,
        out_layout->list_header_rect.y + out_layout->list_header_rect.h + 8,
        out_layout->list_rect.w - 24 - LINE_DRAWING_HOST_MENU_SCROLLBAR_W -
            LINE_DRAWING_HOST_MENU_SCROLLBAR_GUTTER,
        out_layout->list_rect.h - (out_layout->list_header_rect.h + 22)
    };

    out_layout->detail_preview_rect = (SDL_Rect){
        out_layout->detail_rect.x + 14,
        out_layout->detail_rect.y + 94,
        out_layout->detail_rect.w - 28,
        LINE_DRAWING_HOST_MENU_PREVIEW_HEIGHT
    };

    out_layout->status_rect = (SDL_Rect){
        out_layout->footer_rect.x + 12,
        out_layout->footer_rect.y + 10,
        out_layout->footer_rect.w - 24,
        18
    };
    out_layout->keyboard_rect = (SDL_Rect){
        out_layout->footer_rect.x + 12,
        out_layout->status_rect.y + out_layout->status_rect.h + 8,
        out_layout->footer_rect.w - 24,
        18
    };
    out_layout->input_root_rect = (SDL_Rect){
        out_layout->footer_rect.x + 12,
        out_layout->keyboard_rect.y + out_layout->keyboard_rect.h + 10,
        out_layout->footer_rect.w - 24,
        18
    };
    out_layout->output_root_rect = (SDL_Rect){
        out_layout->footer_rect.x + 12,
        out_layout->input_root_rect.y + out_layout->input_root_rect.h + 6,
        out_layout->footer_rect.w - 24,
        18
    };
}

static int line_drawing_host_menu_hit_test_nav(const LineDrawingHostMenuLayout* layout,
                                               int mouse_x,
                                               int mouse_y) {
    SDL_Point point = {mouse_x, mouse_y};
    int i = 0;
    if (!layout) return -1;
    for (i = 0; i < LINE_DRAWING_HOST_MENU_SECTION_COUNT; ++i) {
        if (SDL_PointInRect(&point, &layout->nav_rows[i])) {
            return i;
        }
    }
    return -1;
}

static bool line_drawing_host_menu_hit_test_filter(const LineDrawingHostMenuState* state,
                                                   const LineDrawingHostMenuLayout* layout,
                                                   int mouse_x,
                                                   int mouse_y) {
    SDL_Point point = {mouse_x, mouse_y};
    if (!state || !layout) return false;
    if (!line_drawing_host_menu_section_supports_filter(state->selected_section)) return false;
    return SDL_PointInRect(&point, &layout->filter_rect);
}

static int line_drawing_host_menu_hit_test_browse_action(const LineDrawingHostMenuState* state,
                                                         const LineDrawingHostMenuLayout* layout,
                                                         int mouse_x,
                                                         int mouse_y) {
    SDL_Point point = {mouse_x, mouse_y};
    int i = 0;
    if (!state || !layout) return -1;
    if (state->selected_section != LINE_DRAWING_HOST_MENU_SECTION_BROWSE) return -1;
    for (i = 0; i < LINE_DRAWING_HOST_MENU_BROWSE_ACTION_COUNT; ++i) {
        if (SDL_PointInRect(&point, &layout->browse_action_rects[i])) {
            return i;
        }
    }
    return -1;
}

static float line_drawing_host_menu_list_max_scroll(const LineDrawingHostMenuState* state,
                                                    const LineDrawingHostMenuLayout* layout,
                                                    LineDrawingHostMenuSection section) {
    float content_height = 0.0f;
    int count = line_drawing_host_menu_content_count(state, section);
    int row_height = line_drawing_host_menu_content_row_height(section);
    if (!state || !layout || count <= 0) return 0.0f;
    content_height = (float)((row_height + LINE_DRAWING_HOST_MENU_ROW_GAP) * count);
    content_height -= (float)LINE_DRAWING_HOST_MENU_ROW_GAP;
    if (content_height <= (float)layout->list_view_rect.h) return 0.0f;
    return content_height - (float)layout->list_view_rect.h;
}

static void line_drawing_host_menu_clamp_scroll(LineDrawingHostMenuState* state,
                                                const LineDrawingHostMenuLayout* layout,
                                                LineDrawingHostMenuSection section) {
    float* scroll = line_drawing_host_menu_scroll_ptr(state, section);
    float max_scroll = line_drawing_host_menu_list_max_scroll(state, layout, section);
    if (!scroll) return;
    if (*scroll < 0.0f) *scroll = 0.0f;
    if (*scroll > max_scroll) *scroll = max_scroll;
}

static bool line_drawing_host_menu_has_scrollbar(const LineDrawingHostMenuState* state,
                                                 const LineDrawingHostMenuLayout* layout,
                                                 LineDrawingHostMenuSection section) {
    return line_drawing_host_menu_list_max_scroll(state, layout, section) > 0.5f;
}

static SDL_Rect line_drawing_host_menu_scrollbar_thumb_rect(const LineDrawingHostMenuState* state,
                                                            const LineDrawingHostMenuLayout* layout,
                                                            LineDrawingHostMenuSection section) {
    SDL_Rect thumb = {0, 0, 0, 0};
    float* scroll = NULL;
    float max_scroll = 0.0f;
    float content_height = 0.0f;
    float ratio = 1.0f;
    float thumb_h = 0.0f;
    float travel = 0.0f;
    float offset_ratio = 0.0f;
    int count = 0;
    int row_height = 0;

    if (!state || !layout) return thumb;
    if (!line_drawing_host_menu_has_scrollbar(state, layout, section)) return thumb;

    count = line_drawing_host_menu_content_count(state, section);
    row_height = line_drawing_host_menu_content_row_height(section);
    if (count <= 0 || row_height <= 0) return thumb;

    content_height = (float)((row_height + LINE_DRAWING_HOST_MENU_ROW_GAP) * count) -
                     (float)LINE_DRAWING_HOST_MENU_ROW_GAP;
    if (content_height <= 0.0f || layout->list_scrollbar_rect.h <= 0) return thumb;

    thumb = layout->list_scrollbar_rect;
    ratio = (float)layout->list_scrollbar_rect.h / content_height;
    if (ratio > 1.0f) ratio = 1.0f;
    thumb_h = ratio * (float)layout->list_scrollbar_rect.h;
    if (thumb_h < (float)LINE_DRAWING_HOST_MENU_SCROLLBAR_MIN_THUMB_H) {
        thumb_h = (float)LINE_DRAWING_HOST_MENU_SCROLLBAR_MIN_THUMB_H;
    }
    if (thumb_h > (float)layout->list_scrollbar_rect.h) {
        thumb_h = (float)layout->list_scrollbar_rect.h;
    }
    thumb.h = (int)thumb_h;

    max_scroll = line_drawing_host_menu_list_max_scroll(state, layout, section);
    travel = (float)layout->list_scrollbar_rect.h - thumb_h;
    if (travel <= 0.0f || max_scroll <= 0.0f) return thumb;

    scroll = line_drawing_host_menu_scroll_ptr((LineDrawingHostMenuState*)state, section);
    if (!scroll) return thumb;
    offset_ratio = *scroll / max_scroll;
    if (offset_ratio < 0.0f) offset_ratio = 0.0f;
    if (offset_ratio > 1.0f) offset_ratio = 1.0f;
    thumb.y = layout->list_scrollbar_rect.y + (int)(offset_ratio * travel);
    return thumb;
}

static bool line_drawing_host_menu_scrollbar_drag_to(LineDrawingHostMenuState* state,
                                                     const LineDrawingHostMenuLayout* layout,
                                                     LineDrawingHostMenuSection section,
                                                     int mouse_y) {
    SDL_Rect thumb = {0, 0, 0, 0};
    float* scroll = NULL;
    float max_scroll = 0.0f;
    float usable_h = 0.0f;
    float delta = 0.0f;

    if (!state || !layout) return false;
    scroll = line_drawing_host_menu_scroll_ptr(state, section);
    if (!scroll) return false;
    if (!line_drawing_host_menu_has_scrollbar(state, layout, section)) return false;

    thumb = line_drawing_host_menu_scrollbar_thumb_rect(state, layout, section);
    if (thumb.h <= 0) return false;

    max_scroll = line_drawing_host_menu_list_max_scroll(state, layout, section);
    usable_h = (float)(layout->list_scrollbar_rect.h - thumb.h);
    if (usable_h <= 0.0f || max_scroll <= 0.0f) return false;

    delta = (float)(mouse_y - state->scrollbar_drag_start_y);
    *scroll = state->scrollbar_drag_start_offset_px + ((delta / usable_h) * max_scroll);
    line_drawing_host_menu_clamp_scroll(state, layout, section);
    return true;
}

static bool line_drawing_host_menu_jump_scrollbar_to(LineDrawingHostMenuState* state,
                                                     const LineDrawingHostMenuLayout* layout,
                                                     LineDrawingHostMenuSection section,
                                                     int mouse_y) {
    SDL_Rect thumb = {0, 0, 0, 0};
    float* scroll = NULL;
    float max_scroll = 0.0f;
    float usable_h = 0.0f;
    float ratio = 0.0f;
    int rel_y = 0;

    if (!state || !layout) return false;
    scroll = line_drawing_host_menu_scroll_ptr(state, section);
    if (!scroll) return false;
    if (!line_drawing_host_menu_has_scrollbar(state, layout, section)) return false;

    thumb = line_drawing_host_menu_scrollbar_thumb_rect(state, layout, section);
    if (thumb.h <= 0) return false;
    max_scroll = line_drawing_host_menu_list_max_scroll(state, layout, section);
    usable_h = (float)(layout->list_scrollbar_rect.h - thumb.h);
    if (usable_h <= 0.0f || max_scroll <= 0.0f) return false;

    rel_y = mouse_y - layout->list_scrollbar_rect.y - (thumb.h / 2);
    ratio = (float)rel_y / usable_h;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    *scroll = ratio * max_scroll;
    line_drawing_host_menu_clamp_scroll(state, layout, section);
    return true;
}

static bool line_drawing_host_menu_handle_root_folder_shortcut(LineDrawingHostMenuState* state,
                                                               bool output_root) {
    bool changed = false;
    if (!state) return false;

    changed = output_root ? UIPanel_OpenOutputRootFolderDialog()
                          : UIPanel_OpenInputRootFolderDialog();
    if (changed) {
        snprintf(state->browser.current_path,
                 sizeof(state->browser.current_path),
                 "%s",
                 Global_GetInputRoot() ? Global_GetInputRoot() : "");
        line_drawing_host_menu_refresh_catalog(state);
        line_drawing_host_menu_set_status(state,
                                          output_root
                                              ? "Output root updated from native folder picker."
                                              : "Input root updated from native folder picker.",
                                          false);
    } else {
        line_drawing_host_menu_set_status(state,
                                          output_root
                                              ? "Output root unchanged."
                                              : "Input root unchanged.",
                                          false);
    }
    return true;
}

static void line_drawing_host_menu_ensure_selected_visible(LineDrawingHostMenuState* state,
                                                           const LineDrawingHostMenuLayout* layout,
                                                           LineDrawingHostMenuSection section) {
    float* scroll = line_drawing_host_menu_scroll_ptr(state, section);
    int* selected_ptr = line_drawing_host_menu_selected_content_index_ptr(state, section);
    int row_height = line_drawing_host_menu_content_row_height(section) + LINE_DRAWING_HOST_MENU_ROW_GAP;
    float top = 0.0f;
    float bottom = 0.0f;
    if (!state || !layout || !scroll || !selected_ptr || *selected_ptr < 0) return;
    top = (float)(*selected_ptr * row_height);
    bottom = top + (float)(row_height - LINE_DRAWING_HOST_MENU_ROW_GAP);
    if (top < *scroll) {
        *scroll = top;
    } else if (bottom > *scroll + (float)layout->list_view_rect.h) {
        *scroll = bottom - (float)layout->list_view_rect.h;
    }
    line_drawing_host_menu_clamp_scroll(state, layout, section);
}

static int line_drawing_host_menu_hit_test_content(const LineDrawingHostMenuState* state,
                                                   const LineDrawingHostMenuLayout* layout,
                                                   int mouse_x,
                                                   int mouse_y) {
    SDL_Point point = {mouse_x, mouse_y};
    float scroll = 0.0f;
    int count = 0;
    int row_height = 0;
    int content_y = 0;
    if (!state || !layout) return -1;
    if (!SDL_PointInRect(&point, &layout->list_view_rect)) return -1;
    if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_RECENTS) {
        scroll = state->recent_scroll_px;
    } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS) {
        scroll = state->layout_scroll_px;
    } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
        scroll = state->scene_scroll_px;
    } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_BROWSE) {
        scroll = state->browser_scroll_px;
    }
    count = line_drawing_host_menu_content_count(state, state->selected_section);
    row_height = line_drawing_host_menu_content_row_height(state->selected_section) +
                 LINE_DRAWING_HOST_MENU_ROW_GAP;
    if (count <= 0 || row_height <= 0) return -1;
    content_y = (mouse_y - layout->list_view_rect.y) + (int)scroll;
    if (content_y < 0) return -1;
    {
        int index = content_y / row_height;
        if (index < 0 || index >= count) return -1;
        return index;
    }
}

static bool line_drawing_host_menu_hit_test_scrollbar_track(const LineDrawingHostMenuState* state,
                                                            const LineDrawingHostMenuLayout* layout,
                                                            int mouse_x,
                                                            int mouse_y) {
    SDL_Point point = {mouse_x, mouse_y};
    if (!state || !layout) return false;
    if (!line_drawing_host_menu_has_scrollbar(state, layout, state->selected_section)) return false;
    return SDL_PointInRect(&point, &layout->list_scrollbar_rect);
}

static bool line_drawing_host_menu_activate_browse_action(LineDrawingHostMenuState* state,
                                                          int action_index) {
    if (!state) return false;
    switch ((LineDrawingHostMenuBrowseAction)action_index) {
        case LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT:
            return line_drawing_host_menu_handle_root_folder_shortcut(state, false);
        case LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_OUTPUT:
            return line_drawing_host_menu_handle_root_folder_shortcut(state, true);
        default:
            return false;
    }
}

static void line_drawing_host_menu_enter_catalog_for_current_root(
    LineDrawingHostMenuState* state,
    LineDrawingCatalogPreviewSourceKind preferred_kind) {
    if (!state) return;
    if (preferred_kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE &&
        state->catalog.scene_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_SCENES;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_scene_index < 0) {
            state->selected_scene_index = 0;
        }
        return;
    }
    if (preferred_kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT &&
        state->catalog.layout_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_layout_index < 0) {
            state->selected_layout_index = 0;
        }
        return;
    }
    if (state->catalog.scene_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_SCENES;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_scene_index < 0) {
            state->selected_scene_index = 0;
        }
        return;
    }
    if (state->catalog.layout_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_layout_index < 0) {
            state->selected_layout_index = 0;
        }
        return;
    }
    if (state->browser.entry_count > 0 && state->selected_browser_index < 0) {
        state->selected_browser_index = 0;
    }
}

static bool line_drawing_host_menu_activate_browser_entry(LineDrawingHostMenuState* state) {
    const LineDrawingRootBrowserEntry* entry = line_drawing_host_menu_selected_browser_entry(state);
    LineDrawingCatalogPreviewSourceKind preferred_kind =
        LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
    if (!state || !entry || !entry->enabled) return false;
    preferred_kind = entry->preview_kind;

    switch (entry->kind) {
        case LINE_DRAWING_ROOT_BROWSER_ENTRY_NEARBY_INPUT_ROOT:
            if (Global_SetInputRoot(entry->path, true)) {
                snprintf(state->browser.current_path, sizeof(state->browser.current_path), "%s", entry->path);
                line_drawing_host_menu_refresh_catalog(state);
                line_drawing_host_menu_enter_catalog_for_current_root(state, preferred_kind);
                line_drawing_host_menu_set_status(state,
                                                  "Input root switched and opened the catalog for the nearby directory.",
                                                  false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to update input root.", true);
            }
            return true;
        default:
            return false;
    }
}

static bool line_drawing_host_menu_activate_recent_entry(LineDrawingHostMenuState* state,
                                                         LineDrawingHostMenuCommand* out_command) {
    const LineDrawingRecentMenuEntry* entry = line_drawing_host_menu_selected_recent_entry(state);
    if (!state || !entry || !out_command) return false;

    switch (entry->kind) {
        case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT:
            if (UIPanel_LoadLayoutFromPath(entry->path)) {
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened recent layout.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open recent layout.", true);
            }
            return true;
        case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE:
            if (UIPanel_LoadSceneFromPath(entry->path)) {
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened recent scene.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open recent scene.", true);
            }
            return true;
        case LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT:
            if (Global_SetInputRoot(entry->path, true)) {
                snprintf(state->browser.current_path, sizeof(state->browser.current_path), "%s", entry->path);
                line_drawing_host_menu_refresh_catalog(state);
                line_drawing_host_menu_set_status(state, "Switched to recent input root.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to switch input root.", true);
            }
            return true;
        case LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT:
            if (Global_SetOutputRoot(entry->path, true)) {
                snprintf(state->browser.current_path, sizeof(state->browser.current_path), "%s", entry->path);
                line_drawing_host_menu_refresh_catalog(state);
                line_drawing_host_menu_set_status(state, "Switched to recent output root.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to switch output root.", true);
            }
            return true;
        default:
            return false;
    }
}

static bool line_drawing_host_menu_activate(LineDrawingHostMenuState* state,
                                            const LineDrawingHostMenuModel* model,
                                            LineDrawingHostMenuCommand* out_command) {
    const char* current_layout = Global_GetLastLayoutPath();
    const char* current_scene = Global_GetLastSceneAuthoringPath();
    const LineDrawingSceneCatalogEntry* entry = NULL;

    if (!state || !model || !out_command) return false;
    memset(out_command, 0, sizeof(*out_command));

    switch (state->selected_section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            if (state->selected_index < 0 || state->selected_index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT) {
                return false;
            }
            if (!model->item_enabled[state->selected_index]) {
                line_drawing_host_menu_set_status(state,
                                                  "Selection is unavailable for the current session.",
                                                  true);
                return true;
            }
            switch ((LineDrawingHostMenuItemId)state->selected_index) {
                case LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR:
                    line_drawing_host_menu_stop_filter_editing(state);
                    out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                    line_drawing_host_menu_set_status(state, "Opening editor.", false);
                    return true;
                case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT:
                    if (UIPanel_LoadLayoutFromPath(current_layout)) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                        line_drawing_host_menu_set_status(state, "Reopened last layout.", false);
                    } else {
                        line_drawing_host_menu_set_status(state, "Failed to reopen last layout.", true);
                    }
                    return true;
                case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE:
                    if (UIPanel_LoadSceneFromPath(current_scene)) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                        line_drawing_host_menu_set_status(state, "Reopened last scene.", false);
                    } else {
                        line_drawing_host_menu_set_status(state, "Failed to reopen last scene.", true);
                    }
                    return true;
                case LINE_DRAWING_HOST_MENU_ITEM_QUIT:
                    out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_QUIT;
                    return true;
                default:
                    break;
            }
            return false;
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            if (!line_drawing_host_menu_activate_recent_entry(state, out_command)) {
                line_drawing_host_menu_set_status(state, "No recent context is available to activate.", true);
            }
            return true;
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            entry = line_drawing_host_menu_selected_catalog_entry(state,
                                                                  LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS,
                                                                  NULL);
            if (!entry) {
                line_drawing_host_menu_set_status(state, "No layout entry is available to open.", true);
                return true;
            }
            if (UIPanel_LoadLayoutFromPath(entry->path)) {
                line_drawing_host_menu_stop_filter_editing(state);
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened layout from catalog.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open selected layout.", true);
            }
            return true;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            entry = line_drawing_host_menu_selected_catalog_entry(state,
                                                                  LINE_DRAWING_HOST_MENU_SECTION_SCENES,
                                                                  NULL);
            if (!entry) {
                line_drawing_host_menu_set_status(state, "No scene entry is available to open.", true);
                return true;
            }
            if (UIPanel_LoadSceneFromPath(entry->path)) {
                line_drawing_host_menu_stop_filter_editing(state);
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened scene from catalog.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open selected scene.", true);
            }
            return true;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            if (!line_drawing_host_menu_activate_browser_entry(state)) {
                line_drawing_host_menu_set_status(state, "No browser entry is available to activate.", true);
            }
            return true;
        default:
            return false;
    }
}

void LineDrawingHostMenu_Init(LineDrawingHostMenuState* state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->selected_index = LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR;
    state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS;
    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
    state->hovered_section_index = -1;
    state->hovered_content_index = -1;
    state->hovered_browse_action_index = -1;
    state->scrollbar_dragging = false;
    state->selected_recent_index = -1;
    state->selected_layout_index = -1;
    state->selected_scene_index = -1;
    state->selected_browser_index = 0;
    LineDrawingRecentMenuList_Init(&state->recent_entries);
    LineDrawingSceneCatalog_Init(&state->catalog);
    LineDrawingRootBrowser_Init(&state->browser);
    LineDrawingCatalogPreviewCache_Init(&state->preview_cache);
    snprintf(state->status_text,
             sizeof(state->status_text),
             "Phase 2 menu work: recents, catalog, and root-context controls are active.");
}

void LineDrawingHostMenu_BuildModel(LineDrawingHostMenuModel* out_model) {
    const char* current_layout = Global_GetLastLayoutPath();
    const char* current_scene = Global_GetLastSceneAuthoringPath();
    if (!out_model) return;
    memset(out_model, 0, sizeof(*out_model));
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR] = true;
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT] =
        line_drawing_host_menu_path_exists(current_layout);
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE] =
        line_drawing_host_menu_path_exists(current_scene);
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_QUIT] = true;
}

int LineDrawingHostMenu_FirstSelectableIndex(const LineDrawingHostMenuModel* model) {
    int i = 0;
    if (!model) return -1;
    for (i = 0; i < LINE_DRAWING_HOST_MENU_ITEM_COUNT; ++i) {
        if (model->item_enabled[i]) return i;
    }
    return -1;
}

int LineDrawingHostMenu_MoveSelection(const LineDrawingHostMenuModel* model,
                                      int current_index,
                                      int direction) {
    int index = current_index;
    int steps = 0;
    if (!model) return current_index;
    if (direction == 0) return current_index;
    if (current_index < 0 || current_index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT) {
        return LineDrawingHostMenu_FirstSelectableIndex(model);
    }
    while (steps < LINE_DRAWING_HOST_MENU_ITEM_COUNT) {
        index += direction > 0 ? 1 : -1;
        if (index < 0) index = LINE_DRAWING_HOST_MENU_ITEM_COUNT - 1;
        if (index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT) index = 0;
        if (model->item_enabled[index]) return index;
        steps += 1;
    }
    return current_index;
}

bool LineDrawingHostMenu_HandleEvent(LineDrawingHostMenuState* state,
                                     AppContext* ctx,
                                     const SDL_Event* event,
                                     LineDrawingHostMenuCommand* out_command) {
    LineDrawingHostMenuModel model;
    LineDrawingHostMenuLayout layout;
    int hovered_index = -1;
    int* selected_ptr = NULL;
    int nav_index = -1;
    int browse_action_index = -1;
    bool filter_hit = false;
    int x = 0;
    int y = 0;
    (void)ctx;
    if (!state || !event || !out_command) return false;
    memset(out_command, 0, sizeof(*out_command));

    line_drawing_host_menu_refresh_catalog(state);
    LineDrawingHostMenu_BuildModel(&model);
    line_drawing_host_menu_layout(&layout,
                                  Global_GetScreenWidth(),
                                  Global_GetScreenHeight());
    line_drawing_host_menu_clear_hover(state);

    switch (event->type) {
        case SDL_KEYDOWN:
            line_drawing_host_menu_stop_scrollbar_drag(state);
            if ((SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) != 0) {
                if ((SDL_GetModState() & KMOD_SHIFT) != 0 && event->key.keysym.sym == SDLK_b) {
                    line_drawing_host_menu_stop_filter_editing(state);
                    line_drawing_host_menu_stop_scrollbar_drag(state);
                    return line_drawing_host_menu_handle_root_folder_shortcut(state, true);
                }
                if (event->key.keysym.sym == SDLK_b) {
                    line_drawing_host_menu_stop_filter_editing(state);
                    line_drawing_host_menu_stop_scrollbar_drag(state);
                    return line_drawing_host_menu_handle_root_folder_shortcut(state, false);
                }
            }
            switch (event->key.keysym.sym) {
                case SDLK_UP:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->selected_section =
                            (LineDrawingHostMenuSection)((state->selected_section +
                                                           LINE_DRAWING_HOST_MENU_SECTION_COUNT - 1) %
                                                          LINE_DRAWING_HOST_MENU_SECTION_COUNT);
                    } else {
                        selected_ptr =
                            line_drawing_host_menu_selected_content_index_ptr(state,
                                                                              state->selected_section);
                        if (selected_ptr) {
                            *selected_ptr = line_drawing_host_menu_move_content_selection(state,
                                                                                          &model,
                                                                                          state->selected_section,
                                                                                          *selected_ptr,
                                                                                          -1);
                            line_drawing_host_menu_ensure_selected_visible(state,
                                                                           &layout,
                                                                           state->selected_section);
                        }
                    }
                    return true;
                case SDLK_DOWN:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->selected_section =
                            (LineDrawingHostMenuSection)((state->selected_section + 1) %
                                                          LINE_DRAWING_HOST_MENU_SECTION_COUNT);
                    } else {
                        selected_ptr =
                            line_drawing_host_menu_selected_content_index_ptr(state,
                                                                              state->selected_section);
                        if (selected_ptr) {
                            *selected_ptr = line_drawing_host_menu_move_content_selection(state,
                                                                                          &model,
                                                                                          state->selected_section,
                                                                                          *selected_ptr,
                                                                                          1);
                            line_drawing_host_menu_ensure_selected_visible(state,
                                                                           &layout,
                                                                           state->selected_section);
                        }
                    }
                    return true;
                case SDLK_LEFT:
                    line_drawing_host_menu_stop_filter_editing(state);
                    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_NAV;
                    return true;
                case SDLK_RIGHT:
                    if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                        state->focus_region = (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV)
                                                  ? LINE_DRAWING_HOST_MENU_FOCUS_FILTER
                                                  : LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                    } else {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                    }
                    return true;
                case SDLK_TAB:
                    if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                        if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                            line_drawing_host_menu_begin_filter_editing(state);
                        } else if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                            line_drawing_host_menu_stop_filter_editing(state);
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        } else {
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_NAV;
                        }
                    } else {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->focus_region = (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV)
                                                  ? LINE_DRAWING_HOST_MENU_FOCUS_CONTENT
                                                  : LINE_DRAWING_HOST_MENU_FOCUS_NAV;
                    }
                    return true;
                case SDLK_SLASH:
                    if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                        line_drawing_host_menu_begin_filter_editing(state);
                        return true;
                    }
                    break;
                case SDLK_BACKSPACE:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                        size_t len = strlen(state->filter_query);
                        if (len > 0u) {
                            state->filter_query[len - 1u] = '\0';
                            line_drawing_host_menu_apply_filter(state);
                            line_drawing_host_menu_set_status(state, "Catalog filter updated.", false);
                        }
                        return true;
                    }
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                        if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                            line_drawing_host_menu_begin_filter_editing(state);
                        } else {
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        }
                        return true;
                    }
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        return true;
                    }
                    return line_drawing_host_menu_activate(state, &model, out_command);
                case SDLK_ESCAPE:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                        if (state->filter_query[0]) {
                            state->filter_query[0] = '\0';
                            line_drawing_host_menu_apply_filter(state);
                            line_drawing_host_menu_set_status(state, "Catalog filter cleared.", false);
                        } else {
                            line_drawing_host_menu_stop_filter_editing(state);
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        }
                        return true;
                    }
                    out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_QUIT;
                    return true;
                default:
                    break;
            }
            break;
        case SDL_TEXTINPUT:
            if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER &&
                line_drawing_host_menu_section_supports_filter(state->selected_section) &&
                event->text.text[0]) {
                size_t len = strlen(state->filter_query);
                size_t add_len = strlen(event->text.text);
                if (len + add_len >= sizeof(state->filter_query)) {
                    add_len = sizeof(state->filter_query) - len - 1u;
                }
                if (add_len > 0u) {
                    memcpy(state->filter_query + len, event->text.text, add_len);
                    state->filter_query[len + add_len] = '\0';
                    line_drawing_host_menu_apply_filter(state);
                    line_drawing_host_menu_set_status(state, "Catalog filter updated.", false);
                }
                return true;
            }
            break;
        case SDL_MOUSEMOTION:
            if (state->scrollbar_dragging) {
                return line_drawing_host_menu_scrollbar_drag_to(state,
                                                                &layout,
                                                                state->selected_section,
                                                                event->motion.y);
            }
            nav_index = line_drawing_host_menu_hit_test_nav(&layout,
                                                            event->motion.x,
                                                            event->motion.y);
            if (nav_index >= 0) {
                state->hovered_section_index = nav_index;
                return true;
            }
            filter_hit = line_drawing_host_menu_hit_test_filter(state,
                                                                &layout,
                                                                event->motion.x,
                                                                event->motion.y);
            if (filter_hit) {
                state->hovered_filter = true;
                return true;
            }
            browse_action_index = line_drawing_host_menu_hit_test_browse_action(state,
                                                                                &layout,
                                                                                event->motion.x,
                                                                                event->motion.y);
            if (browse_action_index >= 0) {
                state->hovered_browse_action_index = browse_action_index;
                return true;
            }
            hovered_index = line_drawing_host_menu_hit_test_content(state,
                                                                    &layout,
                                                                    event->motion.x,
                                                                    event->motion.y);
            if (hovered_index >= 0) {
                state->hovered_content_index = hovered_index;
                return true;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button != SDL_BUTTON_LEFT) break;
            nav_index = line_drawing_host_menu_hit_test_nav(&layout,
                                                            event->button.x,
                                                            event->button.y);
            if (nav_index >= 0) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                state->selected_section = (LineDrawingHostMenuSection)nav_index;
                state->hovered_section_index = nav_index;
                if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                    line_drawing_host_menu_begin_filter_editing(state);
                } else {
                    line_drawing_host_menu_stop_filter_editing(state);
                    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                }
                return true;
            }
            filter_hit = line_drawing_host_menu_hit_test_filter(state,
                                                                &layout,
                                                                event->button.x,
                                                                event->button.y);
            if (filter_hit) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                state->hovered_filter = true;
                line_drawing_host_menu_begin_filter_editing(state);
                return true;
            }
            browse_action_index = line_drawing_host_menu_hit_test_browse_action(state,
                                                                                &layout,
                                                                                event->button.x,
                                                                                event->button.y);
            if (browse_action_index >= 0) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                line_drawing_host_menu_stop_filter_editing(state);
                state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                state->hovered_browse_action_index = browse_action_index;
                return line_drawing_host_menu_activate_browse_action(state, browse_action_index);
            }
            if (line_drawing_host_menu_hit_test_scrollbar_track(state,
                                                                &layout,
                                                                event->button.x,
                                                                event->button.y)) {
                SDL_Rect thumb = line_drawing_host_menu_scrollbar_thumb_rect(state,
                                                                             &layout,
                                                                             state->selected_section);
                line_drawing_host_menu_stop_filter_editing(state);
                state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                if (SDL_PointInRect(&(SDL_Point){event->button.x, event->button.y}, &thumb)) {
                    state->scrollbar_dragging = true;
                    state->scrollbar_drag_start_y = event->button.y;
                    {
                        float* scroll =
                            line_drawing_host_menu_scroll_ptr(state, state->selected_section);
                        state->scrollbar_drag_start_offset_px = scroll ? *scroll : 0.0f;
                    }
                } else {
                    (void)line_drawing_host_menu_jump_scrollbar_to(state,
                                                                   &layout,
                                                                   state->selected_section,
                                                                   event->button.y);
                }
                return true;
            }
            hovered_index = line_drawing_host_menu_hit_test_content(state,
                                                                    &layout,
                                                                    event->button.x,
                                                                    event->button.y);
            if (hovered_index >= 0) {
                selected_ptr =
                    line_drawing_host_menu_selected_content_index_ptr(state,
                                                                      state->selected_section);
                if (selected_ptr) {
                    *selected_ptr = hovered_index;
                }
                state->hovered_content_index = hovered_index;
                line_drawing_host_menu_stop_filter_editing(state);
                state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                return line_drawing_host_menu_activate(state, &model, out_command);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT && state->scrollbar_dragging) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                return true;
            }
            break;
        case SDL_MOUSEWHEEL:
            SDL_GetMouseState(&x, &y);
            if (SDL_PointInRect(&(SDL_Point){x, y}, &layout.list_view_rect) ||
                SDL_PointInRect(&(SDL_Point){x, y}, &layout.list_scrollbar_rect)) {
                float* scroll = line_drawing_host_menu_scroll_ptr(state, state->selected_section);
                if (scroll) {
                    *scroll -= (float)event->wheel.y *
                               (float)(line_drawing_host_menu_content_row_height(state->selected_section) * 3);
                    line_drawing_host_menu_clamp_scroll(state, &layout, state->selected_section);
                    return true;
                }
            }
            break;
        default:
            break;
    }

    return false;
}

static void line_drawing_host_menu_draw_section_nav(SDL_Renderer* renderer,
                                                    TTF_Font* font,
                                                    const LineDrawingHostMenuState* state,
                                                    const LineDrawingHostMenuLayout* layout,
                                                    SDL_Color panel_fill,
                                                    SDL_Color border_color,
                                                    SDL_Color title_color,
                                                    SDL_Color text_color,
                                                    SDL_Color muted_color,
                                                    SDL_Color highlight_color,
                                                    SDL_Color shadow_color) {
    int i = 0;
    if (!renderer || !font || !state || !layout) return;
    line_drawing_host_menu_draw_panel(renderer,
                                      layout->nav_rect,
                                      panel_fill,
                                      border_color,
                                      shadow_color);
    for (i = 0; i < LINE_DRAWING_HOST_MENU_SECTION_COUNT; ++i) {
        SDL_Rect rect = layout->nav_rows[i];
        SDL_Color fill = line_drawing_host_menu_dim(panel_fill, 225);
        SDL_Color text = text_color;
        SDL_Color border = border_color;
        bool selected = ((int)state->selected_section == i);
        bool hovered = (state->hovered_section_index == i);
        char count_text[32];
        if (selected) {
            fill = highlight_color;
            text = title_color;
            border = (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV)
                         ? title_color
                         : line_drawing_host_menu_mix(title_color, border_color, 0.4f);
        } else if (hovered) {
            fill = line_drawing_host_menu_mix(panel_fill, highlight_color, 0.18f);
            border = line_drawing_host_menu_mix(border_color, title_color, 0.2f);
        }
        line_drawing_host_menu_draw_panel(renderer, rect, fill, border, shadow_color);
        if (selected || hovered) {
            line_drawing_host_menu_draw_accent_bar(renderer,
                                                   rect,
                                                   selected ? title_color
                                                            : line_drawing_host_menu_mix(title_color,
                                                                                         highlight_color,
                                                                                         0.45f));
        }

        snprintf(count_text,
                 sizeof(count_text),
                 "%d",
                 line_drawing_host_menu_section_count_value(state,
                                                            (LineDrawingHostMenuSection)i));
        line_drawing_host_menu_draw_text(renderer,
                                         font,
                                         line_drawing_host_menu_section_label((LineDrawingHostMenuSection)i),
                                         rect.x + 18,
                                         rect.y + 12,
                                         text);
        line_drawing_host_menu_draw_badge(renderer,
                                          font,
                                          count_text,
                                          (SDL_Rect){rect.x + rect.w - 54, rect.y + 8, 44, 24},
                                          line_drawing_host_menu_mix(fill, highlight_color, selected ? 0.25f : 0.12f),
                                          border,
                                          line_drawing_host_menu_dim(text, 245));
    }
    line_drawing_host_menu_draw_text(renderer,
                                     font,
                                     "Sections",
                                     layout->nav_rect.x + 12,
                                     layout->nav_rows[LINE_DRAWING_HOST_MENU_SECTION_COUNT - 1].y +
                                         LINE_DRAWING_HOST_MENU_NAV_ROW_HEIGHT + 24,
                                     muted_color);
}

static void line_drawing_host_menu_draw_quick_action_row(SDL_Renderer* renderer,
                                                         TTF_Font* font,
                                                         const LineDrawingHostMenuModel* model,
                                                         SDL_Rect rect,
                                                         int item_index,
                                                         bool selected,
                                                         SDL_Color fill_color,
                                                         SDL_Color border_color,
                                                         SDL_Color text_color,
                                                         SDL_Color muted_color) {
    SDL_Color text = text_color;
    SDL_Color muted = muted_color;
    const char* action_badge = NULL;
    if (!model->item_enabled[item_index]) {
        text = line_drawing_host_menu_dim(text, 215);
        muted = line_drawing_host_menu_dim(muted, 215);
    }
    line_drawing_host_menu_draw_panel(renderer,
                                      rect,
                                      fill_color,
                                      border_color,
                                      line_drawing_host_menu_dim(border_color, 32));
    if (selected) {
        line_drawing_host_menu_draw_accent_bar(renderer, rect, text);
    }
    action_badge = (item_index == LINE_DRAWING_HOST_MENU_ITEM_QUIT) ? "Exit" : "Open";
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             line_drawing_host_menu_item_label((LineDrawingHostMenuItemId)item_index),
                                             rect.x + 18,
                                             rect.y + 10,
                                             rect.w - 88,
                                             text);
    line_drawing_host_menu_draw_badge(renderer,
                                      font,
                                      action_badge,
                                      (SDL_Rect){rect.x + rect.w - 62, rect.y + 10, 52, 22},
                                      line_drawing_host_menu_mix(fill_color, border_color, 0.2f),
                                      border_color,
                                      muted);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             line_drawing_host_menu_item_description((LineDrawingHostMenuItemId)item_index),
                                             rect.x + 18,
                                             rect.y + 32,
                                             rect.w - 36,
                                             muted);
    (void)selected;
}

static void line_drawing_host_menu_draw_catalog_row(SDL_Renderer* renderer,
                                                    TTF_Font* font,
                                                    SDL_Rect rect,
                                                    const LineDrawingSceneCatalogEntry* entry,
                                                    const LineDrawingCatalogPreviewData* preview,
                                                    bool selected,
                                                    bool active,
                                                    SDL_Color fill_color,
                                                    SDL_Color border_color,
                                                    SDL_Color text_color,
                                                    SDL_Color muted_color,
                                                    SDL_Color active_color) {
    int badge_width = active ? 74 : 0;
    SDL_Rect preview_rect;
    int text_x = rect.x + LINE_DRAWING_HOST_MENU_ROW_PREVIEW_WIDTH + 28;
    int text_width = rect.w - (text_x - rect.x) - 14 - badge_width;
    char summary[128];
    char extents[64];
    if (!entry) return;
    line_drawing_host_menu_draw_panel(renderer,
                                      rect,
                                      fill_color,
                                      border_color,
                                      line_drawing_host_menu_dim(border_color, 28));
    if (selected) {
        line_drawing_host_menu_draw_accent_bar(renderer, rect, text_color);
    }

    preview_rect = (SDL_Rect){
        rect.x + 10,
        rect.y + 8,
        LINE_DRAWING_HOST_MENU_ROW_PREVIEW_WIDTH,
        rect.h - 16
    };
    line_drawing_host_menu_draw_preview(renderer,
                                        preview_rect,
                                        preview,
                                        line_drawing_host_menu_dim(fill_color, 255),
                                        border_color,
                                        text_color,
                                        muted_color);

    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             entry->label,
                                             text_x,
                                             rect.y + 8,
                                             text_width,
                                             text_color);
    line_drawing_host_menu_format_preview_summary(preview, summary, sizeof(summary));
    line_drawing_host_menu_format_preview_extents(preview, extents, sizeof(extents));
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             summary[0] ? summary : "Preview metadata unavailable",
                                             text_x,
                                             rect.y + 26,
                                             text_width,
                                             muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             extents[0] ? extents : line_drawing_host_menu_path_basename(entry->path),
                                             text_x,
                                             rect.y + 42,
                                             text_width,
                                             muted_color);
    if (active) {
        line_drawing_host_menu_draw_badge(renderer,
                                          font,
                                          "Current",
                                          (SDL_Rect){rect.x + rect.w - 74, rect.y + 10, 64, 22},
                                          line_drawing_host_menu_mix(fill_color, active_color, 0.15f),
                                          border_color,
                                          active_color);
    }
    (void)selected;
}

static void line_drawing_host_menu_draw_recent_row(SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   SDL_Rect rect,
                                                   const LineDrawingRecentMenuEntry* entry,
                                                   const LineDrawingCatalogPreviewData* preview,
                                                   SDL_Color fill_color,
                                                   SDL_Color border_color,
                                                   SDL_Color text_color,
                                                   SDL_Color muted_color,
                                                   SDL_Color active_color) {
    SDL_Rect preview_rect;
    int text_x = rect.x + LINE_DRAWING_HOST_MENU_ROW_PREVIEW_WIDTH + 24;
    int text_width = rect.w - (text_x - rect.x) - 14;
    char summary[128] = {0};
    char extents[64] = {0};
    const char* kind_badge = NULL;
    if (!renderer || !font || !entry) return;

    line_drawing_host_menu_draw_panel(renderer,
                                      rect,
                                      fill_color,
                                      border_color,
                                      line_drawing_host_menu_dim(border_color, 28));
    line_drawing_host_menu_draw_accent_bar(renderer,
                                           rect,
                                           entry->current ? active_color : border_color);

    preview_rect = (SDL_Rect){
        rect.x + 10,
        rect.y + 8,
        LINE_DRAWING_HOST_MENU_ROW_PREVIEW_WIDTH,
        rect.h - 16
    };
    line_drawing_host_menu_draw_preview(renderer,
                                        preview_rect,
                                        preview,
                                        line_drawing_host_menu_dim(fill_color, 255),
                                        border_color,
                                        text_color,
                                        muted_color);

    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             entry->label,
                                             text_x,
                                             rect.y + 8,
                                             text_width - 68,
                                             text_color);
    switch (entry->kind) {
        case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT: kind_badge = "Layout"; break;
        case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE: kind_badge = "Scene"; break;
        case LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT: kind_badge = "Input"; break;
        case LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT: kind_badge = "Output"; break;
        default: kind_badge = "Recent"; break;
    }
    line_drawing_host_menu_draw_badge(renderer,
                                      font,
                                      kind_badge,
                                      (SDL_Rect){rect.x + rect.w - 78, rect.y + 10, 68, 22},
                                      line_drawing_host_menu_mix(fill_color, border_color, 0.18f),
                                      border_color,
                                      muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             entry->description,
                                             text_x,
                                             rect.y + 26,
                                             text_width,
                                             muted_color);
    if (preview) {
        line_drawing_host_menu_format_preview_summary(preview, summary, sizeof(summary));
        line_drawing_host_menu_format_preview_extents(preview, extents, sizeof(extents));
    }
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             extents[0] ? extents : (summary[0] ? summary : entry->path),
                                             text_x,
                                             rect.y + 42,
                                             text_width - 12,
                                             muted_color);
    if (entry->current) {
        line_drawing_host_menu_draw_text_clipped(renderer,
                                                 font,
                                                 summary[0] ? summary : entry->path,
                                                 text_x,
                                                 rect.y + 58,
                                                 text_width - 12,
                                                 muted_color);
    }
}

static void line_drawing_host_menu_draw_browser_row(SDL_Renderer* renderer,
                                                    TTF_Font* font,
                                                    SDL_Rect rect,
                                                    const LineDrawingRootBrowserEntry* entry,
                                                    const LineDrawingCatalogPreviewData* preview,
                                                    SDL_Color fill_color,
                                                    SDL_Color border_color,
                                                    SDL_Color text_color,
                                                    SDL_Color muted_color,
                                                    SDL_Color active_color) {
    SDL_Rect preview_rect;
    int text_x = rect.x + LINE_DRAWING_HOST_MENU_ROW_PREVIEW_WIDTH + 24;
    int text_width = rect.w - (text_x - rect.x) - 14;
    char summary[128] = {0};
    char extents[64] = {0};
    if (!renderer || !font || !entry) return;

    line_drawing_host_menu_draw_panel(renderer,
                                      rect,
                                      fill_color,
                                      border_color,
                                      line_drawing_host_menu_dim(border_color, 28));
    line_drawing_host_menu_draw_accent_bar(renderer,
                                           rect,
                                           (entry->kind == LINE_DRAWING_ROOT_BROWSER_ENTRY_NEARBY_INPUT_ROOT)
                                               ? active_color
                                               : border_color);

    preview_rect = (SDL_Rect){
        rect.x + 10,
        rect.y + 8,
        LINE_DRAWING_HOST_MENU_ROW_PREVIEW_WIDTH,
        rect.h - 16
    };
    line_drawing_host_menu_draw_preview(renderer,
                                        preview_rect,
                                        preview,
                                        line_drawing_host_menu_dim(fill_color, 255),
                                        border_color,
                                        text_color,
                                        muted_color);

    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             entry->label,
                                             text_x,
                                             rect.y + 8,
                                             text_width - 68,
                                             text_color);
    line_drawing_host_menu_format_preview_summary(preview, summary, sizeof(summary));
    line_drawing_host_menu_format_preview_extents(preview, extents, sizeof(extents));
    line_drawing_host_menu_draw_badge(renderer,
                                      font,
                                      "Nearby",
                                      (SDL_Rect){rect.x + rect.w - 78, rect.y + 10, 68, 22},
                                      line_drawing_host_menu_mix(fill_color, border_color, 0.18f),
                                      border_color,
                                      muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             summary[0] ? summary : entry->description,
                                             text_x,
                                             rect.y + 28,
                                             text_width,
                                             muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             extents[0] ? extents : entry->description,
                                             text_x,
                                             rect.y + 44,
                                             text_width,
                                             muted_color);
}

static void line_drawing_host_menu_draw_browse_actions(SDL_Renderer* renderer,
                                                       TTF_Font* font,
                                                       const LineDrawingHostMenuState* state,
                                                       const LineDrawingHostMenuLayout* layout,
                                                       SDL_Color panel_fill,
                                                       SDL_Color border_color,
                                                       SDL_Color title_color,
                                                       SDL_Color text_color,
                                                       SDL_Color muted_color,
                                                       SDL_Color highlight_color) {
    int i = 0;
    if (!renderer || !font || !state || !layout) return;
    if (state->selected_section != LINE_DRAWING_HOST_MENU_SECTION_BROWSE) return;
    (void)muted_color;
    for (i = 0; i < LINE_DRAWING_HOST_MENU_BROWSE_ACTION_COUNT; ++i) {
        SDL_Rect rect = layout->browse_action_rects[i];
        SDL_Color fill = line_drawing_host_menu_mix(panel_fill, highlight_color, 0.14f);
        SDL_Color border = border_color;
        bool hovered = (state->hovered_browse_action_index == i);
        if (hovered) {
            fill = line_drawing_host_menu_mix(panel_fill, highlight_color, 0.24f);
            border = title_color;
        }
        line_drawing_host_menu_draw_panel(renderer,
                                          rect,
                                          fill,
                                          border,
                                          line_drawing_host_menu_dim(border_color, 24));
        if (hovered) {
            line_drawing_host_menu_draw_accent_bar(renderer, rect, title_color);
        }
        line_drawing_host_menu_draw_text_clipped(renderer,
                                                 font,
                                                 line_drawing_host_menu_browse_action_label(
                                                     (LineDrawingHostMenuBrowseAction)i),
                                                 rect.x + 10,
                                                 rect.y + 7,
                                                 rect.w - 20,
                                                 hovered ? title_color : text_color);
    }
}

static void line_drawing_host_menu_clear_hover(LineDrawingHostMenuState* state) {
    if (!state) return;
    state->hovered_section_index = -1;
    state->hovered_content_index = -1;
    state->hovered_browse_action_index = -1;
    state->hovered_filter = false;
}

static void line_drawing_host_menu_draw_filter(SDL_Renderer* renderer,
                                               TTF_Font* font,
                                               const LineDrawingHostMenuState* state,
                                               const LineDrawingHostMenuLayout* layout,
                                               SDL_Color panel_fill,
                                               SDL_Color border_color,
                                               SDL_Color title_color,
                                               SDL_Color text_color,
                                               SDL_Color muted_color,
                                               SDL_Color highlight_color) {
    SDL_Color fill = line_drawing_host_menu_mix(panel_fill, highlight_color, 0.16f);
    SDL_Color border = border_color;
    char filter_line[160];
    const char* placeholder = "Type to filter names and paths";
    if (!renderer || !font || !state || !layout) return;
    if (!line_drawing_host_menu_section_supports_filter(state->selected_section)) return;

    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
        fill = line_drawing_host_menu_mix(panel_fill, highlight_color, 0.28f);
        border = title_color;
    } else if (state->hovered_filter) {
        fill = line_drawing_host_menu_mix(panel_fill, highlight_color, 0.2f);
        border = line_drawing_host_menu_mix(border_color, title_color, 0.18f);
    }

    line_drawing_host_menu_draw_panel(renderer,
                                      layout->filter_rect,
                                      fill,
                                      border,
                                      line_drawing_host_menu_dim(border_color, 24));

    snprintf(filter_line,
             sizeof(filter_line),
             "Filter: %s%s",
             state->filter_query[0] ? state->filter_query : placeholder,
             state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER ? "_" : "");
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             font,
                                             filter_line,
                                             layout->filter_rect.x + 10,
                                             layout->filter_rect.y + 7,
                                             layout->filter_rect.w - 20,
                                             state->filter_query[0] ? text_color : muted_color);
}

static void line_drawing_host_menu_draw_content(SDL_Renderer* renderer,
                                                TTF_Font* body_font,
                                                LineDrawingHostMenuState* state,
                                                const LineDrawingHostMenuModel* model,
                                                const LineDrawingHostMenuLayout* layout,
                                                SDL_Color panel_fill,
                                                SDL_Color border_color,
                                                SDL_Color title_color,
                                                SDL_Color text_color,
                                                SDL_Color muted_color,
                                                SDL_Color highlight_color,
                                                SDL_Color active_color) {
    SDL_Rect clip = layout->list_view_rect;
    SDL_Rect scrollbar_thumb = {0, 0, 0, 0};
    float scroll = 0.0f;
    int count = line_drawing_host_menu_content_count(state, state->selected_section);
    int row_height = line_drawing_host_menu_content_row_height(state->selected_section);
    int y = 0;
    int i = 0;
    if (!renderer || !body_font || !state || !model || !layout) return;

    line_drawing_host_menu_draw_panel(renderer,
                                      layout->list_rect,
                                      panel_fill,
                                      border_color,
                                      line_drawing_host_menu_dim(border_color, 38));

    line_drawing_host_menu_draw_text(renderer,
                                     body_font,
                                     line_drawing_host_menu_section_label(state->selected_section),
                                     layout->list_header_rect.x,
                                     layout->list_header_rect.y,
                                     title_color);
    {
        char summary[192];
        if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_BROWSE) {
            snprintf(summary,
                     sizeof(summary),
                     "%d nearby scene-like roots around %s",
                     state->browser.nearby_count,
                     state->browser.current_path[0] ? state->browser.current_path : "current input root");
        } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_RECENTS) {
            snprintf(summary,
                     sizeof(summary),
                     "%d recent contexts across layouts, scenes, and roots",
                     count);
        } else {
            snprintf(summary,
                     sizeof(summary),
                     "%d matches in %s",
                     count,
                     state->catalog.input_root[0] ? state->catalog.input_root : "current root");
        }
        line_drawing_host_menu_draw_text_clipped(renderer,
                                                 body_font,
                                                 summary,
                                                 layout->list_header_rect.x,
                                                 layout->list_header_rect.y + 20,
                                                 layout->list_header_rect.w,
                                                 muted_color);
    }
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 96);
    SDL_RenderDrawLine(renderer,
                       layout->list_header_rect.x,
                       layout->list_header_rect.y + layout->list_header_rect.h + 2,
                       layout->list_header_rect.x + layout->list_header_rect.w,
                       layout->list_header_rect.y + layout->list_header_rect.h + 2);
    line_drawing_host_menu_draw_filter(renderer,
                                       body_font,
                                       state,
                                       layout,
                                       panel_fill,
                                       border_color,
                                       title_color,
                                       text_color,
                                       muted_color,
                                       highlight_color);
    line_drawing_host_menu_draw_browse_actions(renderer,
                                               body_font,
                                               state,
                                               layout,
                                               panel_fill,
                                               border_color,
                                               title_color,
                                               text_color,
                                               muted_color,
                                               highlight_color);

    if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_RECENTS) {
        scroll = state->recent_scroll_px;
    } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS) {
        scroll = state->layout_scroll_px;
    } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
        scroll = state->scene_scroll_px;
    } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_BROWSE) {
        scroll = state->browser_scroll_px;
    }

    if (count <= 0) {
        line_drawing_host_menu_draw_text_clipped(renderer,
                                                 body_font,
                                                 ((state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS ||
                                                   state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_SCENES) &&
                                                  state->filter_query[0])
                                                     ? "No matches for the current filter."
                                                     : (state->selected_section ==
                                                        LINE_DRAWING_HOST_MENU_SECTION_BROWSE)
                                                           ? "No nearby scene-like directories found around the current input root."
                                                     : "No entries found for the selected section.",
                                                 clip.x,
                                                 clip.y + 12,
                                                 clip.w,
                                                 muted_color);
        if (line_drawing_host_menu_has_scrollbar(state, layout, state->selected_section)) {
            SDL_SetRenderDrawColor(renderer,
                                   border_color.r,
                                   border_color.g,
                                   border_color.b,
                                   72);
            SDL_RenderFillRect(renderer, &layout->list_scrollbar_rect);
        }
        return;
    }

    SDL_RenderSetClipRect(renderer, &clip);
    y = clip.y - (int)scroll;
    for (i = 0; i < count; ++i) {
        SDL_Rect row_rect = {
            clip.x,
            y,
            clip.w,
            row_height
        };
        bool selected = false;
        bool active = false;
        bool hovered = false;
        SDL_Color fill = line_drawing_host_menu_dim(panel_fill, 230);
        SDL_Color border = border_color;

        if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS) {
            selected = (state->selected_index == i);
        } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_RECENTS) {
            selected = (state->selected_recent_index == i);
        } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS) {
            selected = (state->selected_layout_index == i);
            active = (state->filtered_layout_indices[i] == state->catalog.active_layout_index);
        } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
            selected = (state->selected_scene_index == i);
            active = (state->filtered_scene_indices[i] == state->catalog.active_scene_index);
        } else {
            selected = (state->selected_browser_index == i);
        }
        hovered = (state->hovered_content_index == i);

        if (selected) {
            fill = highlight_color;
            border = title_color;
        } else if (hovered) {
            fill = line_drawing_host_menu_mix(panel_fill, highlight_color, 0.14f);
            border = line_drawing_host_menu_mix(border_color, title_color, 0.16f);
        }

        if (row_rect.y + row_rect.h >= clip.y && row_rect.y <= clip.y + clip.h) {
            if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS) {
                line_drawing_host_menu_draw_quick_action_row(renderer,
                                                             body_font,
                                                             model,
                                                             row_rect,
                                                             i,
                                                             selected,
                                                             fill,
                                                             border,
                                                             selected ? title_color : text_color,
                                                             selected ? title_color
                                                                      : line_drawing_host_menu_dim(muted_color, 245));
            } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_RECENTS) {
                const LineDrawingRecentMenuEntry* entry =
                    LineDrawingRecentMenuList_GetEntry(&state->recent_entries, i);
                line_drawing_host_menu_draw_recent_row(renderer,
                                                       body_font,
                                                       row_rect,
                                                       entry,
                                                       line_drawing_host_menu_preview_for_recent_entry(state, entry),
                                                       fill,
                                                       border,
                                                       selected ? title_color : text_color,
                                                       selected ? title_color
                                                                : line_drawing_host_menu_dim(muted_color, 245),
                                                       active_color);
            } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS) {
                const LineDrawingSceneCatalogEntry* entry =
                    LineDrawingSceneCatalog_GetLayout(&state->catalog,
                                                      state->filtered_layout_indices[i]);
                line_drawing_host_menu_draw_catalog_row(renderer,
                                                        body_font,
                                                        row_rect,
                                                        entry,
                                                        line_drawing_host_menu_preview_for_entry(
                                                            state,
                                                            LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS,
                                                            entry),
                                                        selected,
                                                        active,
                                                        fill,
                                                        border,
                                                        selected ? title_color : text_color,
                                                        selected ? title_color
                                                                 : line_drawing_host_menu_dim(muted_color, 245),
                                                        active_color);
            } else if (state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
                const LineDrawingSceneCatalogEntry* entry =
                    LineDrawingSceneCatalog_GetScene(&state->catalog,
                                                     state->filtered_scene_indices[i]);
                line_drawing_host_menu_draw_catalog_row(renderer,
                                                        body_font,
                                                        row_rect,
                                                        entry,
                                                        line_drawing_host_menu_preview_for_entry(
                                                            state,
                                                            LINE_DRAWING_HOST_MENU_SECTION_SCENES,
                                                            entry),
                                                        selected,
                                                        active,
                                                        fill,
                                                        border,
                                                        selected ? title_color : text_color,
                                                        selected ? title_color
                                                                 : line_drawing_host_menu_dim(muted_color, 245),
                                                        active_color);
            } else {
                line_drawing_host_menu_draw_browser_row(renderer,
                                                        body_font,
                                                        row_rect,
                                                        LineDrawingRootBrowser_GetEntry(&state->browser, i),
                                                        line_drawing_host_menu_preview_for_browser_entry(
                                                            state,
                                                            LineDrawingRootBrowser_GetEntry(
                                                                &state->browser,
                                                                i)),
                                                        fill,
                                                        border,
                                                        selected ? title_color : text_color,
                                                        selected ? title_color
                                                                 : line_drawing_host_menu_dim(muted_color, 245),
                                                        active_color);
            }
        }
        y += row_height + LINE_DRAWING_HOST_MENU_ROW_GAP;
    }
    SDL_RenderSetClipRect(renderer, NULL);

    if (line_drawing_host_menu_has_scrollbar(state, layout, state->selected_section)) {
        SDL_Color track_color = line_drawing_host_menu_mix(panel_fill, border_color, 0.24f);
        SDL_Color thumb_color = line_drawing_host_menu_mix(highlight_color,
                                                           title_color,
                                                           state->scrollbar_dragging ? 0.35f : 0.18f);
        scrollbar_thumb = line_drawing_host_menu_scrollbar_thumb_rect(state,
                                                                      layout,
                                                                      state->selected_section);
        SDL_SetRenderDrawColor(renderer,
                               track_color.r,
                               track_color.g,
                               track_color.b,
                               220);
        SDL_RenderFillRect(renderer, &layout->list_scrollbar_rect);
        SDL_SetRenderDrawColor(renderer,
                               border_color.r,
                               border_color.g,
                               border_color.b,
                               132);
        SDL_RenderDrawRect(renderer, &layout->list_scrollbar_rect);
        SDL_SetRenderDrawColor(renderer,
                               thumb_color.r,
                               thumb_color.g,
                               thumb_color.b,
                               255);
        SDL_RenderFillRect(renderer, &scrollbar_thumb);
        SDL_SetRenderDrawColor(renderer,
                               title_color.r,
                               title_color.g,
                               title_color.b,
                               110);
        SDL_RenderDrawRect(renderer, &scrollbar_thumb);
    }
}

static void line_drawing_host_menu_draw_detail(SDL_Renderer* renderer,
                                               TTF_Font* title_font,
                                               TTF_Font* body_font,
                                               LineDrawingHostMenuState* state,
                                               const LineDrawingHostMenuLayout* layout,
                                               SDL_Color panel_fill,
                                               SDL_Color border_color,
                                               SDL_Color title_color,
                                               SDL_Color text_color,
                                               SDL_Color muted_color,
                                               SDL_Color active_color,
                                               SDL_Color shadow_color) {
    const LineDrawingSceneCatalogEntry* entry = NULL;
    const LineDrawingCatalogPreviewData* preview = NULL;
    const char* detail_title = "No selection";
    const char* detail_subtitle = "Select a section and item to inspect it here.";
    const char* path_value = "";
    const char* status_value = "";
    const char* preview_title = "Preview";
    bool is_active = false;
    char summary[128] = {0};
    char extents[64] = {0};
    SDL_Rect text_rect;
    SDL_Rect preview_body_rect;
    int title_h = line_drawing_host_menu_font_height(title_font);
    int body_h = line_drawing_host_menu_font_height(body_font);
    if (!renderer || !title_font || !body_font || !state || !layout) return;

    line_drawing_host_menu_draw_panel(renderer,
                                      layout->detail_rect,
                                      panel_fill,
                                      border_color,
                                      shadow_color);

    switch (state->selected_section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            detail_title = line_drawing_host_menu_item_label((LineDrawingHostMenuItemId)state->selected_index);
            detail_subtitle =
                line_drawing_host_menu_item_description((LineDrawingHostMenuItemId)state->selected_index);
            status_value = "Host action";
            break;
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS: {
            const LineDrawingRecentMenuEntry* recent_entry =
                line_drawing_host_menu_selected_recent_entry(state);
            if (recent_entry) {
                preview = line_drawing_host_menu_preview_for_recent_entry(state, recent_entry);
                detail_title = recent_entry->label;
                detail_subtitle = recent_entry->description[0]
                                      ? recent_entry->description
                                      : "Recent context entry";
                path_value = recent_entry->path;
                status_value = recent_entry->current ? "Current recent context" : "Recent context";
                switch (recent_entry->kind) {
                    case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT:
                        preview_title = "Recent Layout";
                        break;
                    case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE:
                        preview_title = "Recent Scene";
                        break;
                    case LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT:
                        preview_title = "Input Root Context";
                        snprintf(summary,
                                 sizeof(summary),
                                 "Switch the input root back to this directory.");
                        snprintf(extents,
                                 sizeof(extents),
                                 "Nearby root suggestions will refresh around this root.");
                        break;
                    case LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT:
                        preview_title = "Output Root Context";
                        snprintf(summary,
                                 sizeof(summary),
                                 "Switch the output root back to this directory.");
                        snprintf(extents,
                                 sizeof(extents),
                                 "Exports and browser context will follow this path.");
                        break;
                    default:
                        break;
                }
            } else {
                detail_title = "Recents";
                detail_subtitle = "No recent layouts, scenes, or roots are available yet.";
                status_value = "Waiting for recent context";
            }
            break;
        }
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            entry = line_drawing_host_menu_selected_catalog_entry(state,
                                                                  LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS,
                                                                  &is_active);
            if (entry) {
                preview = line_drawing_host_menu_preview_for_entry(state,
                                                                   LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS,
                                                                   entry);
                detail_title = entry->label;
                detail_subtitle = "Layout catalog entry";
                path_value = entry->path;
                status_value = is_active ? "Current layout" : "Available layout";
            } else {
                detail_title = "Layouts";
                detail_subtitle = "No layout is available under the current input root.";
            }
            break;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            entry = line_drawing_host_menu_selected_catalog_entry(state,
                                                                  LINE_DRAWING_HOST_MENU_SECTION_SCENES,
                                                                  &is_active);
            if (entry) {
                preview = line_drawing_host_menu_preview_for_entry(state,
                                                                   LINE_DRAWING_HOST_MENU_SECTION_SCENES,
                                                                   entry);
                detail_title = entry->label;
                detail_subtitle = "Authoring scene entry";
                path_value = entry->path;
                status_value = is_active ? "Current scene" : "Available scene";
            } else {
                detail_title = "Scenes";
                detail_subtitle = "No authoring scene is available under the current input root.";
            }
            break;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE: {
            const LineDrawingRootBrowserEntry* browser_entry =
                line_drawing_host_menu_selected_browser_entry(state);
            detail_title = state->browser.current_path[0] ? state->browser.current_path : "Browse";
            detail_subtitle =
                "Use the header controls to pick roots directly, then switch quickly through nearby scene-like directories.";
            preview_title = "Root Context";
            path_value = state->browser.current_path;
            status_value = "Root controls";
            if (browser_entry) {
                preview = line_drawing_host_menu_preview_for_browser_entry(state, browser_entry);
                detail_title = browser_entry->label;
                detail_subtitle = browser_entry->description[0]
                                      ? browser_entry->description
                                      : "Nearby root suggestion";
                path_value = browser_entry->path;
                status_value = "Nearby input-root suggestion";
            }
            snprintf(summary,
                     sizeof(summary),
                     "Input: %s",
                     Global_GetInputRoot() ? Global_GetInputRoot() : "(none)");
            snprintf(extents,
                     sizeof(extents),
                     "Output: %s",
                     Global_GetOutputRoot() ? Global_GetOutputRoot() : "(none)");
            break;
        }
        default:
            break;
    }

    line_drawing_host_menu_draw_text(renderer,
                                     title_font,
                                     "Details",
                                     layout->detail_rect.x + 16,
                                     layout->detail_rect.y + 16,
                                     muted_color);
    line_drawing_host_menu_draw_badge(renderer,
                                      body_font,
                                      line_drawing_host_menu_section_label(state->selected_section),
                                      (SDL_Rect){layout->detail_rect.x + layout->detail_rect.w - 92,
                                                 layout->detail_rect.y + 14,
                                                 76,
                                                 22},
                                      line_drawing_host_menu_mix(panel_fill, border_color, 0.24f),
                                      border_color,
                                      muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             title_font,
                                             detail_title,
                                             layout->detail_rect.x + 16,
                                             layout->detail_rect.y + 44,
                                             layout->detail_rect.w - 32,
                                             title_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             body_font,
                                             detail_subtitle,
                                             layout->detail_rect.x + 16,
                                             layout->detail_rect.y + 44 + title_h + 6,
                                             layout->detail_rect.w - 32,
                                             muted_color);

    preview_body_rect = (SDL_Rect){
        layout->detail_preview_rect.x + 10,
        layout->detail_preview_rect.y + 62,
        layout->detail_preview_rect.w - 20,
        layout->detail_preview_rect.h - 72
    };

    line_drawing_host_menu_draw_panel(renderer,
                                      layout->detail_preview_rect,
                                      line_drawing_host_menu_dim(panel_fill, 255),
                                      border_color,
                                      line_drawing_host_menu_dim(border_color, 28));

    line_drawing_host_menu_draw_preview(renderer,
                                        preview_body_rect,
                                        preview,
                                        line_drawing_host_menu_dim(panel_fill, 255),
                                        border_color,
                                        text_color,
                                        muted_color);
    line_drawing_host_menu_draw_text(renderer,
                                     body_font,
                                     preview_title,
                                     layout->detail_preview_rect.x + 12,
                                     layout->detail_preview_rect.y + 10,
                                     title_color);
    line_drawing_host_menu_draw_badge(renderer,
                                      body_font,
                                      status_value && status_value[0] ? status_value : "Waiting",
                                      (SDL_Rect){layout->detail_preview_rect.x + layout->detail_preview_rect.w - 102,
                                                 layout->detail_preview_rect.y + 10,
                                                 90,
                                                 22},
                                      line_drawing_host_menu_mix(panel_fill, active_color, 0.1f),
                                      border_color,
                                      active_color);
    if (state->selected_section != LINE_DRAWING_HOST_MENU_SECTION_BROWSE &&
        !(state->selected_section == LINE_DRAWING_HOST_MENU_SECTION_RECENTS &&
          summary[0] && extents[0])) {
        line_drawing_host_menu_format_preview_summary(preview, summary, sizeof(summary));
        line_drawing_host_menu_format_preview_extents(preview, extents, sizeof(extents));
    }
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             body_font,
                                             summary[0] ? summary
                                                        : (state->selected_section ==
                                                                   LINE_DRAWING_HOST_MENU_SECTION_BROWSE
                                                               ? "Pick a root directly or jump to a nearby scene-like directory."
                                                               : (state->selected_section ==
                                                                          LINE_DRAWING_HOST_MENU_SECTION_RECENTS
                                                                      ? "Recents keep the last layouts, scenes, and roots close."
                                                                      : "Preview unavailable for this entry")),
                                             layout->detail_preview_rect.x + 12,
                                             layout->detail_preview_rect.y + 10 + body_h + 6,
                                             layout->detail_preview_rect.w - 24,
                                             muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             body_font,
                                             extents[0] ? extents
                                                        : (preview && preview->diagnostics[0]
                                                               ? preview->diagnostics
                                                               : (state->selected_section ==
                                                                          LINE_DRAWING_HOST_MENU_SECTION_BROWSE
                                                                      ? "Nearby suggestions are ranked from sibling and cousin branches near the input root."
                                                                      : (state->selected_section ==
                                                                                 LINE_DRAWING_HOST_MENU_SECTION_RECENTS
                                                                             ? "Open a recent file or switch back to a recent root."
                                                                             : "Lightweight wireframe preview"))),
                                             layout->detail_preview_rect.x + 12,
                                             layout->detail_preview_rect.y + 10 + ((body_h + 6) * 2),
                                             layout->detail_preview_rect.w - 24,
                                             muted_color);

    text_rect = (SDL_Rect){
        layout->detail_rect.x + 16,
        layout->detail_preview_rect.y + layout->detail_preview_rect.h + 18,
        layout->detail_rect.w - 32,
        layout->detail_rect.y + layout->detail_rect.h -
            (layout->detail_preview_rect.y + layout->detail_preview_rect.h + 18) - 16
    };

    line_drawing_host_menu_draw_text(renderer, body_font, "Path", text_rect.x, text_rect.y, muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             body_font,
                                             path_value && path_value[0] ? path_value : "(none)",
                                             text_rect.x,
                                             text_rect.y + body_h + 4,
                                             text_rect.w,
                                             text_color);
    line_drawing_host_menu_draw_text(renderer,
                                     body_font,
                                     "Status",
                                     text_rect.x,
                                     text_rect.y + (body_h * 2) + 18,
                                     muted_color);
    line_drawing_host_menu_draw_text_clipped(renderer,
                                             body_font,
                                             status_value && status_value[0] ? status_value : "Waiting for selection",
                                             text_rect.x,
                                             text_rect.y + (body_h * 3) + 22,
                                             text_rect.w,
                                             active_color);
    if (summary[0]) {
        line_drawing_host_menu_draw_text(renderer,
                                         body_font,
                                         "Contents",
                                         text_rect.x,
                                         text_rect.y + (body_h * 4) + 34,
                                         muted_color);
        line_drawing_host_menu_draw_text_clipped(renderer,
                                                 body_font,
                                                 summary,
                                                 text_rect.x,
                                                 text_rect.y + (body_h * 5) + 38,
                                                 text_rect.w,
                                                 text_color);
    }
}

void LineDrawingHostMenu_Render(LineDrawingHostMenuState* state, AppContext* ctx) {
    LineDrawing3dThemePalette palette = {0};
    LineDrawingHostMenuModel model;
    LineDrawingHostMenuLayout layout;
    TTF_Font* title_font = FontManager_Get(FONT_DEFAULT);
    TTF_Font* body_font = FontManager_GetUIPanelFont();
    SDL_Color title_color = {255, 255, 255, 255};
    SDL_Color text_color = {224, 224, 224, 255};
    SDL_Color muted_color = {170, 170, 170, 255};
    SDL_Color border_color = {74, 80, 90, 255};
    SDL_Color panel_fill = {18, 21, 26, 242};
    SDL_Color panel_fill_alt = {12, 15, 20, 248};
    SDL_Color background_fill = {11, 12, 15, 255};
    SDL_Color highlight_color = {40, 68, 108, 255};
    SDL_Color active_color = {178, 214, 255, 255};
    SDL_Color shadow_color = {0, 0, 0, 64};
    int title_h = 24;
    int body_h = 16;

    if (!state || !ctx || !ctx->renderer) return;
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        title_color = palette.text_primary;
        text_color = palette.text_primary;
        muted_color = palette.text_muted;
        border_color = palette.panel_border;
        panel_fill = palette.panel_fill;
        background_fill = palette.background_fill;
        highlight_color = line_drawing_host_menu_mix(
            line_drawing_host_menu_mix(background_fill, palette.menu_highlight, 0.38f),
            (SDL_Color){18, 26, 38, 255},
            0.35f);
        active_color = palette.button_text;
    }
    border_color = line_drawing_host_menu_mix(border_color, title_color, 0.14f);
    panel_fill_alt = line_drawing_host_menu_mix(panel_fill_alt, background_fill, 0.55f);
    shadow_color = line_drawing_host_menu_mix(background_fill, (SDL_Color){0, 0, 0, 120}, 0.55f);

    if (!title_font) title_font = body_font;
    if (!body_font) return;
    title_h = line_drawing_host_menu_font_height(title_font);
    body_h = line_drawing_host_menu_font_height(body_font);

    LineDrawingHostMenu_BuildModel(&model);
    line_drawing_host_menu_layout(&layout,
                                  Global_GetScreenWidth(),
                                  Global_GetScreenHeight());

    SDL_SetRenderDrawColor(ctx->renderer,
                           background_fill.r,
                           background_fill.g,
                           background_fill.b,
                           background_fill.a);
    SDL_RenderFillRect(ctx->renderer, NULL);
    {
        SDL_Rect top_band = {0, 0, Global_GetScreenWidth(), Global_GetScreenHeight() / 3};
        SDL_Color top_band_color = line_drawing_host_menu_mix(background_fill, panel_fill_alt, 0.28f);
        SDL_SetRenderDrawColor(ctx->renderer,
                               top_band_color.r,
                               top_band_color.g,
                               top_band_color.b,
                               top_band_color.a);
        SDL_RenderFillRect(ctx->renderer, &top_band);
    }

    line_drawing_host_menu_draw_panel(ctx->renderer,
                                      layout.card_rect,
                                      panel_fill_alt,
                                      border_color,
                                      shadow_color);

    line_drawing_host_menu_draw_text(ctx->renderer,
                                     title_font,
                                     "sCulpt",
                                     layout.header_rect.x + 4,
                                     layout.header_rect.y + 4,
                                     title_color);
    line_drawing_host_menu_draw_text(ctx->renderer,
                                     body_font,
                                     "Top-level host catalog",
                                     layout.header_rect.x + 4,
                                     layout.header_rect.y + 4 + title_h + 6,
                                     muted_color);
    line_drawing_host_menu_draw_text_clipped(ctx->renderer,
                                             body_font,
                                             "Layouts, scenes, recents, and browse now share one host shell, with preview-backed file access, root switching, and guarded editor return.",
                                             layout.header_rect.x + 4,
                                             layout.header_rect.y + 4 + title_h + body_h + 14,
                                             layout.header_rect.w - 8,
                                             text_color);

    {
        char root_summary[640];
        snprintf(root_summary,
                 sizeof(root_summary),
                 "Root: %s",
                 state->catalog.input_root[0] ? state->catalog.input_root : "(unset)");
        line_drawing_host_menu_draw_text_clipped(ctx->renderer,
                                                 body_font,
                                                 root_summary,
                                                 layout.header_rect.x + 4,
                                                 layout.header_rect.y + layout.header_rect.h - body_h - 4,
                                                 layout.header_rect.w - 8,
                                                 muted_color);
    }

    line_drawing_host_menu_draw_section_nav(ctx->renderer,
                                            body_font,
                                            state,
                                            &layout,
                                            panel_fill,
                                            border_color,
                                            title_color,
                                            text_color,
                                            muted_color,
                                            highlight_color,
                                            shadow_color);
    line_drawing_host_menu_draw_content(ctx->renderer,
                                        body_font,
                                        state,
                                        &model,
                                        &layout,
                                        panel_fill,
                                        border_color,
                                       title_color,
                                       text_color,
                                       muted_color,
                                       highlight_color,
                                       active_color);
    line_drawing_host_menu_draw_detail(ctx->renderer,
                                       title_font,
                                       body_font,
                                       state,
                                       &layout,
                                       panel_fill,
                                       border_color,
                                       title_color,
                                       text_color,
                                       muted_color,
                                       active_color,
                                       shadow_color);

    line_drawing_host_menu_draw_panel(ctx->renderer,
                                      layout.footer_rect,
                                      panel_fill,
                                      border_color,
                                      shadow_color);

    if (state->status_text[0]) {
        SDL_Color status_color = state->status_is_error
                                     ? (SDL_Color){255, 180, 168, 255}
                                     : text_color;
        line_drawing_host_menu_draw_text_clipped(ctx->renderer,
                                                 body_font,
                                                 state->status_text,
                                                 layout.status_rect.x,
                                                 layout.status_rect.y,
                                                 layout.status_rect.w,
                                                 status_color);
    }

    line_drawing_host_menu_draw_text_clipped(ctx->renderer,
                                             body_font,
                                             "Keyboard: Tab cycles panes, / filters layouts and scenes, Enter opens or navigates, Esc quits here and returns from editor, Ctrl/Cmd+B picks input root, Shift+Ctrl/Cmd+B picks output root, Ctrl/Cmd+M also returns here.",
                                             layout.keyboard_rect.x,
                                             layout.keyboard_rect.y,
                                             layout.keyboard_rect.w,
                                             muted_color);

    {
        char input_line[640];
        char output_line[640];
        snprintf(input_line,
                 sizeof(input_line),
                 "Input Root: %s",
                 Global_GetInputRoot() && Global_GetInputRoot()[0] ? Global_GetInputRoot() : "(unset)");
        snprintf(output_line,
                 sizeof(output_line),
                 "Output Root: %s",
                 Global_GetOutputRoot() && Global_GetOutputRoot()[0] ? Global_GetOutputRoot() : "(unset)");
        line_drawing_host_menu_draw_text_clipped(ctx->renderer,
                                                 body_font,
                                                 input_line,
                                                 layout.input_root_rect.x,
                                                 layout.input_root_rect.y,
                                                 layout.input_root_rect.w,
                                                 text_color);
        line_drawing_host_menu_draw_text_clipped(ctx->renderer,
                                                 body_font,
                                                 output_line,
                                                 layout.output_root_rect.x,
                                                 layout.output_root_rect.y,
                                                 layout.output_root_rect.w,
                                                 text_color);
    }
}
