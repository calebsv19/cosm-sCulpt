#include "UI/ui_panel_object_workspace_summary.h"

#include "Core/global_state.h"
#include "Layout/layout.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

static const char* UIPanelObjectWorkspace_BaseName(const char* path) {
    const char* base = NULL;
    if (!path || !path[0]) return "(unset)";
    base = strrchr(path, '/');
    return base ? (base + 1) : path;
}

void Render_UIPanelObjectWorkspaceSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 210};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect detail = {0, 0, 0, 0};
    int y = 0;
    const int font_h = (font && TTF_FontHeight(font) > 12) ? TTF_FontHeight(font) : 14;
    char line_asset[192];
    char line_status[128];
    char line_root[320];
    char line_counts[160];
    char line_selection[160];

    if (!ui || !renderer || !font || !state) return;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;

    panel = ui->objectWorkspacePane.summaryRect;
    detail = ui->objectWorkspacePane.browserRect;
    if (panel.w <= 0 || panel.h <= 0) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;

    snprintf(line_asset,
             sizeof(line_asset),
             "Asset  %s",
             UIPanelObjectWorkspace_BaseName(Global_GetCurrentObjectAssetPath()));
    snprintf(line_status,
             sizeof(line_status),
             "Status  %s",
             state->layoutDirtySinceSave ? "Modified" : "Clean");
    snprintf(line_root,
             sizeof(line_root),
             "Root  %s",
             Global_GetObjectAssetRoot() ? Global_GetObjectAssetRoot() : "(unset)");
    snprintf(line_counts,
             sizeof(line_counts),
             "Bodies  %zu live   Next Id %u",
             Layout_ObjectStore_LiveCount(&state->layout.objectStore),
             state->layout.objectStore.nextObjectId);
    if (state->editor.selectedObjectAssetBodyId != 0u) {
        snprintf(line_selection,
                 sizeof(line_selection),
                 "Selection  Body #%u   Face %s",
                 state->editor.selectedObjectAssetBodyId,
                 Layout_Object3DFaceKind_Label(state->editor.selectedObjectAssetFace));
    } else if (state->editor.selectedObject3DId != 0u) {
        snprintf(line_selection,
                 sizeof(line_selection),
                 "Selection  Object #%u",
                 state->editor.selectedObject3DId);
    } else {
        snprintf(line_selection, sizeof(line_selection), "Selection  None");
    }

    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);
    y = panel.y + metrics.pad_y;
    UIPanelSummary_DrawText(renderer, font, "Object Asset", panel.x + metrics.pad_x, y, label_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_asset, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, accent_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawDivider(renderer, panel, y - (metrics.section_gap / 2), metrics.pad_x, accent_color, 90);
    UIPanelSummary_DrawTextClipped(renderer, font, line_root, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_status, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);

    if (detail.w <= 0 || detail.h <= 0) return;

    UIPanelSummary_DrawCard(renderer, detail, fill_color, border_color, accent_color, metrics.accent_h);
    y = detail.y + metrics.pad_y;
    UIPanelSummary_DrawText(renderer, font, "Workspace", detail.x + metrics.pad_x, y, label_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_counts, detail.x + metrics.pad_x, y, detail.w - (metrics.pad_x * 2), font_h + 4, accent_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawDivider(renderer, detail, y - (metrics.section_gap / 2), metrics.pad_x, accent_color, 90);
    UIPanelSummary_DrawTextClipped(renderer, font, line_selection, detail.x + metrics.pad_x, y, detail.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + metrics.section_gap;
    UIPanelSummary_DrawWrappedText(renderer,
                                   font,
                                   "Use Shape for the authoring workflow: Seed / Sketch at the lower-middle, Operations at the bottom, Asset for selected body inspection, and the viewport to choose the next face target.",
                                   detail.x + metrics.pad_x,
                                   y,
                                   detail.w - (metrics.pad_x * 2),
                                   font_h,
                                   metrics.section_gap,
                                   4,
                                   label_color);
}
