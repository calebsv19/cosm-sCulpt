#include "UI/ui_panel_object_layout.h"
#include "UI/ui_panel_furniture.h"

#include "UI/ui_panel_right_controls.h"
#include "UI/ui_panel_object_inspector.h"
#include "UI/ui_panel_scene_authoring_inspector.h"
#include "UI/ui_panel_right_scroll.h"

enum {
    UI_OBJECT_PANE_SECTION_GAP = 6
};

void UIPanel_UpdateObjectPaneLayout(UIPanelState* ui) {
    UIPanelLayoutMetrics metrics = {0};
    SDL_Rect zero = {0, 0, 0, 0};
    int summary_height = 0;
    int details_height = 0;
    int actions_height = 0;
    int prism_height = 0;
    int gizmo_height = 0;
    int transform_height = 0;
    int cursor_y = 0;
    int content_height = 0;
    int scroll_offset = 0;
    SDL_Rect content_rect = {0, 0, 0, 0};

    if (!ui) return;

    ui->objectPane.furnitureRect = zero;
    ui->objectPane.summaryRect = zero;
    ui->objectPane.detailsRect = zero;
    ui->objectPane.actionsRect = zero;
    ui->objectPane.prismRect = zero;
    ui->objectPane.gizmoRect = zero;
    ui->objectPane.transformRect = zero;

    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    UIPanel_GetLayoutMetrics(&metrics);
    content_rect = ui->rightBodyRect;
    if (content_rect.w > 12) content_rect.w -= 12;
    if (UIPanel_SceneAuthoringInspectorHasSelection()) {
        summary_height = UIPanel_SceneAuthoringInspectorReservedHeight(ui);
        details_height = UIPanel_SceneAuthoringInspectorDetailsHeight(ui);
    } else {
        summary_height = UIPanel_ObjectInspectorReservedHeight(ui);
        details_height = UIPanel_ObjectInspectorDetailsHeight(ui);
    }
    actions_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS);
    prism_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_PRISM);
    gizmo_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_GIZMO);
    transform_height = UIPanel_RightControlsSectionHeight(&metrics, UI_PANEL_GROUP_RIGHT_TRANSFORM);

    int furniture_height = UIPanel_FurnitureHeight();
    if (UIPanel_FurnitureEditing()) {
        summary_height = details_height = actions_height = prism_height = gizmo_height = transform_height = 0;
    }
    cursor_y = content_rect.y;
#define UI_OBJECT_PLACE(rect_field, height_value) \
    do { \
        if ((height_value) > 0) { \
            ui->objectPane.rect_field = (SDL_Rect){content_rect.x, cursor_y, content_rect.w, (height_value)}; \
            cursor_y += (height_value) + UI_OBJECT_PANE_SECTION_GAP; \
        } \
    } while (0)
    UI_OBJECT_PLACE(furnitureRect, furniture_height);
    UI_OBJECT_PLACE(summaryRect, summary_height);
    UI_OBJECT_PLACE(detailsRect, details_height);
    UI_OBJECT_PLACE(actionsRect, actions_height);
    UI_OBJECT_PLACE(prismRect, prism_height);
    UI_OBJECT_PLACE(gizmoRect, gizmo_height);
    UI_OBJECT_PLACE(transformRect, transform_height);
#undef UI_OBJECT_PLACE
    if (cursor_y > content_rect.y) cursor_y -= UI_OBJECT_PANE_SECTION_GAP;
    content_height = cursor_y - content_rect.y;
    UIPanel_RightScrollSetContentHeight(ui, (float)content_height);
    scroll_offset = (int)UIPanel_RightScrollOffset(ui);
    ui->objectPane.furnitureRect.y -= scroll_offset;
    ui->objectPane.summaryRect.y -= scroll_offset;
    ui->objectPane.detailsRect.y -= scroll_offset;
    ui->objectPane.actionsRect.y -= scroll_offset;
    ui->objectPane.prismRect.y -= scroll_offset;
    ui->objectPane.gizmoRect.y -= scroll_offset;
    ui->objectPane.transformRect.y -= scroll_offset;
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
