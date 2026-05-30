#include "Menu/line_drawing_host_menu_internal.h"

#include <SDL2/SDL.h>
#include <string.h>

int* line_drawing_host_menu_selected_content_index_ptr(LineDrawingHostMenuState* state,
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

float* line_drawing_host_menu_scroll_ptr(LineDrawingHostMenuState* state,
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

void line_drawing_host_menu_layout(LineDrawingHostMenuLayout* out_layout,
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

int line_drawing_host_menu_hit_test_nav(const LineDrawingHostMenuLayout* layout,
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

bool line_drawing_host_menu_hit_test_filter(const LineDrawingHostMenuState* state,
                                            const LineDrawingHostMenuLayout* layout,
                                            int mouse_x,
                                            int mouse_y) {
    SDL_Point point = {mouse_x, mouse_y};
    if (!state || !layout) return false;
    if (!line_drawing_host_menu_section_supports_filter(state->selected_section)) return false;
    return SDL_PointInRect(&point, &layout->filter_rect);
}

int line_drawing_host_menu_hit_test_browse_action(const LineDrawingHostMenuState* state,
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

void line_drawing_host_menu_clamp_scroll(LineDrawingHostMenuState* state,
                                         const LineDrawingHostMenuLayout* layout,
                                         LineDrawingHostMenuSection section) {
    float* scroll = line_drawing_host_menu_scroll_ptr(state, section);
    float max_scroll = line_drawing_host_menu_list_max_scroll(state, layout, section);
    if (!scroll) return;
    if (*scroll < 0.0f) *scroll = 0.0f;
    if (*scroll > max_scroll) *scroll = max_scroll;
}

bool line_drawing_host_menu_has_scrollbar(const LineDrawingHostMenuState* state,
                                          const LineDrawingHostMenuLayout* layout,
                                          LineDrawingHostMenuSection section) {
    return line_drawing_host_menu_list_max_scroll(state, layout, section) > 0.5f;
}

SDL_Rect line_drawing_host_menu_scrollbar_thumb_rect(const LineDrawingHostMenuState* state,
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

bool line_drawing_host_menu_scrollbar_drag_to(LineDrawingHostMenuState* state,
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

bool line_drawing_host_menu_jump_scrollbar_to(LineDrawingHostMenuState* state,
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

void line_drawing_host_menu_ensure_selected_visible(LineDrawingHostMenuState* state,
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

int line_drawing_host_menu_hit_test_content(const LineDrawingHostMenuState* state,
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

bool line_drawing_host_menu_hit_test_scrollbar_track(const LineDrawingHostMenuState* state,
                                                     const LineDrawingHostMenuLayout* layout,
                                                     int mouse_x,
                                                     int mouse_y) {
    SDL_Point point = {mouse_x, mouse_y};
    if (!state || !layout) return false;
    if (!line_drawing_host_menu_has_scrollbar(state, layout, state->selected_section)) return false;
    return SDL_PointInRect(&point, &layout->list_scrollbar_rect);
}
