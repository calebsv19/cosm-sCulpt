#include "Menu/line_drawing_host_menu_render_internal.h"

#include "Core/line_drawing_file_catalog.h"
#include "UI/text_draw.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

const char* line_drawing_host_menu_item_label(LineDrawingHostMenuItemId item_id) {
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

const char* line_drawing_host_menu_item_description(LineDrawingHostMenuItemId item_id) {
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

const char* line_drawing_host_menu_section_label(LineDrawingHostMenuSection section) {
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

int line_drawing_host_menu_font_height(TTF_Font* font) {
    int height = 16;
    if (font) {
        height = TTF_FontHeight(font);
    }
    if (height < 14) height = 14;
    return height;
}

SDL_Color line_drawing_host_menu_dim(SDL_Color color, Uint8 alpha) {
    SDL_Color out = color;
    out.a = alpha;
    out.r = (Uint8)((out.r * 3u) / 5u);
    out.g = (Uint8)((out.g * 3u) / 5u);
    out.b = (Uint8)((out.b * 3u) / 5u);
    return out;
}

SDL_Color line_drawing_host_menu_mix(SDL_Color a, SDL_Color b, float t) {
    SDL_Color out = a;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    out.r = (Uint8)((a.r * (1.0f - t)) + (b.r * t));
    out.g = (Uint8)((a.g * (1.0f - t)) + (b.g * t));
    out.b = (Uint8)((a.b * (1.0f - t)) + (b.b * t));
    out.a = (Uint8)((a.a * (1.0f - t)) + (b.a * t));
    return out;
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

void line_drawing_host_menu_draw_text(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_text_clipped(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_panel(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_accent_bar(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_badge(SDL_Renderer* renderer,
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

void line_drawing_host_menu_format_preview_summary(const LineDrawingCatalogPreviewData* preview,
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

void line_drawing_host_menu_format_preview_extents(const LineDrawingCatalogPreviewData* preview,
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

void line_drawing_host_menu_draw_preview(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_section_nav(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_quick_action_row(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_catalog_row(SDL_Renderer* renderer,
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
                                             extents[0] ? extents : LineDrawingFileCatalog_PathBasename(entry->path),
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

void line_drawing_host_menu_draw_recent_row(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_browser_row(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_browse_actions(SDL_Renderer* renderer,
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

void line_drawing_host_menu_draw_filter(SDL_Renderer* renderer,
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
