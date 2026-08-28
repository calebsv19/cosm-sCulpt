#include "UI/ui_panel_right_scroll.h"

#include "Core/global_state.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/font_manager.h"

enum {
    UI_RIGHT_SCROLLBAR_W = 8,
    UI_RIGHT_SCROLLBAR_GUTTER = 3,
    UI_RIGHT_SCROLLBAR_MIN_THUMB_H = 28,
    UI_RIGHT_SCROLL_WHEEL_STEP = 42
};

static bool UIPanelRightScroll_PointInRect(int x, int y, SDL_Rect rect) {
    return rect.w > 0 && rect.h > 0 &&
           x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

static float UIPanelRightScroll_MaxOffset(const UIPanelState* ui) {
    const int tab = ui ? (int)ui->activeRightTab : -1;
    float max_offset = 0.0f;
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT) return 0.0f;
    max_offset = ui->rightScroll[tab].contentHeightPx - (float)ui->rightBodyRect.h;
    return max_offset > 0.0f ? max_offset : 0.0f;
}

static void UIPanelRightScroll_Clamp(UIPanelState* ui) {
    const int tab = ui ? (int)ui->activeRightTab : -1;
    float max_offset = 0.0f;
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT) return;
    max_offset = UIPanelRightScroll_MaxOffset(ui);
    if (ui->rightScroll[tab].scrollOffsetPx < 0.0f) ui->rightScroll[tab].scrollOffsetPx = 0.0f;
    if (ui->rightScroll[tab].scrollOffsetPx > max_offset) {
        ui->rightScroll[tab].scrollOffsetPx = max_offset;
    }
}

static SDL_Rect UIPanelRightScroll_Track(const UIPanelState* ui) {
    SDL_Rect track = {0, 0, 0, 0};
    if (!ui) return track;
    track.x = ui->rightBodyRect.x + ui->rightBodyRect.w - UI_RIGHT_SCROLLBAR_W;
    track.y = ui->rightBodyRect.y;
    track.w = UI_RIGHT_SCROLLBAR_W;
    track.h = ui->rightBodyRect.h;
    return track;
}

static SDL_Rect UIPanelRightScroll_Thumb(const UIPanelState* ui) {
    SDL_Rect track = UIPanelRightScroll_Track(ui);
    SDL_Rect thumb = track;
    const int tab = ui ? (int)ui->activeRightTab : -1;
    float max_offset = UIPanelRightScroll_MaxOffset(ui);
    float content_height = 0.0f;
    float ratio = 1.0f;
    float travel = 0.0f;
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT || max_offset <= 0.0f || track.h <= 0) {
        thumb.h = 0;
        return thumb;
    }
    content_height = ui->rightScroll[tab].contentHeightPx;
    ratio = content_height > 0.0f ? (float)track.h / content_height : 1.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    thumb.h = (int)(ratio * (float)track.h);
    if (thumb.h < UI_RIGHT_SCROLLBAR_MIN_THUMB_H) thumb.h = UI_RIGHT_SCROLLBAR_MIN_THUMB_H;
    if (thumb.h > track.h) thumb.h = track.h;
    travel = (float)(track.h - thumb.h);
    thumb.y = track.y + (int)(travel * (ui->rightScroll[tab].scrollOffsetPx / max_offset));
    return thumb;
}

float UIPanel_RightScrollOffset(const UIPanelState* ui) {
    const int tab = ui ? (int)ui->activeRightTab : -1;
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT) return 0.0f;
    return ui->rightScroll[tab].scrollOffsetPx;
}

void UIPanel_RightScrollSetContentHeight(UIPanelState* ui, float content_height_px) {
    const int tab = ui ? (int)ui->activeRightTab : -1;
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT) return;
    if (content_height_px < 0.0f) content_height_px = 0.0f;
    ui->rightScroll[tab].contentHeightPx = content_height_px;
    UIPanelRightScroll_Clamp(ui);
}

bool UIPanel_RightScrollHandleWheel(int mouse_x, int mouse_y, float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    const int tab = ui ? (int)ui->activeRightTab : -1;
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT ||
        !UIPanelRightScroll_PointInRect(mouse_x, mouse_y, ui->rightBodyRect) ||
        UIPanelRightScroll_MaxOffset(ui) <= 0.0f) {
        return false;
    }
    ui->rightScroll[tab].scrollOffsetPx -= wheel_delta * (float)UI_RIGHT_SCROLL_WHEEL_STEP;
    UIPanelRightScroll_Clamp(ui);
    UIPanel_OnWindowResized(Global_GetScreenWidth(), Global_GetScreenHeight());
    return true;
}

bool UIPanel_RightScrollHandleClick(int mouse_x, int mouse_y) {
    UIPanelState* ui = UIPanel_Get();
    const int tab = ui ? (int)ui->activeRightTab : -1;
    SDL_Rect track = UIPanelRightScroll_Track(ui);
    SDL_Rect thumb = UIPanelRightScroll_Thumb(ui);
    float max_offset = UIPanelRightScroll_MaxOffset(ui);
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT || max_offset <= 0.0f ||
        !UIPanelRightScroll_PointInRect(mouse_x, mouse_y, track)) return false;
    if (UIPanelRightScroll_PointInRect(mouse_x, mouse_y, thumb)) {
        ui->rightScroll[tab].scrollbarDragging = true;
        ui->rightScroll[tab].scrollbarDragStartY = mouse_y;
        ui->rightScroll[tab].scrollbarDragStartOffsetPx = ui->rightScroll[tab].scrollOffsetPx;
    } else {
        const float travel = (float)(track.h - thumb.h);
        float ratio = travel > 0.0f ? (float)(mouse_y - track.y - thumb.h / 2) / travel : 0.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        ui->rightScroll[tab].scrollOffsetPx = ratio * max_offset;
        UIPanel_OnWindowResized(Global_GetScreenWidth(), Global_GetScreenHeight());
    }
    return true;
}

void UIPanel_RightScrollHandleMouseMotion(int mouse_x, int mouse_y) {
    UIPanelState* ui = UIPanel_Get();
    const int tab = ui ? (int)ui->activeRightTab : -1;
    SDL_Rect track = UIPanelRightScroll_Track(ui);
    SDL_Rect thumb = UIPanelRightScroll_Thumb(ui);
    float max_offset = UIPanelRightScroll_MaxOffset(ui);
    float travel = (float)(track.h - thumb.h);
    (void)mouse_x;
    if (!ui || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT ||
        !ui->rightScroll[tab].scrollbarDragging || travel <= 0.0f || max_offset <= 0.0f) return;
    ui->rightScroll[tab].scrollOffsetPx = ui->rightScroll[tab].scrollbarDragStartOffsetPx +
        ((float)(mouse_y - ui->rightScroll[tab].scrollbarDragStartY) / travel) * max_offset;
    UIPanelRightScroll_Clamp(ui);
    UIPanel_OnWindowResized(Global_GetScreenWidth(), Global_GetScreenHeight());
}

void UIPanel_RightScrollHandleMouseUp(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    for (int i = 0; i < UI_PANEL_RIGHT_TAB_COUNT; ++i) {
        ui->rightScroll[i].scrollbarDragging = false;
    }
}

void UIPanel_RightScrollRender(const UIPanelState* ui, SDL_Renderer* renderer) {
    SDL_Rect track = UIPanelRightScroll_Track(ui);
    SDL_Rect thumb = UIPanelRightScroll_Thumb(ui);
    UIPanelVisualPalette palette = {0};
    const int tab = ui ? (int)ui->activeRightTab : -1;
    if (!ui || !renderer || tab < 0 || tab >= UI_PANEL_RIGHT_TAB_COUNT || thumb.h <= 0) return;
    (void)UIPanelVisual_ResolvePalette(&palette);
    track.x += UI_RIGHT_SCROLLBAR_GUTTER;
    track.w -= UI_RIGHT_SCROLLBAR_GUTTER;
    thumb.x = track.x;
    thumb.w = track.w;
    UIPanelVisual_DrawScrollbar(renderer,
                                track,
                                thumb,
                                palette.workspace_fill,
                                palette.button_border,
                                palette.accent,
                                palette.pane_border,
                                ui->rightScroll[tab].scrollbarDragging);
}
