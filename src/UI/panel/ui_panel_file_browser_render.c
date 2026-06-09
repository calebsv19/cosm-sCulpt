#include "UI/panel/ui_panel_file_browser_internal.h"

#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <stdio.h>

void Render_UIPanelFileBrowser(const UIPanelState* ui, SDL_Renderer* renderer) {
    UIPanelVisualPalette palette = {0};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color panel_fill = {20, 20, 24, 170};
    SDL_Color panel_border = {90, 100, 115, 200};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect rect = {0};
    SDL_Rect list_clip = {0};
    SDL_Rect track = {0};
    SDL_Rect thumb = {0};
    SDL_Rect set_dir_button = {0};
    const char* title = NULL;
    const char* empty_label = NULL;
    SDL_Color active_fill = {58, 68, 84, 185};
    SDL_Color hover_fill = {42, 50, 62, 150};
    SDL_Color active_text_color = {240, 242, 248, 255};
    SDL_Color remembered_fill = {92, 82, 42, 176};
    SDL_Color remembered_border = {186, 156, 84, 210};
    SDL_Color remembered_accent = {222, 188, 96, 255};
    SDL_Color chip_fill = {36, 40, 48, 220};
    SDL_Color chip_border = {140, 170, 210, 235};
    char status_line[96];
    char helper_line[320];
    int font_h = 14;
    int chip_font_h = 0;
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);

    if (!ui || !renderer || !font) return;
    if (!UIPanel_IsLoadMenuOpen()) return;
    rect = UIPanel_GetLoadMenuRect(ui);
    if (rect.w <= 0 || rect.h <= 0) return;

    title = (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) ? "JSON Browser" :
            (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) ? "Scene Browser" :
            (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) ? "Asset Browser" :
            (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) ? "Runtime Mesh Browser" :
            (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) ? "STL Import Browser" :
            "Browser";
    if (!UIPanel_GetFileBrowserActionHintText(ui, helper_line, sizeof(helper_line))) {
        snprintf(helper_line,
                 sizeof(helper_line),
                 "Actions  Use Session targets the live row. Clear Last removes remembered fallback rows.");
    }
    empty_label = (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON)
                      ? "No JSON files found in the current input root."
                      : (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE)
                            ? "No scene entries found in the current input root."
                            : (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH)
                                  ? "No runtime mesh sidecars found in the current asset root."
                                  : (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT)
                                        ? "No STL files found in the current STL root."
                                        : "No object asset files found in the current asset root.";

    if (UIPanelVisual_ResolvePalette(&palette)) {
        label_color = palette.text_muted;
        value_color = palette.text_primary;
        accent_color = palette.accent;
        panel_fill = palette.workspace_fill;
        panel_fill.a = palette.workspace_fill.a;
        panel_border = palette.pane_border;
        active_fill = UIPanelVisual_BlendColor(palette.workspace_fill, palette.button_border, 102);
        active_fill.a = 188;
        hover_fill = UIPanelVisual_BlendColor(palette.workspace_fill, palette.button_border, 48);
        hover_fill.a = 150;
        remembered_fill = UIPanelVisual_BlendColor(palette.workspace_fill, palette.button_border, 128);
        remembered_fill.a = 176;
        remembered_border = UIPanelVisual_BlendColor(palette.button_border,
                                                     (SDL_Color){255, 210, 120, 255},
                                                     110);
        remembered_border.a = 210;
        remembered_accent = UIPanelVisual_BlendColor(palette.button_border,
                                                     (SDL_Color){255, 214, 132, 255},
                                                     136);
        remembered_accent.a = 255;
        chip_fill = UIPanelVisual_AdjustColor(palette.workspace_fill, 8, 8);
        chip_fill.a = 220;
        chip_border = palette.button_border;
        chip_border.a = 235;
        if ((((int)active_fill.r * 299) + ((int)active_fill.g * 587) + ((int)active_fill.b * 114)) / 1000 > 180) {
            active_fill.r = (Uint8)(((int)active_fill.r > 48) ? ((int)active_fill.r - 48) : 0);
            active_fill.g = (Uint8)(((int)active_fill.g > 48) ? ((int)active_fill.g - 48) : 0);
            active_fill.b = (Uint8)(((int)active_fill.b > 48) ? ((int)active_fill.b - 48) : 0);
            active_text_color = (SDL_Color){28, 32, 40, 255};
        } else {
            active_text_color = palette.text_primary;
        }
    }

    font_h = TTF_FontHeight(font);
    if (font_h < 12) font_h = 12;
    chip_font_h = font_h;
    {
        const char* singular = "entry";
        const char* plural = "entries";
        if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
            singular = "JSON entry";
            plural = "JSON entries";
        } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
            singular = "scene entry";
            plural = "scene entries";
        } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
            singular = "asset entry";
            plural = "asset entries";
        } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
            singular = "runtime mesh";
            plural = "runtime meshes";
        } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
            singular = "STL";
            plural = "STLs";
        }
        snprintf(status_line,
                 sizeof(status_line),
                 "%d %s",
                 ui->loadMenu.count,
                 ui->loadMenu.count == 1 ? singular : plural);
    }
    list_clip = UIPanel_GetLoadMenuListClipRect(ui);
    track = UIPanel_GetLoadMenuScrollTrackRect(ui);
    thumb = UIPanel_GetLoadMenuScrollThumbRect(ui);
    set_dir_button = UIPanel_GetLoadMenuSetDirectoryButtonRect(ui);

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    UIPanelSummary_DrawCard(renderer,
                            rect,
                            panel_fill,
                            panel_border,
                            accent_color,
                            4);

    {
        const int header_text_w = set_dir_button.w > 0
            ? set_dir_button.x - rect.x - (metrics.pad_x * 2) - 6
            : rect.w - (metrics.pad_x * 2);
        UIPanelSummary_DrawTextClipped(renderer,
                                       font,
                                       title,
                                       rect.x + metrics.pad_x,
                                       rect.y + metrics.pad_y + 1,
                                       header_text_w,
                                       font_h + 3,
                                       label_color);
        if (set_dir_button.w > 0 && set_dir_button.h > 0) {
            SDL_Color button_fill = UIPanelVisual_AdjustColor(panel_fill, 12, 8);
            SDL_Color button_border = panel_border;
            UIPanelVisual_DrawFrame(renderer, set_dir_button, button_fill, button_border, 90);
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           "Set Directory",
                                           set_dir_button.x + 6,
                                           set_dir_button.y + ((set_dir_button.h - font_h) / 2),
                                           set_dir_button.w - 12,
                                           font_h + 2,
                                           value_color);
        }
    }
    UIPanelSummary_DrawTextClipped(renderer,
                                   font,
                                   status_line,
                                   rect.x + metrics.pad_x,
                                   rect.y + metrics.pad_y + font_h + 3,
                                   set_dir_button.w > 0
                                       ? set_dir_button.x - rect.x - (metrics.pad_x * 2) - 6
                                       : rect.w - (metrics.pad_x * 2),
                                   font_h + 4,
                                   value_color);
    UIPanelSummary_DrawDivider(renderer,
                               rect,
                               rect.y + 28 - 2,
                               metrics.pad_x,
                               accent_color,
                               90);

    if (list_clip.w > 0 && list_clip.h > 0) {
        SDL_RenderSetClipRect(renderer, &list_clip);
    }
    if (ui->loadMenu.count <= 0) {
        UIPanelSummary_DrawTextClipped(renderer,
                                       font,
                                       empty_label,
                                       list_clip.x,
                                       list_clip.y + metrics.row_text_y,
                                       list_clip.w,
                                       font_h + 4,
                                       label_color);
    } else {
        const int first_index = (int)(ui->loadMenu.scrollOffsetPx / 24.0f);
        const int offset_in_row = (int)ui->loadMenu.scrollOffsetPx % 24;
        int y = list_clip.y - offset_in_row;
        for (int i = first_index; i < ui->loadMenu.count && y < list_clip.y + list_clip.h; ++i) {
            const bool hovered = (i == ui->loadMenu.hoverIndex);
            const bool active = (i == ui->loadMenu.activeIndex);
            UILoadMenuSelectionState row_state = UI_LOAD_MENU_SELECTION_NONE;
            SDL_Rect row_rect = {
                list_clip.x,
                y,
                list_clip.w,
                24 - 2
            };
            SDL_Rect chip_rect = {0, 0, 0, 0};
            const char* chip_label = NULL;
            int text_max_width = row_rect.w - (metrics.pad_x * 2);
            (void)UIPanel_GetFileBrowserRowSelectionState(ui, i, &row_state);
            if (row_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION) {
                chip_label = "LIVE";
            } else if (row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY) {
                chip_label = "LAST";
            }
            if (chip_label) {
                chip_rect.w = 40;
                chip_rect.h = metrics.chip_h - 2;
                chip_rect.x = row_rect.x + row_rect.w - metrics.pad_x - chip_rect.w;
                chip_rect.y = row_rect.y + ((row_rect.h - chip_rect.h) / 2);
                text_max_width = chip_rect.x - row_rect.x - (metrics.pad_x * 2) - 6;
                if (text_max_width < 24) text_max_width = 24;
            }
            {
                SDL_Color fill = active ? active_fill : hover_fill;
                SDL_Color border = active ? panel_border : UIPanelVisual_AdjustColor(panel_border, -10, -30);
                SDL_Color accent = active ? accent_color : UIPanelVisual_AdjustColor(accent_color, -8, 0);
                Uint8 inner_alpha = active ? 70 : 0;
                if (row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY) {
                    fill = remembered_fill;
                    border = remembered_border;
                    accent = remembered_accent;
                    inner_alpha = active ? 92 : 0;
                }
                if (hovered || active) {
                    UIPanelVisual_DrawInteractiveRow(renderer,
                                                     row_rect,
                                                     fill,
                                                     border,
                                                     accent,
                                                     hovered,
                                                     active,
                                                     3,
                                                     inner_alpha);
                }
            }
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           ui->loadMenu.entries[i],
                                           row_rect.x + metrics.pad_x,
                                           row_rect.y + metrics.row_text_y,
                                           text_max_width,
                                           font_h + 4,
                                           active ? active_text_color : value_color);
            if (chip_label && chip_rect.w > 0 && chip_rect.h > 0) {
                SDL_Color local_chip_border = (row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY)
                                                  ? remembered_border
                                                  : chip_border;
                UIPanelVisual_DrawLabelChip(renderer,
                                            chip_rect,
                                            chip_fill,
                                            local_chip_border,
                                            chip_fill.a,
                                            local_chip_border.a);
                UIPanelSummary_DrawTextClipped(renderer,
                                               font,
                                               chip_label,
                                               chip_rect.x + 6,
                                               chip_rect.y + ((chip_rect.h - chip_font_h) / 2),
                                               chip_rect.w - 12,
                                               chip_font_h + 2,
                                               row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY
                                                   ? remembered_accent
                                                   : accent_color);
            }
            y += 24;
        }
    }
    SDL_RenderSetClipRect(renderer, NULL);

    if (UIPanel_LoadMenuHasScrollableContent(ui) &&
        track.w > 0 && track.h > 0 && thumb.h > 0) {
        SDL_Color thumb_fill = accent_color;
        SDL_Color thumb_border = panel_border;
        thumb_fill.a = 220;
        thumb_border.a = 210;
        UIPanelVisual_DrawScrollbar(renderer,
                                    track,
                                    thumb,
                                    UIPanelVisual_AdjustColor(panel_border, -12, -120),
                                    panel_border,
                                    thumb_fill,
                                    thumb_border,
                                    ui->loadMenu.scrollbarDragging);
    }

    UIPanelSummary_DrawTextClipped(renderer,
                                   font,
                                   helper_line,
                                   rect.x + metrics.pad_x,
                                   rect.y + rect.h - 20 + 4,
                                   rect.w - (metrics.pad_x * 2),
                                   font_h + 4,
                                   label_color);
}
