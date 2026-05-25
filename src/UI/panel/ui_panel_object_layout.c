#include "UI/ui_panel_object_layout.h"

#include "UI/ui_panel_object_inspector.h"

enum {
    UI_OBJECT_PANE_SECTION_GAP = 6,
    UI_OBJECT_PANE_ACTION_ROW_COUNT = 1,
    UI_OBJECT_PANE_PRISM_ROW_COUNT = 1,
    UI_OBJECT_PANE_GIZMO_ROW_COUNT = 1,
    UI_OBJECT_PANE_TRANSFORM_ROW_COUNT = 2
};

static int UIPanelObjectPane_GroupHeightForRows(int header_height,
                                                int button_height,
                                                int button_spacing,
                                                int row_count) {
    if (row_count <= 0) return 0;
    return header_height +
           (button_height * row_count) +
           (button_spacing * (row_count - 1));
}

void UIPanel_UpdateObjectPaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    int summary_height = 0;
    int details_height = 0;
    int actions_height = 0;
    int prism_height = 0;
    int gizmo_height = 0;
    int transform_height = 0;
    int summary_top = 0;
    int details_top = 0;
    int details_bottom = 0;
    int actions_top = 0;
    int prism_top = 0;
    int gizmo_top = 0;
    int transform_top = 0;
    int details_target_height = 0;

    if (!ui) return;

    ui->objectPane.summaryRect = zero;
    ui->objectPane.detailsRect = zero;
    ui->objectPane.actionsRect = zero;
    ui->objectPane.prismRect = zero;
    ui->objectPane.gizmoRect = zero;
    ui->objectPane.transformRect = zero;

    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    summary_height = UIPanel_ObjectInspectorReservedHeight(ui);
    details_height = UIPanel_ObjectInspectorDetailsHeight(ui);
    actions_height = UIPanelObjectPane_GroupHeightForRows(metrics.group_header_height_px,
                                                          metrics.button_height_px,
                                                          metrics.button_spacing_px,
                                                          UI_OBJECT_PANE_ACTION_ROW_COUNT);
    prism_height = UIPanelObjectPane_GroupHeightForRows(metrics.group_header_height_px,
                                                        metrics.button_height_px,
                                                        metrics.button_spacing_px,
                                                        UI_OBJECT_PANE_PRISM_ROW_COUNT);
    gizmo_height = UIPanelObjectPane_GroupHeightForRows(metrics.group_header_height_px,
                                                        metrics.button_height_px,
                                                        metrics.button_spacing_px,
                                                        UI_OBJECT_PANE_GIZMO_ROW_COUNT);
    transform_height = UIPanelObjectPane_GroupHeightForRows(metrics.group_header_height_px,
                                                            metrics.button_height_px,
                                                            metrics.button_spacing_px,
                                                            UI_OBJECT_PANE_TRANSFORM_ROW_COUNT);

    summary_top = ui->rightBodyRect.y;
    details_top = summary_top + summary_height + UI_OBJECT_PANE_SECTION_GAP;
    transform_top = ui->rightBodyRect.y + ui->rightBodyRect.h - transform_height;
    if (transform_top < details_top) transform_top = details_top;
    gizmo_top = transform_top - UI_OBJECT_PANE_SECTION_GAP - gizmo_height;
    if (gizmo_top < details_top) gizmo_top = details_top;
    prism_top = gizmo_top - UI_OBJECT_PANE_SECTION_GAP - prism_height;
    if (prism_top < details_top) prism_top = details_top;
    actions_top = prism_top - UI_OBJECT_PANE_SECTION_GAP - actions_height;
    if (actions_top < details_top) actions_top = details_top;

    details_bottom = actions_top - UI_OBJECT_PANE_SECTION_GAP;
    if (details_bottom < details_top) details_bottom = details_top;
    details_target_height = details_bottom - details_top;
    if (details_target_height > details_height) details_target_height = details_height;
    if (details_target_height < 0) details_target_height = 0;

    details_bottom = details_top + details_target_height;
    actions_top = details_bottom + UI_OBJECT_PANE_SECTION_GAP;
    prism_top = actions_top + actions_height + UI_OBJECT_PANE_SECTION_GAP;
    gizmo_top = prism_top + prism_height + UI_OBJECT_PANE_SECTION_GAP;
    transform_top = gizmo_top + gizmo_height + UI_OBJECT_PANE_SECTION_GAP;

    if (transform_top + transform_height < ui->rightBodyRect.y + ui->rightBodyRect.h) {
        int slack = (ui->rightBodyRect.y + ui->rightBodyRect.h) - (transform_top + transform_height);
        actions_top += slack;
        prism_top += slack;
        gizmo_top += slack;
        transform_top += slack;
    } else if (transform_top + transform_height > ui->rightBodyRect.y + ui->rightBodyRect.h) {
        int overflow = (transform_top + transform_height) - (ui->rightBodyRect.y + ui->rightBodyRect.h);
        details_target_height -= overflow;
        if (details_target_height < 0) details_target_height = 0;
        details_bottom = details_top + details_target_height;
        actions_top = details_bottom + UI_OBJECT_PANE_SECTION_GAP;
        prism_top = actions_top + actions_height + UI_OBJECT_PANE_SECTION_GAP;
        gizmo_top = prism_top + prism_height + UI_OBJECT_PANE_SECTION_GAP;
        transform_top = gizmo_top + gizmo_height + UI_OBJECT_PANE_SECTION_GAP;
    }

    ui->objectPane.summaryRect = (SDL_Rect){
        ui->rightBodyRect.x,
        summary_top,
        ui->rightBodyRect.w,
        summary_height
    };

    ui->objectPane.detailsRect = (SDL_Rect){
        ui->rightBodyRect.x,
        details_top,
        ui->rightBodyRect.w,
        details_target_height
    };

    ui->objectPane.actionsRect = (SDL_Rect){
        ui->rightBodyRect.x,
        actions_top,
        ui->rightBodyRect.w,
        actions_height
    };

    ui->objectPane.prismRect = (SDL_Rect){
        ui->rightBodyRect.x,
        prism_top,
        ui->rightBodyRect.w,
        prism_height
    };

    ui->objectPane.gizmoRect = (SDL_Rect){
        ui->rightBodyRect.x,
        gizmo_top,
        ui->rightBodyRect.w,
        gizmo_height
    };

    ui->objectPane.transformRect = (SDL_Rect){
        ui->rightBodyRect.x,
        transform_top,
        ui->rightBodyRect.w,
        transform_height
    };
}

bool UIPanel_GetObjectPaneRects(const UIPanelState* ui,
                                SDL_Rect* out_summary_rect,
                                SDL_Rect* out_details_rect,
                                SDL_Rect* out_actions_rect,
                                SDL_Rect* out_prism_rect,
                                SDL_Rect* out_gizmo_rect,
                                SDL_Rect* out_transform_rect) {
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return false;
    if (out_summary_rect) *out_summary_rect = ui->objectPane.summaryRect;
    if (out_details_rect) *out_details_rect = ui->objectPane.detailsRect;
    if (out_actions_rect) *out_actions_rect = ui->objectPane.actionsRect;
    if (out_prism_rect) *out_prism_rect = ui->objectPane.prismRect;
    if (out_gizmo_rect) *out_gizmo_rect = ui->objectPane.gizmoRect;
    if (out_transform_rect) *out_transform_rect = ui->objectPane.transformRect;
    return true;
}
