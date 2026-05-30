#include "UI/ui_panel_create_layout.h"

#include "Core/global_state.h"
#include "UI/ui_panel_right_controls.h"
#include "UI/ui_panel_create_summary.h"

enum {
    UI_CREATE_PANE_SECTION_GAP = 8
};

void UIPanel_UpdateCreatePaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    int summary_height = 0;
    int primitives_height = 0;
    int operations_height = 0;
    int construction_height = 0;
    int workspace_top = 0;
    int workspace_bottom = 0;

    if (!ui) return;

    ui->createPane.summaryRect = zero;
    ui->createPane.workspaceRect = zero;
    ui->createPane.primitivesRect = zero;
    ui->createPane.operationsRect = zero;
    ui->createPane.constructionRect = zero;

    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_CREATE) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    summary_height = UIPanel_CreateSummaryReservedHeight(ui);
    primitives_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_PRIMITIVES);
    operations_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_OPERATIONS);
    construction_height = object_mode
        ? 0
        : UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_CONSTRUCTION);

    ui->createPane.summaryRect = (SDL_Rect){
        ui->rightBodyRect.x,
        ui->rightBodyRect.y,
        ui->rightBodyRect.w,
        summary_height
    };

    if (object_mode) {
        ui->createPane.operationsRect = (SDL_Rect){
            ui->rightBodyRect.x,
            ui->rightBodyRect.y + ui->rightBodyRect.h - operations_height,
            ui->rightBodyRect.w,
            operations_height
        };
        ui->createPane.primitivesRect = (SDL_Rect){
            ui->rightBodyRect.x,
            ui->createPane.operationsRect.y - UI_CREATE_PANE_SECTION_GAP - primitives_height,
            ui->rightBodyRect.w,
            primitives_height
        };
    } else {
        ui->createPane.constructionRect = (SDL_Rect){
            ui->rightBodyRect.x,
            ui->rightBodyRect.y + ui->rightBodyRect.h - construction_height,
            ui->rightBodyRect.w,
            construction_height
        };
        ui->createPane.primitivesRect = (SDL_Rect){
            ui->rightBodyRect.x,
            ui->createPane.constructionRect.y - UI_CREATE_PANE_SECTION_GAP - primitives_height,
            ui->rightBodyRect.w,
            primitives_height
        };
    }
    if (ui->createPane.primitivesRect.y < ui->rightBodyRect.y) {
        ui->createPane.primitivesRect.y = ui->rightBodyRect.y;
    }

    workspace_top = ui->rightBodyRect.y + summary_height;
    if (summary_height > 0) workspace_top += UI_CREATE_PANE_SECTION_GAP;
    workspace_bottom = ui->createPane.primitivesRect.y - UI_CREATE_PANE_SECTION_GAP;
    if (workspace_bottom < workspace_top) workspace_bottom = workspace_top;
    ui->createPane.workspaceRect = (SDL_Rect){
        ui->rightBodyRect.x,
        workspace_top,
        ui->rightBodyRect.w,
        workspace_bottom - workspace_top
    };
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
