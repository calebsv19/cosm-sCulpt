#include "UI/ui_panel_file_controls.h"

#include "UI/font_manager.h"

#include <SDL2/SDL_ttf.h>
#include <string.h>

typedef struct UIPanelFileControlRowSpec {
    int row_key;
    int columns;
    int column_index;
} UIPanelFileControlRowSpec;

static bool UIPanel_FileControlRowSpecForButton(int button_id, UIPanelFileControlRowSpec* out_spec) {
    UIPanelFileControlRowSpec spec = {0, 0, 0};
    switch (button_id) {
        case UI_BTN_SAVE_JSON: spec = (UIPanelFileControlRowSpec){ 1, 2, 0 }; break;
        case UI_BTN_LOAD_JSON: spec = (UIPanelFileControlRowSpec){ 1, 2, 1 }; break;
        case UI_BTN_LOAD_SCENE: spec = (UIPanelFileControlRowSpec){ 2, 2, 0 }; break;
        case UI_BTN_EXPORT_SHAPE: spec = (UIPanelFileControlRowSpec){ 2, 2, 1 }; break;
        case UI_BTN_EXPORT_SCENE: spec = (UIPanelFileControlRowSpec){ 3, 1, 0 }; break;
        case UI_BTN_FILE_BROWSER_USE_ACTIVE: spec = (UIPanelFileControlRowSpec){ 4, 2, 0 }; break;
        case UI_BTN_FILE_BROWSER_CLEAR_REMEMBERED: spec = (UIPanelFileControlRowSpec){ 4, 2, 1 }; break;
        case UI_BTN_INPUT_ROOT_EDIT: spec = (UIPanelFileControlRowSpec){ 5, 2, 0 }; break;
        case UI_BTN_INPUT_ROOT_FOLDER: spec = (UIPanelFileControlRowSpec){ 5, 2, 1 }; break;
        case UI_BTN_OUTPUT_ROOT_EDIT: spec = (UIPanelFileControlRowSpec){ 6, 2, 0 }; break;
        case UI_BTN_OUTPUT_ROOT_FOLDER: spec = (UIPanelFileControlRowSpec){ 6, 2, 1 }; break;
        default: return false;
    }
    if (out_spec) *out_spec = spec;
    return true;
}

static int UIPanel_FileControlsRowCount(UIPanelGroup group) {
    switch (group) {
        case UI_PANEL_GROUP_LEFT_FILE_IO: return 4;
        case UI_PANEL_GROUP_LEFT_ROOT_PATHS: return 2;
        default: return 0;
    }
}

static int UIPanel_FileControlsMeasureTextWidthPx(const char* text) {
    TTF_Font* font = FontManager_GetUIPanelFont();
    int width = 0;
    if (!text || !text[0]) return 0;
    if (font && TTF_SizeUTF8(font, text, &width, NULL) == 0 && width > 0) {
        return width;
    }
    return (int)strlen(text) * 8;
}

static int UIPanel_FileControlsButtonWidthPx(const UIButton* button, int text_pad_x) {
    int width = 0;
    if (!button) return 22;
    width = UIPanel_FileControlsMeasureTextWidthPx(button->label) + (text_pad_x * 2);
    if (width < 22) width = 22;
    return width;
}

static void UIPanel_FileControlsLayoutGroup(UIPanelState* ui,
                                            UIPanelGroup group,
                                            SDL_Rect group_rect,
                                            int button_height,
                                            int button_spacing,
                                            int compact_gap,
                                            int text_pad_x) {
    int group_button_top = 0;
    int row_start = -1;

    if (!ui || group_rect.w <= 0 || group_rect.h <= 0) return;
    group_button_top = group_rect.y + group_rect.h - (button_height * UIPanel_FileControlsRowCount(group));
    group_button_top -= button_spacing * (UIPanel_FileControlsRowCount(group) - 1);
    if (group_button_top < group_rect.y) group_button_top = group_rect.y;

    for (int i = 0; i < ui->count; ++i) {
        UIButton* button = &ui->buttons[i];
        UIPanelFileControlRowSpec row_spec = {0, 0, 0};
        if (button->side != UI_PANEL_LEFT || button->group != group) continue;
        if (!UIPanel_FileControlRowSpecForButton(button->id, &row_spec)) continue;
        if (row_start >= 0 && row_spec.row_key == row_start) continue;

        {
            int row_indices[2] = {-1, -1};
            int row_widths[2] = {0, 0};
            int row_total_width = 0;
            int row_x = group_rect.x;
            int row_y = group_button_top;

            for (int j = 0; j < ui->count; ++j) {
                UIButton* place = &ui->buttons[j];
                UIPanelFileControlRowSpec place_spec = {0, 0, 0};
                if (place->side != UI_PANEL_LEFT || place->group != group) continue;
                if (!UIPanel_FileControlRowSpecForButton(place->id, &place_spec)) continue;
                if (place_spec.row_key != row_spec.row_key) continue;
                if (place_spec.column_index < 0 || place_spec.column_index >= 2) continue;
                row_indices[place_spec.column_index] = j;
                row_widths[place_spec.column_index] = UIPanel_FileControlsButtonWidthPx(place, text_pad_x);
            }

            if (row_spec.columns <= 1) {
                if (row_indices[0] >= 0) {
                    ui->buttons[row_indices[0]].bounds = (SDL_Rect){
                        group_rect.x,
                        row_y,
                        group_rect.w,
                        button_height
                    };
                    group_button_top += button_height + button_spacing;
                }
                row_start = row_spec.row_key;
                continue;
            }

            row_total_width = row_widths[0] + row_widths[1] + compact_gap;
            if (row_total_width > group_rect.w) {
                int cell_width = (group_rect.w - compact_gap) / 2;
                if (cell_width < 22) cell_width = 22;
                row_widths[0] = cell_width;
                row_widths[1] = cell_width;
                row_total_width = row_widths[0] + row_widths[1] + compact_gap;
            }
            if (row_total_width < group_rect.w) {
                row_x = group_rect.x + (group_rect.w - row_total_width) / 2;
            }
            if (row_indices[0] >= 0) {
                ui->buttons[row_indices[0]].bounds = (SDL_Rect){
                    row_x,
                    row_y,
                    row_widths[0],
                    button_height
                };
            }
            if (row_indices[1] >= 0) {
                ui->buttons[row_indices[1]].bounds = (SDL_Rect){
                    row_x + row_widths[0] + compact_gap,
                    row_y,
                    row_widths[1],
                    button_height
                };
            }
            group_button_top += button_height + button_spacing;
            row_start = row_spec.row_key;
        }
    }
}

int UIPanel_FileControlsButtonHeightPx(const UIPanelLayoutMetrics* metrics) {
    int button_height = 16;
    if (!metrics) return button_height;
    button_height = metrics->button_height_px - 2;
    if (button_height < 16) button_height = 16;
    return button_height;
}

int UIPanel_FileControlsSectionHeight(const UIPanelLayoutMetrics* metrics, UIPanelGroup group) {
    int rows = 0;
    int button_height = 0;
    if (!metrics) return 0;
    rows = UIPanel_FileControlsRowCount(group);
    if (rows <= 0) return 0;
    button_height = UIPanel_FileControlsButtonHeightPx(metrics);
    return metrics->group_header_height_px +
           (button_height * rows) +
           (metrics->button_spacing_px * (rows - 1));
}

void UIPanel_LayoutFilePaneButtons(UIPanelState* ui,
                                   const UIPanelLayoutMetrics* metrics,
                                   int text_pad_x) {
    if (!ui || !metrics) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_FILE) return;

    UIPanel_FileControlsLayoutGroup(ui,
                                    UI_PANEL_GROUP_LEFT_FILE_IO,
                                    ui->filePane.fileActionsRect,
                                    UIPanel_FileControlsButtonHeightPx(metrics),
                                    metrics->button_spacing_px,
                                    metrics->compact_row_gap_px,
                                    text_pad_x);
    UIPanel_FileControlsLayoutGroup(ui,
                                    UI_PANEL_GROUP_LEFT_ROOT_PATHS,
                                    ui->filePane.rootPathsRect,
                                    UIPanel_FileControlsButtonHeightPx(metrics),
                                    metrics->button_spacing_px,
                                    metrics->compact_row_gap_px,
                                    text_pad_x);
}
