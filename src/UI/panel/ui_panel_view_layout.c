#include "UI/ui_panel_section.h"
#include "UI/ui_panel_right_scroll.h"
#include "Core/global_state.h"
#include "UI/ui_panel_view_layout.h"

#include "UI/ui_panel_right_controls.h"
#include "UI/ui_panel_view_summary.h"

enum {
    UI_VIEW_PANE_SECTION_GAP = 8
};

void UIPanel_UpdateViewPaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    int summary_height = 0;
    int view_height = 0;
    int modes_height = 0;
    int workspace_top = 0;
    int workspace_bottom = 0;

    if (!ui) return;

    ui->viewPane.summaryRect = zero;
    ui->viewPane.workspaceRect = zero;
    ui->viewPane.viewRect = zero;
    ui->viewPane.modesRect = zero;

    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_VIEW) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    summary_height = UIPanel_ViewSummaryReservedHeight(ui);
    view_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_VIEW);
    modes_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_MODES);

    int total_height=ui->rightBodyRect.h;
    if(Global_GetWorkspaceMode()==LINE_DRAWING_WORKSPACE_MODE_SCENE) {
        int minimum=summary_height+view_height+modes_height+UIPanel_SectionHeight()+24;
        if(total_height<minimum)total_height=minimum;
        UIPanel_RightScrollSetContentHeight(ui,(float)total_height);
    }
    int scroll=(int)UIPanel_RightScrollOffset(ui);
    ui->viewPane.summaryRect = (SDL_Rect){
        ui->rightBodyRect.x,
        ui->rightBodyRect.y-scroll,
        ui->rightBodyRect.w,
        summary_height
    };
    ui->viewPane.modesRect = (SDL_Rect){
        ui->rightBodyRect.x,
        ui->rightBodyRect.y + total_height - modes_height-scroll,
        ui->rightBodyRect.w,
        modes_height
    };
    ui->viewPane.viewRect = (SDL_Rect){
        ui->rightBodyRect.x,
        ui->viewPane.modesRect.y - UI_VIEW_PANE_SECTION_GAP - view_height,
        ui->rightBodyRect.w,
        view_height
    };
    if (Global_GetWorkspaceMode()==LINE_DRAWING_WORKSPACE_MODE_OBJECT && ui->viewPane.viewRect.y < ui->rightBodyRect.y) {
        ui->viewPane.viewRect.y = ui->rightBodyRect.y;
    }

    workspace_top = ui->rightBodyRect.y + summary_height-scroll;
    if (summary_height > 0) workspace_top += UI_VIEW_PANE_SECTION_GAP;
    workspace_bottom = ui->viewPane.viewRect.y - UI_VIEW_PANE_SECTION_GAP;
    if (workspace_bottom < workspace_top) workspace_bottom = workspace_top;
    ui->viewPane.workspaceRect = (SDL_Rect){
        ui->rightBodyRect.x,
        workspace_top,
        ui->rightBodyRect.w,
        workspace_bottom - workspace_top
    };
}

bool UIPanel_GetViewPaneRects(const UIPanelState* ui,
                              SDL_Rect* out_summary_rect,
                              SDL_Rect* out_workspace_rect,
                              SDL_Rect* out_view_rect,
                              SDL_Rect* out_modes_rect) {
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_VIEW) return false;
    if (out_summary_rect) *out_summary_rect = ui->viewPane.summaryRect;
    if (out_workspace_rect) *out_workspace_rect = ui->viewPane.workspaceRect;
    if (out_view_rect) *out_view_rect = ui->viewPane.viewRect;
    if (out_modes_rect) *out_modes_rect = ui->viewPane.modesRect;
    return true;
}
