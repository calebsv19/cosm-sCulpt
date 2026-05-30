#include "Menu/line_drawing_host_menu_render_internal.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <stdio.h>

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
