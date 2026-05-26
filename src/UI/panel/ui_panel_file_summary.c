#include "UI/ui_panel_file_summary.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelFileSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelFileSummary_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelFileSummary_PanelPad(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
}

static const char* UIPanelFileSummary_BaseName(const char* path) {
    const char* base = NULL;
    if (!path || !path[0]) return "(unset)";
    base = strrchr(path, '/');
    return base ? (base + 1) : path;
}

static const char* UIPanelFileSummary_ModeLabel(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return "JSON";
        case UI_LOAD_MENU_MODE_SCENE: return "Scene";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "Idle";
    }
}

int UIPanel_FileSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeLeftTab != UI_PANEL_LEFT_TAB_FILE) return 0;
    font_h = UIPanelFileSummary_FontHeight();
    line_gap = UIPanelFileSummary_LineGap();
    pad = UIPanelFileSummary_PanelPad();
    return (pad * 2) + (font_h * 7) + (line_gap * 6);
}

void Render_UIPanelFileSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 210};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Rect panel = {0, 0, 0, 0};
    int font_h = 0;
    int line_gap = 0;
    int panel_pad = 0;
    int y = 0;
    char line_layout_scene[256];
    char line_input[384];
    char line_output[384];
    char line_status_browser[256];
    char line_action_hint[320];
    char line_browser_root[384];

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_FILE) return;
    if (!UIPanel_GetFilePaneRects(ui, &panel, NULL, NULL, NULL)) return;
    if (panel.h <= 0) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;
    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);

    font_h = UIPanelFileSummary_FontHeight();
    line_gap = UIPanelFileSummary_LineGap();
    panel_pad = metrics.pad_y;
    y = panel.y + panel_pad;

    UIPanelSummary_DrawText(renderer, font, "File", panel.x + metrics.pad_x, y, label_color);
    y += font_h + line_gap;

    snprintf(line_layout_scene,
             sizeof(line_layout_scene),
             "Layout %s   Scene %s",
             UIPanelFileSummary_BaseName(Global_GetCurrentConfigPath()),
             UIPanelFileSummary_BaseName(Global_GetCurrentSceneAuthoringPath()));
    snprintf(line_input,
             sizeof(line_input),
             "Input  %s",
             Global_GetInputRoot() ? Global_GetInputRoot() : "(unset)");
    snprintf(line_output,
             sizeof(line_output),
             "Output  %s",
             Global_GetOutputRoot() ? Global_GetOutputRoot() : "(unset)");
    snprintf(line_status_browser,
             sizeof(line_status_browser),
             "Status  %s",
             state->layoutDirtySinceSave ? "Modified" : "Clean");
    (void)UIPanel_GetFileBrowserStatusText(ui, line_action_hint, sizeof(line_action_hint));
    {
        char browser_status_line[256];
        if (!UIPanel_GetFileBrowserStatusText(ui, browser_status_line, sizeof(browser_status_line))) {
            snprintf(browser_status_line, sizeof(browser_status_line), "Browser  %s", UIPanelFileSummary_ModeLabel(ui->loadMenu.mode));
        }
        snprintf(line_status_browser,
                 sizeof(line_status_browser),
                 "Status  %s   %s",
                 state->layoutDirtySinceSave ? "Modified" : "Clean",
                 browser_status_line);
    }
    if (!UIPanel_GetFileBrowserActionHintText(ui, line_action_hint, sizeof(line_action_hint))) {
        snprintf(line_action_hint,
                 sizeof(line_action_hint),
                 "Actions  Use Session targets the live row. Clear Last removes remembered fallback rows.");
    }
    snprintf(line_browser_root,
             sizeof(line_browser_root),
             "Browser  %s",
             ui->loadMenu.rootPath[0] ? ui->loadMenu.rootPath : "(mode root unset)");

    UIPanelSummary_DrawTextClipped(renderer, font, line_layout_scene, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, accent_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawDivider(renderer, panel, y - (line_gap / 2), metrics.pad_x, accent_color, 90);
    UIPanelSummary_DrawTextClipped(renderer, font, line_input, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_browser_root, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_output, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_status_browser, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, label_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_action_hint, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, label_color);
}
