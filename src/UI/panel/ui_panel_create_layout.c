#include "UI/ui_panel_create_layout.h"

#include "Core/global_state.h"
#include "UI/ui_panel_right_controls.h"
#include "UI/ui_panel_create_summary.h"
#include "UI/ui_panel_right_scroll.h"

enum { UI_CREATE_PANE_SECTION_GAP = 8 };

void UIPanel_UpdateCreatePaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    int summary_height = 0;
    int primitives_height = 0;
    int operations_height = 0;
    int construction_height = 0;
    int categories_height = 0;
    int workspace_height = 0;
    int cursor_y = 0;
    int content_height = 0;
    int scroll_offset = 0;
    SDL_Rect content_rect = {0, 0, 0, 0};

    if (!ui) return;

    ui->createPane.summaryRect = zero;
    ui->createPane.workspaceRect = zero;
    ui->createPane.primitivesRect = zero;
    ui->createPane.operationsRect = zero;
    ui->createPane.constructionRect = zero;
    ui->createPane.categoriesRect = zero;

    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_CREATE) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    content_rect = ui->rightBodyRect;
    if (!object_mode && content_rect.w > 12) content_rect.w -= 12;
    summary_height = UIPanel_CreateSummaryReservedHeight(ui);
    primitives_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_PRIMITIVES);
    operations_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_OPERATIONS);
    construction_height = object_mode
        ? 0
        : UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_CONSTRUCTION);
    categories_height = object_mode
        ? 0
        : UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_CREATE_CATEGORIES);
    workspace_height = object_mode ? 0 :
        metrics.group_header_height_px + (metrics.button_height_px * 3) +
        (metrics.button_spacing_px * 2);

    if (object_mode) {
        ui->createPane.summaryRect = (SDL_Rect){content_rect.x, content_rect.y, content_rect.w, summary_height};
        ui->createPane.operationsRect = (SDL_Rect){
            content_rect.x,
            content_rect.y + content_rect.h - operations_height,
            content_rect.w,
            operations_height
        };
        ui->createPane.primitivesRect = (SDL_Rect){
            ui->rightBodyRect.x,
            ui->createPane.operationsRect.y - UI_CREATE_PANE_SECTION_GAP - primitives_height,
            content_rect.w,
            primitives_height
        };
        return;
    }

    cursor_y = content_rect.y;
    ui->createPane.summaryRect = (SDL_Rect){content_rect.x, cursor_y, content_rect.w, summary_height};
    cursor_y += summary_height + UI_CREATE_PANE_SECTION_GAP;
    ui->createPane.workspaceRect = (SDL_Rect){content_rect.x, cursor_y, content_rect.w, workspace_height};
    cursor_y += workspace_height + UI_CREATE_PANE_SECTION_GAP;
    ui->createPane.categoriesRect = (SDL_Rect){content_rect.x, cursor_y, content_rect.w, categories_height};
    cursor_y += categories_height + UI_CREATE_PANE_SECTION_GAP;

    switch (ui->createCategory) {
        case UI_CREATE_CATEGORY_GEOMETRY:
            ui->createPane.primitivesRect = (SDL_Rect){content_rect.x, cursor_y, content_rect.w, primitives_height};
            cursor_y += primitives_height;
            break;
        case UI_CREATE_CATEGORY_PATHS:
        case UI_CREATE_CATEGORY_LIGHTING:
        case UI_CREATE_CATEGORY_MATERIALS:
            ui->createPane.operationsRect = (SDL_Rect){content_rect.x, cursor_y, content_rect.w, operations_height};
            cursor_y += operations_height;
            break;
        case UI_CREATE_CATEGORY_CONSTRUCTION:
            ui->createPane.constructionRect = (SDL_Rect){content_rect.x, cursor_y, content_rect.w, construction_height};
            cursor_y += construction_height;
            break;
        default:
            break;
    }
    content_height = cursor_y - content_rect.y;
    UIPanel_RightScrollSetContentHeight(ui, (float)content_height);
    scroll_offset = (int)UIPanel_RightScrollOffset(ui);
    ui->createPane.summaryRect.y -= scroll_offset;
    ui->createPane.workspaceRect.y -= scroll_offset;
    ui->createPane.categoriesRect.y -= scroll_offset;
    ui->createPane.primitivesRect.y -= scroll_offset;
    ui->createPane.operationsRect.y -= scroll_offset;
    ui->createPane.constructionRect.y -= scroll_offset;
}

bool UIPanel_GetCreatePaneRects(const UIPanelState* ui,
                                SDL_Rect* out_summary_rect,
                                SDL_Rect* out_workspace_rect,
                                SDL_Rect* out_primitives_rect,
                                SDL_Rect* out_operations_rect,
                                SDL_Rect* out_construction_rect) {
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_CREATE) return false;
    if (out_summary_rect) *out_summary_rect = ui->createPane.summaryRect;
    if (out_workspace_rect) *out_workspace_rect = ui->createPane.workspaceRect;
    if (out_primitives_rect) *out_primitives_rect = ui->createPane.primitivesRect;
    if (out_operations_rect) *out_operations_rect = ui->createPane.operationsRect;
    if (out_construction_rect) *out_construction_rect = ui->createPane.constructionRect;
    return true;
}
