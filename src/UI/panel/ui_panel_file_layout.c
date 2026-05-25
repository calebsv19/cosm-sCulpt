#include "UI/ui_panel_file_layout.h"

#include "UI/ui_panel_file_controls.h"
#include "UI/ui_panel_file_summary.h"

enum {
    UI_FILE_PANE_SECTION_GAP = 8
};

void UIPanel_UpdateFilePaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    int summary_height = 0;
    int file_actions_height = 0;
    int root_paths_height = 0;
    int controls_bottom = 0;
    int browser_top = 0;
    int browser_bottom = 0;

    if (!ui) return;

    ui->filePane.summaryRect = zero;
    ui->filePane.fileActionsRect = zero;
    ui->filePane.rootPathsRect = zero;
    ui->filePane.browserRect = zero;

    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_FILE) return;
    if (ui->leftBodyRect.w <= 0 || ui->leftBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    summary_height = UIPanel_FileSummaryReservedHeight(ui);
    file_actions_height = UIPanel_FileControlsSectionHeight(&metrics, UI_PANEL_GROUP_LEFT_FILE_IO);
    root_paths_height = UIPanel_FileControlsSectionHeight(&metrics, UI_PANEL_GROUP_LEFT_ROOT_PATHS);

    ui->filePane.summaryRect = (SDL_Rect){
        ui->leftBodyRect.x,
        ui->leftBodyRect.y,
        ui->leftBodyRect.w,
        summary_height
    };

    controls_bottom = ui->leftBodyRect.y + ui->leftBodyRect.h;
    ui->filePane.rootPathsRect = (SDL_Rect){
        ui->leftBodyRect.x,
        controls_bottom - root_paths_height,
        ui->leftBodyRect.w,
        root_paths_height
    };
    ui->filePane.fileActionsRect = (SDL_Rect){
        ui->leftBodyRect.x,
        ui->filePane.rootPathsRect.y - metrics.group_gap_px - file_actions_height,
        ui->leftBodyRect.w,
        file_actions_height
    };
    if (ui->filePane.fileActionsRect.y < ui->leftBodyRect.y) {
        ui->filePane.fileActionsRect.y = ui->leftBodyRect.y;
    }

    browser_top = ui->leftBodyRect.y + summary_height;
    if (summary_height > 0) browser_top += UI_FILE_PANE_SECTION_GAP;
    browser_bottom = ui->filePane.fileActionsRect.y - UI_FILE_PANE_SECTION_GAP;
    if (browser_bottom < browser_top) browser_bottom = browser_top;
    ui->filePane.browserRect = (SDL_Rect){
        ui->leftBodyRect.x,
        browser_top,
        ui->leftBodyRect.w,
        browser_bottom - browser_top
    };
}

bool UIPanel_GetFilePaneRects(const UIPanelState* ui,
                              SDL_Rect* out_summary_rect,
                              SDL_Rect* out_file_actions_rect,
                              SDL_Rect* out_root_paths_rect,
                              SDL_Rect* out_browser_rect) {
    if (!ui || ui->activeLeftTab != UI_PANEL_LEFT_TAB_FILE) return false;
    if (out_summary_rect) *out_summary_rect = ui->filePane.summaryRect;
    if (out_file_actions_rect) *out_file_actions_rect = ui->filePane.fileActionsRect;
    if (out_root_paths_rect) *out_root_paths_rect = ui->filePane.rootPathsRect;
    if (out_browser_rect) *out_browser_rect = ui->filePane.browserRect;
    return true;
}
