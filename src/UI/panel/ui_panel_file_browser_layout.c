#include "UI/panel/ui_panel_file_browser_internal.h"
#include "UI/ui_panel_file_layout.h"

#include <math.h>

enum {
    UI_LOAD_MENU_PADDING_PX = 8,
    UI_LOAD_MENU_HEADER_H = 28,
    UI_LOAD_MENU_FOOTER_H = 20,
    UI_LOAD_MENU_ROW_H = 24,
    UI_LOAD_MENU_SCROLLBAR_W = 10,
    UI_LOAD_MENU_GUTTER_PX = 8
};

float UIPanel_LoadMenuContentHeight(const UIPanelState* ui) {
    if (!ui) return 0.0f;
    return (float)ui->loadMenu.count * (float)UI_LOAD_MENU_ROW_H;
}

SDL_Rect UIPanel_GetLoadMenuListClipRect(const UIPanelState* ui) {
    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    SDL_Rect clip = {
        rect.x + UI_LOAD_MENU_PADDING_PX,
        rect.y + UI_LOAD_MENU_HEADER_H,
        rect.w - (UI_LOAD_MENU_PADDING_PX * 2) - UI_LOAD_MENU_SCROLLBAR_W - UI_LOAD_MENU_GUTTER_PX,
        rect.h - UI_LOAD_MENU_HEADER_H - UI_LOAD_MENU_FOOTER_H
    };
    if (clip.w < 0) clip.w = 0;
    if (clip.h < 0) clip.h = 0;
    return clip;
}

SDL_Rect UIPanel_GetLoadMenuScrollTrackRect(const UIPanelState* ui) {
    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    SDL_Rect clip = UIPanel_GetLoadMenuListClipRect(ui);
    SDL_Rect track = {
        rect.x + rect.w - UI_LOAD_MENU_PADDING_PX - UI_LOAD_MENU_SCROLLBAR_W,
        clip.y,
        UI_LOAD_MENU_SCROLLBAR_W,
        clip.h
    };
    if (track.h < 0) track.h = 0;
    return track;
}

float UIPanel_LoadMenuMaxScrollOffset(const UIPanelState* ui) {
    SDL_Rect clip = UIPanel_GetLoadMenuListClipRect(ui);
    float max_offset = UIPanel_LoadMenuContentHeight(ui) - (float)clip.h;
    return max_offset > 0.0f ? max_offset : 0.0f;
}

bool UIPanel_LoadMenuHasScrollableContent(const UIPanelState* ui) {
    return UIPanel_LoadMenuMaxScrollOffset(ui) > 0.5f;
}

void UIPanel_LoadMenuClampScroll(UIPanelState* ui) {
    float max_offset = UIPanel_LoadMenuMaxScrollOffset(ui);
    if (!ui) return;
    if (ui->loadMenu.scrollOffsetPx < 0.0f) ui->loadMenu.scrollOffsetPx = 0.0f;
    if (ui->loadMenu.scrollOffsetPx > max_offset) ui->loadMenu.scrollOffsetPx = max_offset;
}

void UIPanel_LoadMenuScrollIndexIntoView(UIPanelState* ui, int index) {
    SDL_Rect clip = {0, 0, 0, 0};
    float row_top = 0.0f;
    float row_bottom = 0.0f;
    if (!ui || index < 0 || index >= ui->loadMenu.count) return;
    clip = UIPanel_GetLoadMenuListClipRect(ui);
    if (clip.h <= 0) return;
    row_top = (float)index * (float)UI_LOAD_MENU_ROW_H;
    row_bottom = row_top + (float)UI_LOAD_MENU_ROW_H;
    if (row_top < ui->loadMenu.scrollOffsetPx) {
        ui->loadMenu.scrollOffsetPx = row_top;
    } else if (row_bottom > ui->loadMenu.scrollOffsetPx + (float)clip.h) {
        ui->loadMenu.scrollOffsetPx = row_bottom - (float)clip.h;
    }
    UIPanel_LoadMenuClampScroll(ui);
}

SDL_Rect UIPanel_GetLoadMenuScrollThumbRect(const UIPanelState* ui) {
    SDL_Rect track = UIPanel_GetLoadMenuScrollTrackRect(ui);
    SDL_Rect thumb = track;
    float content_h = UIPanel_LoadMenuContentHeight(ui);
    float max_offset = UIPanel_LoadMenuMaxScrollOffset(ui);
    float ratio = 1.0f;
    float thumb_h = 0.0f;
    float travel = 0.0f;
    float offset_ratio = 0.0f;

    if (track.h <= 0 || content_h <= 0.0f) {
        thumb.h = 0;
        return thumb;
    }

    ratio = (float)track.h / content_h;
    if (ratio > 1.0f) ratio = 1.0f;
    thumb_h = ratio * (float)track.h;
    if (thumb_h < 12.0f) thumb_h = 12.0f;
    if (thumb_h > (float)track.h) thumb_h = (float)track.h;
    thumb.h = (int)lroundf(thumb_h);

    travel = (float)track.h - thumb_h;
    if (travel <= 0.0f || max_offset <= 0.0f) {
        return thumb;
    }

    offset_ratio = ui->loadMenu.scrollOffsetPx / max_offset;
    if (offset_ratio < 0.0f) offset_ratio = 0.0f;
    if (offset_ratio > 1.0f) offset_ratio = 1.0f;
    thumb.y = track.y + (int)lroundf(offset_ratio * travel);
    return thumb;
}

SDL_Rect UIPanel_GetLoadMenuSetDirectoryButtonRect(const UIPanelState* ui) {
    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    SDL_Rect button = {0, 0, 0, 0};
    const int button_w = 92;
    const int button_h = 18;
    if (!ui || rect.w <= 0 || rect.h <= 0 || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) {
        return button;
    }
    button.w = button_w;
    button.h = button_h;
    button.x = rect.x + rect.w - UI_LOAD_MENU_PADDING_PX - button.w;
    button.y = rect.y + 5;
    if (button.x < rect.x + UI_LOAD_MENU_PADDING_PX) {
        button.x = rect.x + UI_LOAD_MENU_PADDING_PX;
        button.w = rect.w - (UI_LOAD_MENU_PADDING_PX * 2);
        if (button.w < 0) button.w = 0;
    }
    return button;
}

int UIPanel_LoadMenuIndexAtPoint(const UIPanelState* ui, int mouseX, int mouseY) {
    SDL_Rect clip = UIPanel_GetLoadMenuListClipRect(ui);
    int content_y = 0;
    if (!ui) return -1;
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &clip)) return -1;
    content_y = (mouseY - clip.y) + (int)floorf(ui->loadMenu.scrollOffsetPx);
    if (content_y < 0) return -1;
    {
        const int index = content_y / UI_LOAD_MENU_ROW_H;
        if (index < 0 || index >= ui->loadMenu.count) return -1;
        return index;
    }
}

SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui) {
    SDL_Rect rect = {0, 0, 0, 0};
    if (ui && UIPanel_GetFilePaneRects(ui, NULL, NULL, NULL, &rect)) {
        return rect;
    }
    return rect;
}

SDL_Rect UIPanel_GetLoadMenuPaneClipRect(const UIPanelState* ui) {
    return UIPanel_GetLoadMenuRect(ui);
}
