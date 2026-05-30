#include "UI/panel/ui_panel_object_workspace_layout.h"
#include "Core/global_state.h"

enum {
    UI_OBJECT_WORKSPACE_LEFT_GAP = 8
};

void UIPanel_UpdateObjectWorkspacePaneLayout(UIPanelState* ui) {
    SDL_Rect zero = {0, 0, 0, 0};
    int summary_height = 0;
    int browser_top = 0;

    if (!ui) return;

    ui->objectWorkspacePane.summaryRect = zero;
    ui->objectWorkspacePane.browserRect = zero;

    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;
    if (ui->leftBodyRect.w <= 0 || ui->leftBodyRect.h <= 0) return;

    summary_height = ui->leftBodyRect.h / 3;
    if (summary_height < 120) summary_height = 120;
    if (summary_height > ui->leftBodyRect.h) summary_height = ui->leftBodyRect.h;

    ui->objectWorkspacePane.summaryRect = (SDL_Rect){
        ui->leftBodyRect.x,
        ui->leftBodyRect.y,
        ui->leftBodyRect.w,
        summary_height
    };

    browser_top = ui->leftBodyRect.y + summary_height;
    if (summary_height > 0) browser_top += UI_OBJECT_WORKSPACE_LEFT_GAP;
    if (browser_top > ui->leftBodyRect.y + ui->leftBodyRect.h) {
        browser_top = ui->leftBodyRect.y + ui->leftBodyRect.h;
    }
    ui->objectWorkspacePane.browserRect = (SDL_Rect){
        ui->leftBodyRect.x,
        browser_top,
        ui->leftBodyRect.w,
        (ui->leftBodyRect.y + ui->leftBodyRect.h) - browser_top
    };
    if (ui->objectWorkspacePane.browserRect.h < 0) {
        ui->objectWorkspacePane.browserRect.h = 0;
    }
}
