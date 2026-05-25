#include "UI/ui_panel_create_summary.h"
#include "UI/render_ui_panel.h"
#include "UI/ui_panel_file_summary.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_object_inspector.h"
#include "UI/ui_panel_scene_summary.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/ui_panel_view_summary.h"
#include "UI/font_manager.h"
#include "UI/text_draw.h"
#include "Core/global_state.h"

#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void DrawTextBasic(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void UIPanel_DrawTextWithOptionalClip(SDL_Renderer* renderer,
                                             TTF_Font* font,
                                             const char* text,
                                             SDL_Rect clip_rect,
                                             int x,
                                             int y,
                                             SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    if (clip_rect.w > 0 && clip_rect.h > 0) {
        SDL_Rect previous_clip = {0, 0, 0, 0};
        SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer);
        if (had_clip) {
            (void)SDL_RenderGetClipRect(renderer, &previous_clip);
        }
        SDL_RenderSetClipRect(renderer, &clip_rect);
        DrawTextBasic(renderer, font, text, x, y, color);
        SDL_RenderSetClipRect(renderer, had_clip ? &previous_clip : NULL);
        return;
    }
    DrawTextBasic(renderer, font, text, x, y, color);
}

static ViewPlane UIPanel_RenderCurrentConstructionViewPlane(const GlobalState* state) {
    if (state && Layout_ConstructionPlane3D_IsValid(&state->layout.scene3d.constructionPlane)) {
        return Layout_ConstructionPlane3D_ToViewPlane(&state->layout.scene3d.constructionPlane);
    }
    if (state) {
        return state->activePlane;
    }
    return (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
}

static const char* UIPanel_RenderPlaneAxisLabel(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "YZ";
        case VIEW_PLANE_XZ: return "XZ";
        case VIEW_PLANE_XY:
        default: return "XY";
    }
}

static const char* UIPanel_RenderPlaneCoordinateLabel(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "x";
        case VIEW_PLANE_XZ: return "y";
        case VIEW_PLANE_XY:
        default: return "z";
    }
}

static void DrawButton(SDL_Renderer* r, const UIButton* btn) {
    UIPanelVisualPalette palette = {0};
    SDL_Color button_fill = {70, 70, 70, 200};
    SDL_Color button_border = {180, 180, 180, 255};
    SDL_Color textColor = {255, 255, 255, 255};
    TTF_Font* font = FontManager_GetUIPanelFont();
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);

    (void)UIPanelVisual_ResolvePalette(&palette);
    button_fill = palette.button_fill;
    button_border = palette.button_border;
    textColor = palette.text_primary;
    if (btn->hovered) {
        button_fill = palette.button_fill_hover;
        button_border = UIPanelVisual_AdjustColor(button_border, 18, 0);
    }

    UIPanelVisual_DrawFrame(r, btn->bounds, button_fill, button_border, 80);
    if (btn->hovered) {
        SDL_Rect accent = { btn->bounds.x, btn->bounds.y, 3, btn->bounds.h };
        UIPanelVisual_DrawAccentBand(r, accent, palette.accent, 0, accent.h, 220);
    }

    if (!font) return;

    char dynamicLabel[64];
    const char* label = btn->label;
    if (btn->id == UI_BTN_TOGGLE_SPACE_MODE) {
        GlobalState* state = Global_Get();
        const char* modeLabel = state ? Global_GetSpaceModeLabel(state->spaceMode) : "3D";
        snprintf(dynamicLabel, sizeof(dynamicLabel), "%s (M)", modeLabel);
        label = dynamicLabel;
    } else if (btn->id == UI_BTN_TOGGLE_OBJECT_GIZMO_MODE) {
        GlobalState* state = Global_Get();
        const bool rotateMode = state ? state->editor.object3DRotateMode : false;
        snprintf(dynamicLabel,
                 sizeof(dynamicLabel),
                 "%s (X)",
                 rotateMode ? "Rotate" : "Move");
        label = dynamicLabel;
    } else if (btn->id == UI_BTN_TOGGLE_SCENE_BOUNDS) {
        GlobalState* state = Global_Get();
        const bool enabled = state ? state->layout.scene3d.bounds.enabled : false;
        snprintf(dynamicLabel,
                 sizeof(dynamicLabel),
                 "Bounds: %s",
                 enabled ? "On" : "Off");
        label = dynamicLabel;
    } else if (btn->id == UI_BTN_TOGGLE_SCENE_BOUNDS_CLAMP) {
        GlobalState* state = Global_Get();
        const bool clampOnEdit = state ? state->layout.scene3d.bounds.clampOnEdit : false;
        snprintf(dynamicLabel,
                 sizeof(dynamicLabel),
                 "Clamp: %s",
                 clampOnEdit ? "On" : "Off");
        label = dynamicLabel;
    } else if (btn->id == UI_BTN_CYCLE_DISPLAY_UNITS) {
        snprintf(dynamicLabel,
                 sizeof(dynamicLabel),
                 "%s",
                 UIPanel_GetDisplayUnitSymbol());
        label = dynamicLabel;
    } else if (btn->id == UI_BTN_EDIT_PRISM_WIDTH ||
               btn->id == UI_BTN_EDIT_PRISM_HEIGHT ||
               btn->id == UI_BTN_EDIT_PRISM_DEPTH) {
        const char* axis = "W";
        if (btn->id == UI_BTN_EDIT_PRISM_HEIGHT) axis = "H";
        if (btn->id == UI_BTN_EDIT_PRISM_DEPTH) axis = "D";
        snprintf(dynamicLabel, sizeof(dynamicLabel), "%s", axis);
        label = dynamicLabel;
    } else if (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XY ||
               btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_YZ ||
               btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XZ) {
        const ViewPlane plane = UIPanel_RenderCurrentConstructionViewPlane(Global_Get());
        const ViewPlaneAxis buttonAxis =
            (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_YZ) ? VIEW_PLANE_YZ :
            (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XZ) ? VIEW_PLANE_XZ :
            VIEW_PLANE_XY;
        snprintf(dynamicLabel,
                 sizeof(dynamicLabel),
                 "%s%s%s",
                 (plane.axis == buttonAxis) ? "[" : "",
                 UIPanel_RenderPlaneAxisLabel(buttonAxis),
                 (plane.axis == buttonAxis) ? "]" : "");
        label = dynamicLabel;
    } else if (btn->id == UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET) {
        const ViewPlane plane = UIPanel_RenderCurrentConstructionViewPlane(Global_Get());
        snprintf(dynamicLabel,
                 sizeof(dynamicLabel),
                 "Edit %s",
                 UIPanel_RenderPlaneCoordinateLabel(plane.axis));
        label = dynamicLabel;
    }

    {
        int textW = 0;
        int textH = 0;
        const int maxTextW = btn->bounds.w - ((metrics.pad_x - 2) * 2);
        SDL_Rect dst = {0, 0, 0, 0};
        SDL_Rect textClip = {
            btn->bounds.x + metrics.pad_x - 2,
            btn->bounds.y + 1,
            btn->bounds.w - ((metrics.pad_x - 2) * 2),
            btn->bounds.h - 2
        };
        if (!line_drawing_text_measure_utf8(r, font, label, &textW, &textH)) {
            return;
        }
        dst.w = textW;
        dst.h = textH;
        if (maxTextW > 0 && textW > maxTextW) {
            dst.x = btn->bounds.x + metrics.pad_x - 2;
        } else {
            dst.x = btn->bounds.x + (btn->bounds.w - dst.w) / 2;
        }
        dst.y = btn->bounds.y + (btn->bounds.h - dst.h) / 2;
        UIPanel_DrawTextWithOptionalClip(r, font, label, textClip, dst.x, dst.y, textColor);
    }
}

static void DrawTabButton(SDL_Renderer* renderer, TTF_Font* font, const UIPanelTabButton* tab) {
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Color fill = {42, 45, 52, 225};
    SDL_Color border = {110, 120, 138, 255};
    SDL_Color text = {220, 224, 232, 255};

    if (!renderer || !font || !tab || tab->bounds.w <= 0 || tab->bounds.h <= 0) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    fill = tab->active
               ? UIPanelVisual_BlendColor(palette.pane_fill, palette.button_fill_active, 96)
               : palette.pane_fill;
    border = tab->active ? palette.button_border : palette.pane_border;
    text = tab->active ? palette.text_primary : palette.text_muted;
    if (UIPanelVisual_ColorLuma(fill) > 210) {
        fill = UIPanelVisual_AdjustColor(fill, -36, 0);
    }

    UIPanelVisual_DrawFrame(renderer, tab->bounds, fill, border, 70);
    if (tab->active) {
        SDL_Rect accent = { tab->bounds.x + 2, tab->bounds.y + tab->bounds.h - metrics.accent_h, tab->bounds.w - 4, metrics.accent_h - 1 };
        UIPanelVisual_DrawAccentBand(renderer, accent, palette.accent, 0, accent.h, 255);
    }

    {
        int textW = 0;
        int textH = 0;
        if (!line_drawing_text_measure_utf8(renderer, font, tab->label, &textW, &textH)) return;
        DrawTextBasic(renderer,
                      font,
                      tab->label,
                      tab->bounds.x + (tab->bounds.w - textW) / 2,
                      tab->bounds.y + (tab->bounds.h - textH) / 2,
                      text);
    }
}

static void DrawPanelTabs(SDL_Renderer* renderer, const UIPanelState* ui, UIPanelSide side) {
    TTF_Font* font = FontManager_GetUIPanelFont();
    const UIPanelTabButton* tabs = NULL;
    int count = 0;
    if (!renderer || !ui || !font) return;

    if (side == UI_PANEL_LEFT) {
        tabs = ui->leftTabs;
        count = UI_PANEL_LEFT_TAB_COUNT;
    } else {
        tabs = ui->rightTabs;
        count = UI_PANEL_RIGHT_TAB_COUNT;
    }
    for (int i = 0; i < count; ++i) {
        DrawTabButton(renderer, font, &tabs[i]);
    }
}

static void DrawPaneSurface(SDL_Renderer* renderer,
                            const SDL_Rect* pane_rect,
                            const SDL_Rect* body_rect) {
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(FontManager_GetUIPanelFont());
    SDL_Color fill = {18, 20, 25, 215};
    SDL_Color border = {78, 90, 108, 220};
    SDL_Color divider = {62, 72, 88, 210};

    if (!renderer || !pane_rect || pane_rect->w <= 0 || pane_rect->h <= 0) return;
    (void)UIPanelVisual_ResolvePalette(&palette);
    fill = palette.pane_fill;
    border = palette.pane_border;
    divider = palette.pane_divider;
    UIPanelVisual_DrawFrame(renderer, *pane_rect, fill, border, 70);

    if (body_rect && body_rect->w > 0 && body_rect->h > 0) {
        const int divider_y = body_rect->y - metrics.section_gap;
        UIPanelVisual_DrawDividerLine(renderer,
                                      pane_rect->x + metrics.pad_x,
                                      pane_rect->x + pane_rect->w - metrics.pad_x,
                                      divider_y,
                                      divider,
                                      divider.a);
    }
}

static const char* UIPanel_GroupTitle(UIPanelGroup group, UIPanelSide side) {
    (void)side;
    switch (group) {
        case UI_PANEL_GROUP_LEFT_SCENE_SELECTION: return "Selection";
        case UI_PANEL_GROUP_LEFT_SCENE_BOUNDS: return "Scene Bounds";
        case UI_PANEL_GROUP_LEFT_FILE_IO: return "File / IO";
        case UI_PANEL_GROUP_LEFT_ROOT_PATHS: return "Session Paths";
        case UI_PANEL_GROUP_RIGHT_VIEW: return "View";
        case UI_PANEL_GROUP_RIGHT_MODES: return "Modes";
        case UI_PANEL_GROUP_RIGHT_PRIMITIVES: return "Primitives";
        case UI_PANEL_GROUP_RIGHT_CONSTRUCTION: return "Construction";
        case UI_PANEL_GROUP_RIGHT_PRISM: return "Prism";
        case UI_PANEL_GROUP_RIGHT_GIZMO: return "Gizmo";
        case UI_PANEL_GROUP_RIGHT_TRANSFORM: return "Transform";
        case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS: return "Object Actions";
        case UI_PANEL_GROUP_NONE:
        default: return "Controls";
    }
}

static void DrawGroupSection(SDL_Renderer* renderer,
                             const UIPanelState* ui,
                             UIPanelSide side,
                             UIPanelGroup group,
                             int first_index,
                             int last_index) {
    UIPanelVisualPalette palette = {0};
    SDL_Color title_color = {200, 200, 210, 255};
    SDL_Color panel_fill = {25, 28, 34, 145};
    SDL_Color panel_border = {80, 95, 115, 210};
    SDL_Rect bounds = {0, 0, 0, 0};
    int min_x = INT_MAX;
    int min_y = INT_MAX;
    int max_x = 0;
    int max_y = 0;
    TTF_Font* font = NULL;
    const char* title = UIPanel_GroupTitle(group, side);
    UIPanelVisualMetrics metrics = {0};
    int title_band_h = 16;
    int title_w = 0;
    SDL_Rect title_chip = { bounds.x + 6, bounds.y + 3, 0, 0 };

    if (!renderer || !ui || first_index < 0 || last_index < first_index) return;

    for (int i = first_index; i <= last_index && i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != side || btn->group != group) continue;
        if (btn->bounds.x < min_x) min_x = btn->bounds.x;
        if (btn->bounds.y < min_y) min_y = btn->bounds.y;
        if (btn->bounds.x + btn->bounds.w > max_x) max_x = btn->bounds.x + btn->bounds.w;
        if (btn->bounds.y + btn->bounds.h > max_y) max_y = btn->bounds.y + btn->bounds.h;
    }
    if (min_x == INT_MAX || max_x <= min_x || max_y <= min_y) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    title_color = palette.text_muted;
    panel_fill = palette.pane_fill;
    panel_border = palette.pane_border;
    panel_fill.a = 145;
    panel_border.a = 210;

    font = FontManager_GetUIPanelFont();
    metrics = UIPanelVisual_MakeMetrics(font);
    title_band_h = metrics.chip_h + 2;
    bounds.x = min_x - 4;
    bounds.y = min_y - title_band_h;
    bounds.w = (max_x - min_x) + 8;
    bounds.h = (max_y - min_y) + title_band_h + 4;
    title_chip.x = bounds.x + metrics.pad_x - 2;
    title_chip.y = bounds.y + 3;

    UIPanelVisual_DrawFrame(renderer, bounds, panel_fill, panel_border, 60);

    if (font) {
        if (line_drawing_text_measure_utf8(renderer, font, title, &title_w, NULL)) {
            title_chip = (SDL_Rect){ bounds.x + metrics.pad_x - 2, bounds.y + 3, title_w + 12, metrics.chip_h };
            UIPanelVisual_DrawLabelChip(renderer,
                                        title_chip,
                                        UIPanelVisual_BlendColor(panel_fill, panel_border, 38),
                                        panel_border,
                                        110,
                                        130);
        }
        DrawTextBasic(renderer, font, title, title_chip.x + 4, bounds.y + 4, title_color);
    }
}

void Render_UIPanelSide(const UIPanelState* ui, SDL_Renderer* renderer, UIPanelSide side) {
    int i = 0;
    if (!ui || !renderer) return;
    if (side == UI_PANEL_LEFT) {
        DrawPaneSurface(renderer, &ui->leftPaneRect, &ui->leftBodyRect);
    } else {
        DrawPaneSurface(renderer, &ui->rightPaneRect, &ui->rightBodyRect);
    }
    DrawPanelTabs(renderer, ui, side);
    while (i < ui->count) {
        int first = 0;
        int last = 0;
        UIPanelGroup group = UI_PANEL_GROUP_NONE;
        while (i < ui->count && ui->buttons[i].side != side) {
            ++i;
        }
        if (i >= ui->count) break;
        if (!UIPanel_ShouldShowGroup(ui, ui->buttons[i].group)) {
            ++i;
            continue;
        }

        first = i;
        group = ui->buttons[i].group;
        last = i;
        while (last + 1 < ui->count &&
               ui->buttons[last + 1].side == side &&
               ui->buttons[last + 1].group == group) {
            ++last;
        }

        DrawGroupSection(renderer, ui, side, group, first, last);
        for (int j = first; j <= last; ++j) {
            DrawButton(renderer, &ui->buttons[j]);
        }

        i = last + 1;
    }
}

void Render_UIPanelRootSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;
    if (!UIPanel_ShouldRenderRootSummary(ui)) return;
    Render_UIPanelFileSummary(ui, renderer);
}

void Render_UIPanelObjectSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;
    if (!UIPanel_ShouldRenderObjectSummary(ui)) return;
    Render_UIPanelObjectInspector(ui, renderer);
}

static void Render_UIPanelRightTabSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;
    Render_UIPanelViewSummary(ui, renderer);
    Render_UIPanelCreateSummary(ui, renderer);
    Render_UIPanelObjectInspector(ui, renderer);
}

void Render_UIPanel(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;
    Render_UIPanelSide(ui, renderer, UI_PANEL_LEFT);
    Render_UIPanelSide(ui, renderer, UI_PANEL_RIGHT);
    Render_UIPanelSceneSummary(ui, renderer);
    Render_UIPanelSceneList(ui, renderer);
    Render_UIPanelRootSummary(ui, renderer);
    Render_UIPanelFileBrowser(ui, renderer);
    Render_UIPanelRightTabSummary(ui, renderer);
}
