#include "UI/ui_panel_scene_layout.h"

#include "UI/ui_panel_scene_summary.h"

enum {
    UI_SCENE_PANE_LIST_GAP = 8,
    UI_SCENE_PANE_SELECTION_BUTTON_COUNT = 2,
    UI_SCENE_PANE_BOUNDS_BUTTON_COUNT = 5
};

static int UIPanelScenePane_GroupHeight(int header_height,
                                        int button_height,
                                        int button_spacing,
                                        int button_count) {
    if (button_count <= 0) return 0;
    return header_height +
           (button_height * button_count) +
           (button_spacing * (button_count - 1));
}

void UIPanel_UpdateScenePaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    int summary_height = 0;
    int selection_height = 0;
    int bounds_height = 0;
    int bottom_controls_top = 0;
    int list_top = 0;
    int list_bottom = 0;

    if (!ui) return;

    ui->scenePane.summaryRect = zero;
    ui->scenePane.listRect = zero;
    ui->scenePane.selectionRect = zero;
    ui->scenePane.boundsRect = zero;

    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;
    if (ui->leftBodyRect.w <= 0 || ui->leftBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    summary_height = UIPanel_SceneSummaryReservedHeight(ui);
    selection_height = UIPanelScenePane_GroupHeight(metrics.group_header_height_px,
                                                    metrics.button_height_px,
                                                    metrics.button_spacing_px,
                                                    UI_SCENE_PANE_SELECTION_BUTTON_COUNT);
    bounds_height = UIPanelScenePane_GroupHeight(metrics.group_header_height_px,
                                                 metrics.button_height_px,
                                                 metrics.button_spacing_px,
                                                 UI_SCENE_PANE_BOUNDS_BUTTON_COUNT);

    ui->scenePane.summaryRect = (SDL_Rect){
        ui->leftBodyRect.x,
        ui->leftBodyRect.y,
        ui->leftBodyRect.w,
        summary_height
    };

    bottom_controls_top = ui->leftBodyRect.y + ui->leftBodyRect.h - bounds_height;
    if (bottom_controls_top < ui->leftBodyRect.y) {
        bottom_controls_top = ui->leftBodyRect.y;
    }
    ui->scenePane.boundsRect = (SDL_Rect){
        ui->leftBodyRect.x,
        bottom_controls_top,
        ui->leftBodyRect.w,
        bounds_height
    };

    bottom_controls_top -= metrics.group_gap_px + selection_height;
    if (bottom_controls_top < ui->leftBodyRect.y) {
        bottom_controls_top = ui->leftBodyRect.y;
    }
    ui->scenePane.selectionRect = (SDL_Rect){
        ui->leftBodyRect.x,
        bottom_controls_top,
        ui->leftBodyRect.w,
        selection_height
    };

    list_top = ui->leftBodyRect.y + summary_height;
    if (summary_height > 0) list_top += UI_SCENE_PANE_LIST_GAP;
    list_bottom = ui->scenePane.selectionRect.y - UI_SCENE_PANE_LIST_GAP;
    if (list_bottom < list_top) list_bottom = list_top;
    ui->scenePane.listRect = (SDL_Rect){
        ui->leftBodyRect.x,
        list_top,
        ui->leftBodyRect.w,
        list_bottom - list_top
    };
}

bool UIPanel_GetScenePaneRects(const UIPanelState* ui,
                               SDL_Rect* out_summary_rect,
                               SDL_Rect* out_list_rect,
                               SDL_Rect* out_selection_rect,
                               SDL_Rect* out_bounds_rect) {
    if (!ui || ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return false;
    if (out_summary_rect) *out_summary_rect = ui->scenePane.summaryRect;
    if (out_list_rect) *out_list_rect = ui->scenePane.listRect;
    if (out_selection_rect) *out_selection_rect = ui->scenePane.selectionRect;
    if (out_bounds_rect) *out_bounds_rect = ui->scenePane.boundsRect;
    return true;
}
