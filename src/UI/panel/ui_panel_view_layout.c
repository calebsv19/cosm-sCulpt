#include "UI/ui_panel_section.h"
#include "UI/ui_panel_camera.h"
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

    if (!ui) return;

    ui->viewPane.summaryRect = zero;
    ui->viewPane.workspaceRect = zero;
    ui->viewPane.viewRect = zero;
    ui->viewPane.modesRect = zero;

    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_VIEW) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    summary_height = Global_GetWorkspaceMode()==LINE_DRAWING_WORKSPACE_MODE_SCENE ?
        UIPanel_CameraHeight() : UIPanel_ViewSummaryReservedHeight(ui);
    view_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_VIEW);
    modes_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_MODES);

    int content_height = summary_height + view_height + modes_height + 12;
    int section_height = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_SCENE
                             ? UIPanel_SectionHeight() : 120;
    if (Global_Get() && Global_Get()->cameraView.active) {
        view_height=0; modes_height=0; section_height=0; content_height=summary_height;
    } else content_height += section_height;
    UIPanel_RightScrollSetContentHeight(ui, (float)content_height);
    int y = ui->rightBodyRect.y - (int)UIPanel_RightScrollOffset(ui);
    ui->viewPane.summaryRect = (SDL_Rect){ui->rightBodyRect.x, y, ui->rightBodyRect.w, summary_height};
    y += summary_height;
    ui->viewPane.viewRect = (SDL_Rect){ui->rightBodyRect.x, y, ui->rightBodyRect.w, view_height};
    y += view_height + 4;
    ui->viewPane.workspaceRect = (SDL_Rect){ui->rightBodyRect.x, y, ui->rightBodyRect.w, section_height};
    y += section_height + 4;
    ui->viewPane.modesRect = (SDL_Rect){ui->rightBodyRect.x, y, ui->rightBodyRect.w, modes_height};

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
