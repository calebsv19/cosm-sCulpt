#include "UI/ui_panel_scene_list.h"

#include "UI/font_manager.h"
#include "UI/ui_panel_scene_layout.h"
#include "UI/ui_panel_scene_summary.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Editor/scene_authoring_path_handles.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

enum {
    UI_SCENE_LIST_DOUBLE_CLICK_MS = 350,
    UI_SCENE_LIST_CONTENT_INSET = 3,
    UI_SCENE_LIST_SCROLLBAR_W = 6,
    UI_SCENE_LIST_SCROLLBAR_GUTTER = 5,
    UI_SCENE_LIST_SCROLLBAR_MIN_THUMB_H = 12
};

static int UIPanelSceneList_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelSceneList_RowHeight(void) {
    int row = (UIPanelSceneList_FontHeight() * 3) + 16;
    if (row < 50) row = 50;
    return row;
}

static int UIPanelSceneList_ExpandedExtraHeight(void) {
    int extra = (UIPanelSceneList_FontHeight() * 4) + 18;
    if (extra < 70) extra = 70;
    return extra;
}

static int UIPanelSceneList_RowGap(void) {
    return 4;
}

static bool UIPanelSceneList_IsExpanded(const UIPanelState* ui, uint32_t objectId) {
    return ui && objectId != 0u && ui->sceneList.expandedObjectId == objectId;
}

static int UIPanelSceneList_RowHeightForObject(const UIPanelState* ui, const Object3D* object) {
    int row = UIPanelSceneList_RowHeight();
    if (object && UIPanelSceneList_IsExpanded(ui, object->objectId)) {
        row += UIPanelSceneList_ExpandedExtraHeight();
    }
    return row;
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

typedef enum {
    UI_SCENE_LIST_ROW_NONE = 0,
    UI_SCENE_LIST_ROW_OBJECT = 1,
    UI_SCENE_LIST_ROW_LIGHT = 2,
    UI_SCENE_LIST_ROW_PATH = 3,
    UI_SCENE_LIST_ROW_MATERIAL = 4
} UIPanelSceneListRowKind;

typedef struct {
    UIPanelSceneListRowKind kind;
    size_t index;
    const Object3D* object;
} UIPanelSceneListRow;

static void UIPanelSceneList_ClampScroll(UIPanelState* ui, const Layout* layout);
static bool UIPanelSceneList_HasScrollbar(const UIPanelState* ui, const Layout* layout);
static SDL_Rect UIPanelSceneList_ScrollTrackRect(const UIPanelState* ui);

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

static size_t UIPanelSceneList_TotalRowCount(const Layout* layout) {
    if (!layout) return 0u;
    return UIPanelSceneList_LiveObjectCount(&layout->objectStore) +
           layout->sceneAuthoring.light_count +
           layout->sceneAuthoring.path_count +
           layout->sceneAuthoring.material_count;
}

static UIPanelSceneListRow UIPanelSceneList_RowAtVisibleIndex(const Layout* layout, int visibleIndex) {
    UIPanelSceneListRow row = { UI_SCENE_LIST_ROW_NONE, 0u, NULL };
    const size_t live_objects = layout ? UIPanelSceneList_LiveObjectCount(&layout->objectStore) : 0u;
    size_t offset = 0u;
    if (!layout || visibleIndex < 0) return row;
    if ((size_t)visibleIndex < live_objects) {
        row.kind = UI_SCENE_LIST_ROW_OBJECT;
        row.index = (size_t)visibleIndex;
        row.object = UIPanelSceneList_ObjectAtVisibleIndex(&layout->objectStore, visibleIndex);
        return row;
    }

    offset = (size_t)visibleIndex - live_objects;
    if (offset < layout->sceneAuthoring.light_count) {
        row.kind = UI_SCENE_LIST_ROW_LIGHT;
        row.index = offset;
        return row;
    }
    offset -= layout->sceneAuthoring.light_count;
    if (offset < layout->sceneAuthoring.path_count) {
        row.kind = UI_SCENE_LIST_ROW_PATH;
        row.index = offset;
        return row;
    }
    offset -= layout->sceneAuthoring.path_count;
    if (offset < layout->sceneAuthoring.material_count) {
        row.kind = UI_SCENE_LIST_ROW_MATERIAL;
        row.index = offset;
        return row;
    }
    return row;
}

static int UIPanelSceneList_RowHeightForRow(const UIPanelState* ui, UIPanelSceneListRow row) {
    if (row.kind == UI_SCENE_LIST_ROW_OBJECT) {
        return UIPanelSceneList_RowHeightForObject(ui, row.object);
    }
    return UIPanelSceneList_RowHeight();
}

static SDL_Rect UIPanelSceneList_ListRect(const UIPanelState* ui) {
    SDL_Rect rect = {0, 0, 0, 0};
    if (!ui) return rect;
    (void)UIPanel_GetScenePaneRects(ui, NULL, &rect, NULL, NULL);
    return rect;
}

static SDL_Rect UIPanelSceneList_ContentClipRect(const UIPanelState* ui,
                                                 const Layout* layout) {
    SDL_Rect clip = UIPanelSceneList_ListRect(ui);
    if (clip.w <= 0 || clip.h <= 0) return clip;
    clip.x += UI_SCENE_LIST_CONTENT_INSET;
    clip.y += UI_SCENE_LIST_CONTENT_INSET;
    clip.w -= UI_SCENE_LIST_CONTENT_INSET * 2;
    clip.h -= UI_SCENE_LIST_CONTENT_INSET * 2;
    if (UIPanelSceneList_HasScrollbar(ui, layout)) {
        SDL_Rect track = UIPanelSceneList_ScrollTrackRect(ui);
        clip.w = (track.x - UI_SCENE_LIST_SCROLLBAR_GUTTER) - clip.x;
    }
    if (clip.w < 0) clip.w = 0;
    if (clip.h < 0) clip.h = 0;
    return clip;
}

static float UIPanelSceneList_MaxScrollOffset(const UIPanelState* ui, const Layout* layout) {
    SDL_Rect listRect = UIPanelSceneList_ListRect(ui);
    const size_t rowCount = UIPanelSceneList_TotalRowCount(layout);
    const int rowGap = UIPanelSceneList_RowGap();
    float totalHeight = 0.0f;
    if (rowCount == 0u) return 0.0f;
    for (int i = 0;; ++i) {
        UIPanelSceneListRow row = UIPanelSceneList_RowAtVisibleIndex(layout, i);
        if (row.kind == UI_SCENE_LIST_ROW_NONE) break;
        totalHeight += (float)UIPanelSceneList_RowHeightForRow(ui, row);
        totalHeight += (float)rowGap;
    }
    if (totalHeight > 0.0f) {
        totalHeight -= (float)rowGap;
    }
    if (totalHeight <= (float)listRect.h) return 0.0f;
    return totalHeight - (float)listRect.h;
}

static bool UIPanelSceneList_HasScrollbar(const UIPanelState* ui, const Layout* layout) {
    return UIPanelSceneList_MaxScrollOffset(ui, layout) > 0.5f;
}

static SDL_Rect UIPanelSceneList_ScrollTrackRect(const UIPanelState* ui) {
    SDL_Rect listRect = UIPanelSceneList_ListRect(ui);
    SDL_Rect track = {
        listRect.x + listRect.w - UI_SCENE_LIST_CONTENT_INSET - UI_SCENE_LIST_SCROLLBAR_W,
        listRect.y + UI_SCENE_LIST_CONTENT_INSET,
        UI_SCENE_LIST_SCROLLBAR_W,
        listRect.h - (UI_SCENE_LIST_CONTENT_INSET * 2)
    };
    if (track.h < 0) track.h = 0;
    return track;
}

static SDL_Rect UIPanelSceneList_ScrollThumbRect(const UIPanelState* ui,
                                                 const Layout* layout) {
    SDL_Rect track = UIPanelSceneList_ScrollTrackRect(ui);
    SDL_Rect thumb = track;
    const size_t rowCount = UIPanelSceneList_TotalRowCount(layout);
    const int rowGap = UIPanelSceneList_RowGap();
    float contentHeight = 0.0f;
    float ratio = 1.0f;
    float thumbH = 0.0f;
    float maxScroll = 0.0f;
    float travel = 0.0f;
    float offsetRatio = 0.0f;

    if (!ui || !layout || !UIPanelSceneList_HasScrollbar(ui, layout) || track.h <= 0 || rowCount == 0u) {
        thumb.h = 0;
        return thumb;
    }

    for (int i = 0;; ++i) {
        UIPanelSceneListRow row = UIPanelSceneList_RowAtVisibleIndex(layout, i);
        if (row.kind == UI_SCENE_LIST_ROW_NONE) break;
        contentHeight += (float)UIPanelSceneList_RowHeightForRow(ui, row);
        contentHeight += (float)rowGap;
    }
    if (contentHeight > 0.0f) {
        contentHeight -= (float)rowGap;
    }
    if (contentHeight <= 0.0f) {
        thumb.h = 0;
        return thumb;
    }

    ratio = (float)track.h / contentHeight;
    if (ratio > 1.0f) ratio = 1.0f;
    thumbH = ratio * (float)track.h;
    if (thumbH < (float)UI_SCENE_LIST_SCROLLBAR_MIN_THUMB_H) {
        thumbH = (float)UI_SCENE_LIST_SCROLLBAR_MIN_THUMB_H;
    }
    if (thumbH > (float)track.h) {
        thumbH = (float)track.h;
    }
    thumb.h = (int)thumbH;

    maxScroll = UIPanelSceneList_MaxScrollOffset(ui, layout);
    travel = (float)track.h - thumbH;
    if (travel <= 0.0f || maxScroll <= 0.0f) return thumb;

    offsetRatio = ui->sceneList.scrollOffsetPx / maxScroll;
    if (offsetRatio < 0.0f) offsetRatio = 0.0f;
    if (offsetRatio > 1.0f) offsetRatio = 1.0f;
    thumb.y = track.y + (int)(offsetRatio * travel);
    return thumb;
}

static bool UIPanelSceneList_ScrollbarDragTo(UIPanelState* ui,
                                             const Layout* layout,
                                             int mouseY) {
    SDL_Rect track = UIPanelSceneList_ScrollTrackRect(ui);
    SDL_Rect thumb = UIPanelSceneList_ScrollThumbRect(ui, layout);
    float maxScroll = 0.0f;
    float usableH = 0.0f;
    float delta = 0.0f;
    if (!ui || !layout || thumb.h <= 0) return false;
    maxScroll = UIPanelSceneList_MaxScrollOffset(ui, layout);
    usableH = (float)(track.h - thumb.h);
    if (usableH <= 0.0f || maxScroll <= 0.0f) return false;
    delta = (float)(mouseY - ui->sceneList.scrollbarDragStartY);
    ui->sceneList.scrollOffsetPx =
        ui->sceneList.scrollbarDragStartOffsetPx + ((delta / usableH) * maxScroll);
    UIPanelSceneList_ClampScroll(ui, layout);
    return true;
}

static bool UIPanelSceneList_JumpScrollbarTo(UIPanelState* ui,
                                             const Layout* layout,
                                             int mouseY) {
    SDL_Rect track = UIPanelSceneList_ScrollTrackRect(ui);
    SDL_Rect thumb = UIPanelSceneList_ScrollThumbRect(ui, layout);
    float maxScroll = 0.0f;
    float usableH = 0.0f;
    float ratio = 0.0f;
    int relY = 0;
    if (!ui || !layout || thumb.h <= 0) return false;
    maxScroll = UIPanelSceneList_MaxScrollOffset(ui, layout);
    usableH = (float)(track.h - thumb.h);
    if (usableH <= 0.0f || maxScroll <= 0.0f) return false;
    relY = mouseY - track.y - (thumb.h / 2);
    ratio = (float)relY / usableH;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    ui->sceneList.scrollOffsetPx = ratio * maxScroll;
    UIPanelSceneList_ClampScroll(ui, layout);
    return true;
}

static void UIPanelSceneList_ClampScroll(UIPanelState* ui, const Layout* layout) {
    float maxOffset = 0.0f;
    if (!ui) return;
    maxOffset = UIPanelSceneList_MaxScrollOffset(ui, layout);
    if (ui->sceneList.scrollOffsetPx < 0.0f) ui->sceneList.scrollOffsetPx = 0.0f;
    if (ui->sceneList.scrollOffsetPx > maxOffset) ui->sceneList.scrollOffsetPx = maxOffset;
}

static int UIPanelSceneList_RowIndexAtPoint(const UIPanelState* ui,
                                            const Layout* layout,
                                            int mouseX,
                                            int mouseY) {
    SDL_Rect contentClip = UIPanelSceneList_ContentClipRect(ui, layout);
    const int rowGap = UIPanelSceneList_RowGap();
    float contentY = 0.0f;
    float cursorY = 0.0f;
    if (!ui || !layout) return -1;
    if (!UIPanelSceneList_PointInRect(mouseX, mouseY, contentClip)) return -1;
    contentY = (float)(mouseY - contentClip.y) + ui->sceneList.scrollOffsetPx;
    for (int rowIndex = 0;; ++rowIndex) {
        UIPanelSceneListRow row = UIPanelSceneList_RowAtVisibleIndex(layout, rowIndex);
        float rowH = 0.0f;
        if (row.kind == UI_SCENE_LIST_ROW_NONE) break;
        rowH = (float)UIPanelSceneList_RowHeightForRow(ui, row);
        if (contentY >= cursorY && contentY <= cursorY + rowH) {
            return rowIndex;
        }
        cursorY += rowH + (float)rowGap;
    }
    return -1;
}

static void UIPanelSceneList_SelectObject(uint32_t objectId) {
    GlobalState* state = Global_Get();
    EditorState* editor = NULL;
    if (!state || objectId == 0u) return;
    Layout_SceneAuthoringState_ClearSelection(&state->layout.sceneAuthoring);
    editor = &state->editor;
    Editor_ClearSceneAuthoringEditMode(editor);
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

static void UIPanelSceneList_SelectAuthoringEntity(LineDrawingSceneAuthoringSelectionKind kind,
                                                   size_t index) {
    GlobalState* state = Global_Get();
    EditorState* editor = NULL;
    if (!state) return;
    if (!Layout_SceneAuthoringState_Select(&state->layout.sceneAuthoring, kind, index)) return;
    editor = &state->editor;
    Editor_ClearSceneAuthoringEditMode(editor);
    Editor_ClearAnchorSelection(editor);
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    editor->selectedWallIndex = -1;
    editor->selectedHandleAnchor = -1;
    editor->selectedHandleComponent = -1;
    editor->hoveredObject3DId = 0u;
    Global_FlagHitboxesDirty();
}

static bool UIPanelSceneList_IsDoubleClick(UIPanelState* ui, uint32_t objectId, Uint32 now_ticks) {
    if (!ui || objectId == 0u) return false;
    return ui->sceneList.lastClickedObjectId == objectId &&
           now_ticks >= ui->sceneList.lastClickTicks &&
           (now_ticks - ui->sceneList.lastClickTicks) <= UI_SCENE_LIST_DOUBLE_CLICK_MS;
}

static void UIPanelSceneList_RememberClick(UIPanelState* ui, uint32_t objectId, Uint32 now_ticks) {
    if (!ui) return;
    ui->sceneList.lastClickedObjectId = objectId;
    ui->sceneList.lastClickTicks = now_ticks;
}

static void UIPanelSceneList_ClearClickMemory(UIPanelState* ui) {
    if (!ui) return;
    ui->sceneList.lastClickedObjectId = 0u;
    ui->sceneList.lastClickTicks = 0u;
}

void UIPanel_SceneListClearSelection(void) {
    GlobalState* state = Global_Get();
    UIPanelState* ui = UIPanel_Get();
    EditorState* editor = NULL;
    if (!state) return;
    editor = &state->editor;
    Editor_ClearSceneAuthoringEditMode(editor);
    editor->selectedAnchorIndex = -1;
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    editor->selectedWallIndex = -1;
    editor->selectedHandleAnchor = -1;
    editor->selectedHandleComponent = -1;
    editor->hoveredObject3DId = 0u;
    editor->hoveredSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    editor->hoveredSceneBoundsGizmoAxis = -1;
    editor->activeSceneBoundsGizmoAxis = -1;
    editor->isResizingSceneBounds = false;
    Layout_SceneAuthoringState_ClearSelection(&state->layout.sceneAuthoring);
    if (ui) {
        ui->sceneList.expandedObjectId = 0u;
        UIPanelSceneList_ClearClickMemory(ui);
    }
    Global_FlagHitboxesDirty();
}

bool UIPanel_SceneListDeleteSelectedObject(void) {
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = NULL;
    uint32_t selected_id = 0u;
    if (!state) return false;
    authoring = &state->layout.sceneAuthoring;
    if (authoring->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE) {
        if (state->editor.selectedSceneAuthoringPathElementKind !=
            LINE_DRAWING_SCENE_PATH_ELEMENT_NONE) {
            return SceneAuthoringPathHandles_DeleteSelectedControlPoint(
                state, &state->editor);
        }
        Editor_HistoryCapture(&state->editor, &state->layout);
        if (!Layout_SceneAuthoringState_DeleteSelected(authoring)) {
            return false;
        }
        Editor_ClearSceneAuthoringEditMode(&state->editor);
        state->editor.selectedSceneAuthoringPathIndex = -1;
        state->editor.selectedSceneAuthoringControlPointIndex = -1;
        state->editor.selectedSceneAuthoringPathElementKind =
            LINE_DRAWING_SCENE_PATH_ELEMENT_NONE;
        state->editor.selectedSceneAuthoringPathSegmentIndex = -1;
        state->editor.selectedSceneAuthoringLightPosition = false;
        state->editor.selectedSceneAuthoringLightAim = false;
        state->editor.selectedSceneAuthoringCameraAim = false;
        Global_FlagLayoutChanged();
        Global_FlagHitboxesDirty();
        return true;
    }
    selected_id = state->editor.selectedObject3DId;
    if (selected_id == 0u) return false;
    Editor_HistoryCapture(&state->editor, &state->layout);
    if (!Layout_ObjectStore_Delete(&state->layout.objectStore, selected_id)) {
        return false;
    }
    UIPanel_SceneListClearSelection();
    return true;
}

bool UIPanel_HandleSceneListClick(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    SDL_Rect listRect = {0};
    int rowIndex = -1;
    const Object3D* object = NULL;
    Uint32 now_ticks = 0u;
    if (!ui || !state) return false;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return false;
    now_ticks = SDL_GetTicks();
    listRect = UIPanelSceneList_ListRect(ui);
    if (!UIPanelSceneList_PointInRect(mouseX, mouseY, listRect)) return false;

    if (UIPanelSceneList_HasScrollbar(ui, &state->layout)) {
        SDL_Rect track = UIPanelSceneList_ScrollTrackRect(ui);
        SDL_Rect thumb = UIPanelSceneList_ScrollThumbRect(ui, &state->layout);
        if (SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &track)) {
            if (SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &thumb)) {
                ui->sceneList.scrollbarDragging = true;
                ui->sceneList.scrollbarDragStartY = mouseY;
                ui->sceneList.scrollbarDragStartOffsetPx = ui->sceneList.scrollOffsetPx;
            } else {
                (void)UIPanelSceneList_JumpScrollbarTo(ui, &state->layout, mouseY);
            }
            return true;
        }
    }

    rowIndex = UIPanelSceneList_RowIndexAtPoint(ui, &state->layout, mouseX, mouseY);
    if (rowIndex >= 0) {
        UIPanelSceneListRow row = UIPanelSceneList_RowAtVisibleIndex(&state->layout, rowIndex);
        object = row.object;
        if (row.kind == UI_SCENE_LIST_ROW_OBJECT && object) {
            const bool is_double_click =
                UIPanelSceneList_IsDoubleClick(ui, object->objectId, now_ticks);
            UIPanelSceneList_SelectObject(object->objectId);
            if (is_double_click) {
                if (ui->sceneList.expandedObjectId == object->objectId) {
                    ui->sceneList.expandedObjectId = 0u;
                } else {
                    ui->sceneList.expandedObjectId = object->objectId;
                }
                UIPanelSceneList_ClearClickMemory(ui);
            } else {
                UIPanelSceneList_RememberClick(ui, object->objectId, now_ticks);
            }
        } else if (row.kind == UI_SCENE_LIST_ROW_LIGHT) {
            UIPanelSceneList_SelectAuthoringEntity(LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                   row.index);
            UIPanelSceneList_ClearClickMemory(ui);
        } else if (row.kind == UI_SCENE_LIST_ROW_PATH) {
            UIPanelSceneList_SelectAuthoringEntity(LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                   row.index);
            UIPanelSceneList_ClearClickMemory(ui);
        } else if (row.kind == UI_SCENE_LIST_ROW_MATERIAL) {
            UIPanelSceneList_SelectAuthoringEntity(LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL,
                                                   row.index);
            UIPanelSceneList_ClearClickMemory(ui);
        }
    } else {
        UIPanelSceneList_ClearClickMemory(ui);
    }
    return true;
}

bool UIPanel_HandleSceneListWheel(int mouseX, int mouseY, float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    SDL_Rect listRect = {0};
    if (!ui || !state) return false;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return false;
    listRect = UIPanelSceneList_ListRect(ui);
    if (!UIPanelSceneList_PointInRect(mouseX, mouseY, listRect)) return false;
    ui->sceneList.scrollOffsetPx -= wheel_delta * (float)(UIPanelSceneList_RowHeight() * 2);
    UIPanelSceneList_ClampScroll(ui, &state->layout);
    return true;
}

void UIPanel_HandleSceneListMouseUp(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    ui->sceneList.scrollbarDragging = false;
}

void UIPanel_HandleSceneListMouseMotion(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    if (!ui || !state || ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) {
        if (ui) ui->sceneList.hoverIndex = -1;
        return;
    }
    if (ui->sceneList.scrollbarDragging) {
        (void)UIPanelSceneList_ScrollbarDragTo(ui, &state->layout, mouseY);
    }
    ui->sceneList.hoverIndex = UIPanelSceneList_RowIndexAtPoint(ui, &state->layout, mouseX, mouseY);
}

void Render_UIPanelSceneList(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    UIPanelVisualPalette palette = {0};
    SDL_Color labelColor = {200, 200, 210, 255};
    SDL_Color valueColor = {230, 230, 235, 255};
    SDL_Color accentColor = {140, 170, 210, 255};
    SDL_Color rowFill = {34, 37, 42, 220};
    SDL_Color rowBorder = {80, 92, 110, 230};
    SDL_Color selectedFill = {82, 104, 132, 235};
    SDL_Color selectedBorder = {170, 188, 214, 255};
    SDL_Color hoverFill = {48, 54, 62, 230};
    SDL_Color scrollbarTrackColor = {30, 33, 39, 170};
    SDL_Color scrollbarThumbColor = {120, 134, 158, 235};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Rect listRect = {0};
    int fontH = 0;
    size_t rowCount = 0u;
    SDL_Rect contentClip = {0};
    SDL_Rect previousClip = {0};
    SDL_bool hadClip = SDL_FALSE;

    if (!ui || !renderer || !state || !font) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;
    if (ui->leftBodyRect.w <= 0 || ui->leftBodyRect.h <= 0) return;

    if (UIPanelVisual_ResolvePalette(&palette)) {
        labelColor = palette.text_muted;
        valueColor = palette.text_primary;
        accentColor = palette.accent;
        rowFill = palette.workspace_fill;
        rowBorder = palette.pane_border;
        selectedFill = palette.button_fill;
        selectedBorder = palette.button_border;
        hoverFill = palette.button_fill;
        hoverFill.a = 180;
        scrollbarTrackColor = palette.workspace_fill;
        scrollbarTrackColor.a = 180;
        scrollbarThumbColor = palette.button_border;
    }

    fontH = UIPanelSceneList_FontHeight();
    rowCount = UIPanelSceneList_TotalRowCount(&state->layout);

    listRect = UIPanelSceneList_ListRect(ui);
    contentClip = UIPanelSceneList_ContentClipRect(ui, &state->layout);
    if (listRect.w > 0 && listRect.h > 0) {
        SDL_Rect listInner = listRect;
        UIPanelSummary_DrawCard(renderer,
                                listRect,
                                rowFill,
                                rowBorder,
                                accentColor,
                                0);
        listInner.x += 1;
        listInner.y += 1;
        listInner.w -= 2;
        listInner.h -= 2;
        if (listInner.w > 0 && listInner.h > 0) {
            SDL_SetRenderDrawColor(renderer, rowBorder.r, rowBorder.g, rowBorder.b, 60);
            SDL_RenderDrawRect(renderer, &listInner);
        }
    }

    if (rowCount == 0u) {
        UIPanelSummary_DrawTextClipped(renderer,
                                       font,
                                       "No scene authoring entities yet.",
                                       contentClip.x + metrics.pad_x,
                                       contentClip.y + metrics.pad_y,
                                       contentClip.w - (metrics.pad_x * 2),
                                       fontH + 4,
                                       labelColor);
        return;
    }

    hadClip = SDL_RenderIsClipEnabled(renderer);
    if (hadClip == SDL_TRUE) {
        SDL_RenderGetClipRect(renderer, &previousClip);
    }
    if (contentClip.w > 0 && contentClip.h > 0) {
        SDL_RenderSetClipRect(renderer, &contentClip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }

    {
        const int rowGap = UIPanelSceneList_RowGap();
        const int contentOffset = (int)ui->sceneList.scrollOffsetPx;
        const int clipBottom = contentClip.y + contentClip.h;
        int visibleIndex = 0;
        int cursorY = contentClip.y - contentOffset;
        for (;;) {
            UIPanelSceneListRow row = UIPanelSceneList_RowAtVisibleIndex(&state->layout, visibleIndex);
            const Object3D* object = row.object;
            SDL_Rect rowRect = {0};
            char line0[96];
            char line1[160];
            char line2[192];
            int rowH = 0;
            bool isExpanded = false;
            bool isSelected = false;
            bool isHovered = false;
            if (row.kind == UI_SCENE_LIST_ROW_NONE) break;
            rowH = UIPanelSceneList_RowHeightForRow(ui, row);
            isExpanded = object && UIPanelSceneList_IsExpanded(ui, object->objectId);
            rowRect.x = contentClip.x;
            rowRect.y = cursorY;
            rowRect.w = contentClip.w;
            rowRect.h = rowH;
            if (rowRect.y + rowRect.h < contentClip.y) {
                cursorY += rowH + rowGap;
                ++visibleIndex;
                continue;
            }
            if (rowRect.y > clipBottom) break;
            if (rowRect.w <= 0) break;

            {
                if (row.kind == UI_SCENE_LIST_ROW_OBJECT && object) {
                    isSelected = (state->editor.selectedObject3DId == object->objectId);
                } else if (row.kind == UI_SCENE_LIST_ROW_LIGHT) {
                    isSelected = state->layout.sceneAuthoring.selected_kind ==
                                     LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
                                 state->layout.sceneAuthoring.selected_index == row.index;
                } else if (row.kind == UI_SCENE_LIST_ROW_PATH) {
                    isSelected = state->layout.sceneAuthoring.selected_kind ==
                                     LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH &&
                                 state->layout.sceneAuthoring.selected_index == row.index;
                } else if (row.kind == UI_SCENE_LIST_ROW_MATERIAL) {
                    isSelected = state->layout.sceneAuthoring.selected_kind ==
                                     LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL &&
                                 state->layout.sceneAuthoring.selected_index == row.index;
                }
                isHovered = (ui->sceneList.hoverIndex == visibleIndex);
                SDL_Color fill = isSelected ? selectedFill : (isHovered ? hoverFill : rowFill);
                SDL_Color border = isSelected ? selectedBorder : rowBorder;
                SDL_Color accent = isSelected ? selectedBorder : accentColor;
                UIPanelVisual_DrawInteractiveRow(renderer,
                                                 rowRect,
                                                 fill,
                                                 border,
                                                 accent,
                                                 isHovered,
                                                 isSelected,
                                                 4,
                                                 60);
            }

            if (row.kind == UI_SCENE_LIST_ROW_OBJECT && object) {
                snprintf(line0,
                         sizeof(line0),
                         "%s #%u  %s",
                         isExpanded ? "v" : ">",
                         object->objectId,
                         object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE
                             ? "Mesh"
                             : (object->kind == OBJECT3D_KIND_RECT_PRISM ? "Prism" : "Plane"));
                snprintf(line1,
                         sizeof(line1),
                         "Pos %.1f, %.1f, %.1f",
                         object->transform.position.x,
                         object->transform.position.y,
                         object->transform.position.z);
                if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
                    snprintf(line2,
                             sizeof(line2),
                             "Asset %s   %zu tris",
                             object->meshInstance.assetId,
                             object->meshInstance.triangleCount);
                } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
                    snprintf(line2,
                             sizeof(line2),
                             "Size %.1f x %.1f x %.1f   Locks P:%s B:%s",
                             object->rectPrism.width,
                             object->rectPrism.height,
                             object->rectPrism.depth,
                             object->rectPrism.lockToConstructionPlane ? "On" : "Off",
                             object->rectPrism.lockToBounds ? "On" : "Off");
                } else {
                    snprintf(line2,
                             sizeof(line2),
                             "Size %.1f x %.1f   Locks P:%s B:%s",
                             object->plane.width,
                             object->plane.height,
                             object->plane.lockToConstructionPlane ? "On" : "Off",
                             object->plane.lockToBounds ? "On" : "Off");
                }
            } else if (row.kind == UI_SCENE_LIST_ROW_LIGHT) {
                const LineDrawingSceneLight* light = &state->layout.sceneAuthoring.lights[row.index];
                snprintf(line0, sizeof(line0), "Light  %s", light->label);
                snprintf(line1,
                         sizeof(line1),
                         "%s   %s",
                         Layout_SceneLightKind_Label(light->kind),
                         light->enabled ? "Enabled" : "Disabled");
                snprintf(line2,
                         sizeof(line2),
                         "Pos %.1f, %.1f, %.1f   Path %s",
                         light->position.x,
                         light->position.y,
                         light->position.z,
                         light->path_id[0] ? light->path_id : "none");
            } else if (row.kind == UI_SCENE_LIST_ROW_PATH) {
                const LineDrawingScenePath* path =
                    &state->layout.sceneAuthoring.paths[row.index];
                snprintf(line0, sizeof(line0), "%s Path  %s",
                         Layout_ScenePathRole_Name(path->role), path->label);
                snprintf(line1,
                         sizeof(line1),
                         "Kind %s   %zu control points",
                         path->curve_type,
                         path->control_point_count);
                snprintf(line2,
                         sizeof(line2),
                         "Binding %s",
                         path->bound_camera_id[0] ? path->bound_camera_id :
                         path->bound_light_id[0] ? path->bound_light_id : "unbound");
            } else if (row.kind == UI_SCENE_LIST_ROW_MATERIAL) {
                const LineDrawingSceneMaterial* material =
                    &state->layout.sceneAuthoring.materials[row.index];
                snprintf(line0, sizeof(line0), "Material  %s", material->label);
                snprintf(line1, sizeof(line1), "Id %s", material->material_id);
                snprintf(line2,
                         sizeof(line2),
                         "RGBA %.2f, %.2f, %.2f, %.2f",
                         material->rgba[0],
                         material->rgba[1],
                         material->rgba[2],
                         material->rgba[3]);
            } else {
                snprintf(line0, sizeof(line0), "Scene Item");
                snprintf(line1, sizeof(line1), "Unavailable");
                snprintf(line2, sizeof(line2), "No details");
            }
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           line0,
                                           rowRect.x + metrics.pad_x,
                                           rowRect.y + metrics.pad_y - 1,
                                           rowRect.w - (metrics.pad_x * 2),
                                           fontH + 4,
                                           valueColor);
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           line1,
                                           rowRect.x + metrics.pad_x,
                                           rowRect.y + metrics.pad_y + fontH + 1,
                                           rowRect.w - (metrics.pad_x * 2),
                                           fontH + 4,
                                           labelColor);
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           line2,
                                           rowRect.x + metrics.pad_x,
                                           rowRect.y + metrics.pad_y + (fontH * 2) + 3,
                                           rowRect.w - (metrics.pad_x * 2),
                                           fontH + 4,
                                           labelColor);
            if (isExpanded && object) {
                char detail0[192];
                char detail1[192];
                char detail2[4096];
                int detailY = rowRect.y + UIPanelSceneList_RowHeight() + metrics.section_gap;
                SDL_Color detailColor = valueColor;
                SDL_Color detailMuted = labelColor;
                snprintf(detail0,
                         sizeof(detail0),
                         "Rotate %.1f, %.1f, %.1f deg   Scale %.1f, %.1f, %.1f",
                         object->transform.rotationDeg.x,
                         object->transform.rotationDeg.y,
                         object->transform.rotationDeg.z,
                         object->transform.scale.x,
                         object->transform.scale.y,
                         object->transform.scale.z);
                snprintf(detail1,
                         sizeof(detail1),
                         "Core id %u   Selected %s   Click row again to collapse.",
                         object->objectId,
                         (state->editor.selectedObject3DId == object->objectId) ? "yes" : "no");
                if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
                    snprintf(detail2,
                             sizeof(detail2),
                             "Runtime %s   Bounds %.1f..%.1f, %.1f..%.1f, %.1f..%.1f",
                             object->meshInstance.runtimePath,
                             object->meshInstance.localBoundsMin.x,
                             object->meshInstance.localBoundsMax.x,
                             object->meshInstance.localBoundsMin.y,
                             object->meshInstance.localBoundsMax.y,
                             object->meshInstance.localBoundsMin.z,
                             object->meshInstance.localBoundsMax.z);
                } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
                    snprintf(detail2,
                             sizeof(detail2),
                             "Frame origin %.1f, %.1f, %.1f   Construction lock %s   Bounds lock %s",
                             object->rectPrism.frame.origin.x,
                             object->rectPrism.frame.origin.y,
                             object->rectPrism.frame.origin.z,
                             object->rectPrism.lockToConstructionPlane ? "on" : "off",
                             object->rectPrism.lockToBounds ? "on" : "off");
                } else {
                    snprintf(detail2,
                             sizeof(detail2),
                             "Frame origin %.1f, %.1f, %.1f   Construction lock %s   Bounds lock %s",
                             object->plane.frame.origin.x,
                             object->plane.frame.origin.y,
                             object->plane.frame.origin.z,
                             object->plane.lockToConstructionPlane ? "on" : "off",
                             object->plane.lockToBounds ? "on" : "off");
                }
                UIPanelSummary_DrawTextClipped(renderer,
                                               font,
                                               detail0,
                                               rowRect.x + metrics.pad_x,
                                               detailY,
                                               rowRect.w - (metrics.pad_x * 2),
                                               fontH + 4,
                                               detailColor);
                UIPanelSummary_DrawTextClipped(renderer,
                                               font,
                                               detail1,
                                               rowRect.x + metrics.pad_x,
                                               detailY + fontH + 4,
                                               rowRect.w - (metrics.pad_x * 2),
                                               fontH + 4,
                                               detailMuted);
                UIPanelSummary_DrawTextClipped(renderer,
                                               font,
                                               detail2,
                                               rowRect.x + metrics.pad_x,
                                               detailY + (fontH * 2) + 8,
                                               rowRect.w - (metrics.pad_x * 2),
                                               fontH + 4,
                                               detailMuted);
            }
            cursorY += rowH + rowGap;
            ++visibleIndex;
        }
    }

    if (hadClip == SDL_TRUE) {
        SDL_RenderSetClipRect(renderer, &previousClip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }

    if (UIPanelSceneList_HasScrollbar(ui, &state->layout)) {
        SDL_Rect track = UIPanelSceneList_ScrollTrackRect(ui);
        SDL_Rect thumb = UIPanelSceneList_ScrollThumbRect(ui, &state->layout);
        if (track.w > 0 && track.h > 0 && thumb.h > 0) {
            SDL_Color thumbFill = scrollbarThumbColor;
            SDL_Color thumbBorder = selectedBorder;
            thumbFill.a = 220;
            thumbBorder.a = 235;
            UIPanelVisual_DrawScrollbar(renderer,
                                        track,
                                        thumb,
                                        scrollbarTrackColor,
                                        rowBorder,
                                        thumbFill,
                                        thumbBorder,
                                        ui->sceneList.scrollbarDragging);
        }
    }
}
