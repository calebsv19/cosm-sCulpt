#include "UI/ui_panel_edit_layout.h"

#include "UI/ui_panel_edit_summary.h"
#include "UI/ui_panel_right_controls.h"

enum {
    UI_EDIT_PANE_SECTION_GAP = 8
};

void UIPanel_UpdateEditPaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    int summary_height = 0;
    int selection_height = 0;
    int workspace_top = 0;
    int workspace_bottom = 0;

    if (!ui) return;

    ui->editPane.summaryRect = zero;
    ui->editPane.workspaceRect = zero;
    ui->editPane.selectionModeRect = zero;

    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_EDIT) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    summary_height = UIPanel_EditSummaryReservedHeight(ui);
    selection_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_EDIT_SELECT);

    ui->editPane.summaryRect = (SDL_Rect){
        ui->rightBodyRect.x,
        ui->rightBodyRect.y,
        ui->rightBodyRect.w,
        summary_height
    };
    ui->editPane.selectionModeRect = (SDL_Rect){
        ui->rightBodyRect.x,
        ui->rightBodyRect.y + ui->rightBodyRect.h - selection_height,
        ui->rightBodyRect.w,
        selection_height
    };

    workspace_top = ui->rightBodyRect.y + summary_height;
    if (summary_height > 0) workspace_top += UI_EDIT_PANE_SECTION_GAP;
    workspace_bottom = ui->editPane.selectionModeRect.y - UI_EDIT_PANE_SECTION_GAP;
    if (workspace_bottom < workspace_top) workspace_bottom = workspace_top;
    ui->editPane.workspaceRect = (SDL_Rect){
        ui->rightBodyRect.x,
        workspace_top,
        ui->rightBodyRect.w,
        workspace_bottom - workspace_top
    };
}

bool UIPanel_GetEditPaneRects(const UIPanelState* ui,
                              SDL_Rect* out_summary_rect,
                              SDL_Rect* out_workspace_rect,
                              SDL_Rect* out_selection_mode_rect) {
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_EDIT) return false;
    if (out_summary_rect) *out_summary_rect = ui->editPane.summaryRect;
    if (out_workspace_rect) *out_workspace_rect = ui->editPane.workspaceRect;
    if (out_selection_mode_rect) *out_selection_mode_rect = ui->editPane.selectionModeRect;
    return true;
}
