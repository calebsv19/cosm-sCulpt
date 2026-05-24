#include "UI/ui_panel_scene_list.h"

#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"
#include "Core/global_state.h"
#include "Editor/editor.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

enum {
    UI_SCENE_LIST_HEADER_ROWS = 2
};

static int UIPanelSceneList_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelSceneList_LineGap(void) {
    int gap = UIPanelSceneList_FontHeight() / 3;
    if (gap < 4) gap = 4;
    return gap;
}

static int UIPanelSceneList_RowHeight(void) {
    int row = (UIPanelSceneList_FontHeight() * 2) + 12;
    if (row < 34) row = 34;
    return row;
}

static int UIPanelSceneList_RowGap(void) {
    return 4;
}

static bool UIPanelSceneList_PointInRect(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

static size_t UIPanelSceneList_LiveObjectCount(const LayoutObjectStore* store) {
    size_t count = 0u;
    if (!store) return 0u;
    for (size_t i = 0; i < store->count; ++i) {
        if (!store->items[i].isDeleted && store->items[i].objectId != 0u) ++count;
    }
    return count;
}

static const Object3D* UIPanelSceneList_ObjectAtVisibleIndex(const LayoutObjectStore* store, int visibleIndex) {
    int current = 0;
    if (!store || visibleIndex < 0) return NULL;
    for (size_t i = 0; i < store->count; ++i) {
        const Object3D* object = &store->items[i];
        if (object->isDeleted || object->objectId == 0u) continue;
        if (current == visibleIndex) return object;
        ++current;
    }
    return NULL;
}

static SDL_Rect UIPanelSceneList_ListRect(const UIPanelState* ui) {
    SDL_Rect rect = {0, 0, 0, 0};
    int font_h = UIPanelSceneList_FontHeight();
    int line_gap = UIPanelSceneList_LineGap();
    int header_h = (font_h * UI_SCENE_LIST_HEADER_ROWS) + line_gap + 12;
    if (!ui) return rect;
    rect = ui->leftBodyRect;
    rect.y += header_h;
    rect.h -= header_h;
    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_LEFT || btn->group != UI_PANEL_GROUP_LEFT_SCENE_BOUNDS) continue;
        if (btn->bounds.w <= 0 || btn->bounds.h <= 0) continue;
        if (btn->bounds.y > rect.y) {
            int clipped_h = (btn->bounds.y - 8) - rect.y;
            if (clipped_h < rect.h) rect.h = clipped_h;
        }
        break;
    }
    if (rect.h < 0) rect.h = 0;
    return rect;
}

static float UIPanelSceneList_MaxScrollOffset(const UIPanelState* ui, const LayoutObjectStore* store) {
    SDL_Rect listRect = UIPanelSceneList_ListRect(ui);
    const size_t liveCount = UIPanelSceneList_LiveObjectCount(store);
    const int rowH = UIPanelSceneList_RowHeight();
    const int rowGap = UIPanelSceneList_RowGap();
    float totalHeight = 0.0f;
    if (liveCount == 0u) return 0.0f;
    totalHeight = (float)(liveCount * (size_t)(rowH + rowGap));
    totalHeight -= (float)rowGap;
    if (totalHeight <= (float)listRect.h) return 0.0f;
    return totalHeight - (float)listRect.h;
}

static void UIPanelSceneList_ClampScroll(UIPanelState* ui, const LayoutObjectStore* store) {
    float maxOffset = 0.0f;
    if (!ui) return;
    maxOffset = UIPanelSceneList_MaxScrollOffset(ui, store);
    if (ui->sceneList.scrollOffsetPx < 0.0f) ui->sceneList.scrollOffsetPx = 0.0f;
    if (ui->sceneList.scrollOffsetPx > maxOffset) ui->sceneList.scrollOffsetPx = maxOffset;
}

static int UIPanelSceneList_RowIndexAtPoint(const UIPanelState* ui,
                                            const LayoutObjectStore* store,
                                            int mouseX,
                                            int mouseY) {
    SDL_Rect listRect = UIPanelSceneList_ListRect(ui);
    const int rowH = UIPanelSceneList_RowHeight();
    const int rowGap = UIPanelSceneList_RowGap();
    float contentY = 0.0f;
    size_t liveCount = 0u;
    int rowIndex = 0;
    if (!ui || !store) return -1;
    if (!UIPanelSceneList_PointInRect(mouseX, mouseY, listRect)) return -1;
    liveCount = UIPanelSceneList_LiveObjectCount(store);
    contentY = (float)(mouseY - listRect.y) + ui->sceneList.scrollOffsetPx;
    rowIndex = (int)(contentY / (float)(rowH + rowGap));
    if (rowIndex < 0 || (size_t)rowIndex >= liveCount) return -1;
    if ((contentY - (float)(rowIndex * (rowH + rowGap))) > (float)rowH) return -1;
    return rowIndex;
}

static void UIPanelSceneList_SelectObject(uint32_t objectId) {
    GlobalState* state = Global_Get();
    EditorState* editor = NULL;
    if (!state || objectId == 0u) return;
    editor = &state->editor;
    Editor_ClearAnchorSelection(editor);
    editor->selectedObject3DId = objectId;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    editor->selectedWallIndex = -1;
    editor->selectedHandleAnchor = -1;
    editor->selectedHandleComponent = -1;
    editor->hoveredObject3DId = objectId;
    Global_FlagHitboxesDirty();
}

bool UIPanel_HandleSceneListClick(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    SDL_Rect body = {0};
    int rowIndex = -1;
    const Object3D* object = NULL;
    if (!ui || !state) return false;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return false;
    body = ui->leftBodyRect;
    if (!UIPanelSceneList_PointInRect(mouseX, mouseY, body)) return false;

    rowIndex = UIPanelSceneList_RowIndexAtPoint(ui, &state->layout.objectStore, mouseX, mouseY);
    if (rowIndex >= 0) {
        object = UIPanelSceneList_ObjectAtVisibleIndex(&state->layout.objectStore, rowIndex);
        if (object) {
            UIPanelSceneList_SelectObject(object->objectId);
        }
    }
    return true;
}

bool UIPanel_HandleSceneListWheel(int mouseX, int mouseY, float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    SDL_Rect body = {0};
    if (!ui || !state) return false;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return false;
    body = ui->leftBodyRect;
    if (!UIPanelSceneList_PointInRect(mouseX, mouseY, body)) return false;
    ui->sceneList.scrollOffsetPx -= wheel_delta * (float)(UIPanelSceneList_RowHeight() * 2);
    UIPanelSceneList_ClampScroll(ui, &state->layout.objectStore);
    return true;
}

void UIPanel_HandleSceneListMouseMotion(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    if (!ui || !state || ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) {
        if (ui) ui->sceneList.hoverIndex = -1;
        return;
    }
    ui->sceneList.hoverIndex = UIPanelSceneList_RowIndexAtPoint(ui, &state->layout.objectStore, mouseX, mouseY);
}

static void UIPanelSceneList_DrawText(SDL_Renderer* renderer,
                                      TTF_Font* font,
                                      const char* text,
                                      int x,
                                      int y,
                                      SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void UIPanelSceneList_DrawTextClipped(SDL_Renderer* renderer,
                                             TTF_Font* font,
                                             const char* text,
                                             int x,
                                             int y,
                                             int maxWidth,
                                             SDL_Color color) {
    char clipped[256];
    int width = 0;
    if (!renderer || !font || !text) return;
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) && width <= maxWidth) {
        UIPanelSceneList_DrawText(renderer, font, text, x, y, color);
        return;
    }
    snprintf(clipped, sizeof(clipped), "%s", text);
    while (strlen(clipped) > 3) {
        size_t len = strlen(clipped);
        clipped[len - 1] = '\0';
        clipped[len - 2] = '.';
        clipped[len - 3] = '.';
        clipped[len - 4] = '.';
        if (line_drawing_text_measure_utf8(renderer, font, clipped, &width, NULL) && width <= maxWidth) {
            UIPanelSceneList_DrawText(renderer, font, clipped, x, y, color);
            return;
        }
    }
}

void Render_UIPanelSceneList(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color labelColor = {200, 200, 210, 255};
    SDL_Color valueColor = {230, 230, 235, 255};
    SDL_Color accentColor = {140, 170, 210, 255};
    SDL_Color rowFill = {34, 37, 42, 220};
    SDL_Color rowBorder = {80, 92, 110, 230};
    SDL_Color selectedFill = {82, 104, 132, 235};
    SDL_Color selectedBorder = {170, 188, 214, 255};
    SDL_Color hoverFill = {48, 54, 62, 230};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect listRect = {0};
    int fontH = 0;
    int lineGap = 0;
    int y = 0;
    size_t liveCount = 0u;

    if (!ui || !renderer || !state || !font) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;
    if (ui->leftBodyRect.w <= 0 || ui->leftBodyRect.h <= 0) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        labelColor = palette.text_muted;
        valueColor = palette.text_primary;
        accentColor = palette.button_border;
        rowFill = palette.panel_fill;
        rowBorder = palette.panel_border;
        selectedFill = palette.button_fill;
        selectedBorder = palette.button_border;
        hoverFill = palette.button_fill;
        hoverFill.a = 180;
    }

    fontH = UIPanelSceneList_FontHeight();
    lineGap = UIPanelSceneList_LineGap();
    y = ui->leftBodyRect.y + 4;
    liveCount = UIPanelSceneList_LiveObjectCount(&state->layout.objectStore);
    UIPanelSceneList_DrawText(renderer, font, "Scene Objects", ui->leftBodyRect.x + 4, y, valueColor);
    y += fontH + lineGap;
    {
        char summary[128];
        snprintf(summary,
                 sizeof(summary),
                 "%zu objects. Click a row to select it in the editor.",
                 liveCount);
        UIPanelSceneList_DrawTextClipped(renderer,
                                         font,
                                         summary,
                                         ui->leftBodyRect.x + 4,
                                         y,
                                         ui->leftBodyRect.w - 8,
                                         labelColor);
    }

    listRect = UIPanelSceneList_ListRect(ui);
    if (listRect.w > 0 && listRect.h > 0) {
        SDL_Rect listInner = listRect;
        SDL_SetRenderDrawColor(renderer, rowFill.r, rowFill.g, rowFill.b, 150);
        SDL_RenderFillRect(renderer, &listRect);
        SDL_SetRenderDrawColor(renderer, rowBorder.r, rowBorder.g, rowBorder.b, 210);
        SDL_RenderDrawRect(renderer, &listRect);
        listInner.x += 1;
        listInner.y += 1;
        listInner.w -= 2;
        listInner.h -= 2;
        if (listInner.w > 0 && listInner.h > 0) {
            SDL_SetRenderDrawColor(renderer, rowBorder.r, rowBorder.g, rowBorder.b, 60);
            SDL_RenderDrawRect(renderer, &listInner);
        }
    }

    if (liveCount == 0u) {
        UIPanelSceneList_DrawTextClipped(renderer,
                                         font,
                                         "No authored plane/prism objects yet.",
                                         listRect.x + 4,
                                         listRect.y + 8,
                                         listRect.w - 8,
                                         labelColor);
        return;
    }

    {
        const int rowH = UIPanelSceneList_RowHeight();
        const int rowGap = UIPanelSceneList_RowGap();
        const int contentOffset = (int)ui->sceneList.scrollOffsetPx;
        const int clipBottom = listRect.y + listRect.h;
        int visibleIndex = 0;
        for (;;) {
            const Object3D* object = UIPanelSceneList_ObjectAtVisibleIndex(&state->layout.objectStore, visibleIndex);
            SDL_Rect rowRect = {0};
            char line0[96];
            char line1[160];
            if (!object) break;
            rowRect.x = listRect.x + 2;
            rowRect.y = listRect.y + (visibleIndex * (rowH + rowGap)) - contentOffset;
            rowRect.w = listRect.w - 4;
            rowRect.h = rowH;
            if (rowRect.y + rowRect.h < listRect.y) {
                ++visibleIndex;
                continue;
            }
            if (rowRect.y > clipBottom) break;

            {
                const bool isSelected = (state->editor.selectedObject3DId == object->objectId);
                const bool isHovered = (ui->sceneList.hoverIndex == visibleIndex);
                SDL_Color fill = isSelected ? selectedFill : (isHovered ? hoverFill : rowFill);
                SDL_Color border = isSelected ? selectedBorder : rowBorder;
                SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
                SDL_RenderFillRect(renderer, &rowRect);
                SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
                SDL_RenderDrawRect(renderer, &rowRect);
                if (isSelected || isHovered) {
                    SDL_Rect accentRect = {
                        rowRect.x,
                        rowRect.y,
                        4,
                        rowRect.h
                    };
                    SDL_Color accent = isSelected ? selectedBorder : accentColor;
                    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 235);
                    SDL_RenderFillRect(renderer, &accentRect);
                }
            }

            snprintf(line0,
                     sizeof(line0),
                     "#%u  %s",
                     object->objectId,
                     object->kind == OBJECT3D_KIND_RECT_PRISM ? "Prism" : "Plane");
            snprintf(line1,
                     sizeof(line1),
                     "Pos %.1f, %.1f, %.1f",
                     object->transform.position.x,
                     object->transform.position.y,
                     object->transform.position.z);
            UIPanelSceneList_DrawTextClipped(renderer,
                                             font,
                                             line0,
                                             rowRect.x + 8,
                                             rowRect.y + 6,
                                             rowRect.w - 16,
                                             valueColor);
            UIPanelSceneList_DrawTextClipped(renderer,
                                             font,
                                             line1,
                                             rowRect.x + 8,
                                             rowRect.y + 6 + fontH + 2,
                                             rowRect.w - 16,
                                             labelColor);
            ++visibleIndex;
        }
    }
}
