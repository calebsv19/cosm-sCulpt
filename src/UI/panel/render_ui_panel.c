#include "UI/ui_panel_create_summary.h"
#include "UI/render_ui_panel.h"
#include "UI/ui_panel_file_summary.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_object_inspector.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_view_summary.h"
#include "UI/font_manager.h"
#include "UI/text_draw.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"

#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void DrawTextBasic(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static Uint8 UIPanel_ClampColorChannel(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (Uint8)value;
}

static SDL_Color UIPanel_AdjustColor(SDL_Color color, int delta_rgb, int delta_alpha) {
    color.r = UIPanel_ClampColorChannel((int)color.r + delta_rgb);
    color.g = UIPanel_ClampColorChannel((int)color.g + delta_rgb);
    color.b = UIPanel_ClampColorChannel((int)color.b + delta_rgb);
    color.a = UIPanel_ClampColorChannel((int)color.a + delta_alpha);
    return color;
}

static void BuildEllipsizedText(SDL_Renderer* renderer,
                                TTF_Font* font,
                                const char* text,
                                int maxWidth,
                                char* out,
                                size_t outSize) {
    static const char* k_ellipsis = "...";
    int width = 0;
    int ellipsisWidth = 0;
    size_t len = 0;
    if (!font || !text || !out || outSize == 0 || maxWidth <= 0) {
        if (out && outSize > 0) out[0] = '\0';
        return;
    }
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) && width <= maxWidth) {
        snprintf(out, outSize, "%s", text);
        return;
    }
    if (!line_drawing_text_measure_utf8(renderer, font, k_ellipsis, &ellipsisWidth, NULL) ||
        ellipsisWidth >= maxWidth) {
        out[0] = '\0';
        return;
    }
    len = strlen(text);
    while (len > 0) {
        --len;
        if (len + strlen(k_ellipsis) + 1 >= outSize) continue;
        memcpy(out, text, len);
        out[len] = '\0';
        strcat(out, k_ellipsis);
        if (line_drawing_text_measure_utf8(renderer, font, out, &width, NULL) && width <= maxWidth) {
            return;
        }
    }
    out[0] = '\0';
}

static int UIPanel_FontHeightPx(TTF_Font* font) {
    int h = 14;
    if (font) {
        h = TTF_FontHeight(font);
    }
    if (h < 12) h = 12;
    return h;
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
    LineDrawing3dThemePalette palette = {0};
    SDL_Color button_fill = {70, 70, 70, 200};
    SDL_Color button_border = {180, 180, 180, 255};
    SDL_Color textColor = {255, 255, 255, 255};
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        button_fill = palette.button_fill;
        button_border = palette.button_border;
        textColor = palette.button_text;
    }
    if (btn->hovered) {
        button_fill = UIPanel_AdjustColor(button_fill, 12, 20);
        button_border = UIPanel_AdjustColor(button_border, 26, 0);
    }

    // ─── Button Background ─────────────────────
    SDL_SetRenderDrawColor(r, button_fill.r, button_fill.g, button_fill.b, button_fill.a);
    SDL_RenderFillRect(r, &btn->bounds);

    // ─── Button Border ─────────────────────────
    SDL_SetRenderDrawColor(r, button_border.r, button_border.g, button_border.b, button_border.a);
    SDL_RenderDrawRect(r, &btn->bounds);
    if (btn->hovered) {
        SDL_Rect accent = { btn->bounds.x, btn->bounds.y, 3, btn->bounds.h };
        SDL_SetRenderDrawColor(r, button_border.r, button_border.g, button_border.b, 220);
        SDL_RenderFillRect(r, &accent);
    }

    // ─── Button Label Text ─────────────────────
    TTF_Font* font = FontManager_GetUIPanelFont();
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
        char clippedLabel[128];
        int textW = 0;
        int textH = 0;
        const int maxTextW = btn->bounds.w - 8;
        SDL_Rect dst = {0, 0, 0, 0};
        if (!line_drawing_text_measure_utf8(r, font, label, &textW, &textH)) {
            return;
        }
        dst.w = textW;
        dst.h = textH;
        if (maxTextW > 0 && textW > maxTextW) {
            BuildEllipsizedText(r, font, label, maxTextW, clippedLabel, sizeof(clippedLabel));
            label = clippedLabel;
            if (!line_drawing_text_measure_utf8(r, font, label, &textW, &textH)) {
                return;
            }
            dst.w = textW;
            dst.h = textH;
        }
        dst.x = btn->bounds.x + (btn->bounds.w - dst.w) / 2;
        dst.y = btn->bounds.y + (btn->bounds.h - dst.h) / 2;
        DrawTextBasic(r, font, label, dst.x, dst.y, textColor);
    }
}

static void DrawTabButton(SDL_Renderer* renderer, TTF_Font* font, const UIPanelTabButton* tab) {
    LineDrawing3dThemePalette palette = {0};
    SDL_Color fill = {42, 45, 52, 225};
    SDL_Color border = {110, 120, 138, 255};
    SDL_Color text = {220, 224, 232, 255};

    if (!renderer || !font || !tab || tab->bounds.w <= 0 || tab->bounds.h <= 0) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        fill = tab->active ? palette.button_fill : palette.panel_fill;
        border = tab->active ? palette.button_border : palette.panel_border;
        text = tab->active ? palette.button_text : palette.text_muted;
    }

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &tab->bounds);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &tab->bounds);
    if (tab->active) {
        SDL_Rect accent = {
            tab->bounds.x + 2,
            tab->bounds.y + tab->bounds.h - 4,
            tab->bounds.w - 4,
            3
        };
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
        SDL_RenderFillRect(renderer, &accent);
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
    LineDrawing3dThemePalette palette = {0};
    SDL_Color fill = {18, 20, 25, 215};
    SDL_Color border = {78, 90, 108, 220};
    SDL_Color divider = {62, 72, 88, 210};
    SDL_Rect inner = {0, 0, 0, 0};

    if (!renderer || !pane_rect || pane_rect->w <= 0 || pane_rect->h <= 0) return;
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        fill = palette.panel_fill;
        fill.a = 215;
        border = palette.panel_border;
        divider = UIPanel_AdjustColor(palette.panel_border, -10, -20);
    }

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, pane_rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, pane_rect);

    inner = *pane_rect;
    inner.x += 1;
    inner.y += 1;
    inner.w -= 2;
    inner.h -= 2;
    if (inner.w > 0 && inner.h > 0) {
        SDL_SetRenderDrawColor(renderer, divider.r, divider.g, divider.b, 70);
        SDL_RenderDrawRect(renderer, &inner);
    }

    if (body_rect && body_rect->w > 0 && body_rect->h > 0) {
        const int divider_y = body_rect->y - 6;
        SDL_SetRenderDrawColor(renderer, divider.r, divider.g, divider.b, divider.a);
        SDL_RenderDrawLine(renderer,
                           pane_rect->x + 8,
                           divider_y,
                           pane_rect->x + pane_rect->w - 8,
                           divider_y);
    }
}

static const char* UIPanel_GroupTitle(UIPanelGroup group, UIPanelSide side) {
    (void)side;
    switch (group) {
        case UI_PANEL_GROUP_LEFT_SCENE_BOUNDS: return "Scene Bounds";
        case UI_PANEL_GROUP_LEFT_FILE_IO: return "File / IO";
        case UI_PANEL_GROUP_LEFT_ROOT_PATHS: return "Root Paths";
        case UI_PANEL_GROUP_RIGHT_VIEW: return "View";
        case UI_PANEL_GROUP_RIGHT_MODES: return "Modes";
        case UI_PANEL_GROUP_RIGHT_PRIMITIVES: return "Primitives";
        case UI_PANEL_GROUP_RIGHT_CONSTRUCTION: return "Construction";
        case UI_PANEL_GROUP_RIGHT_PRISM: return "Prism";
        case UI_PANEL_GROUP_RIGHT_GIZMO: return "Gizmo";
        case UI_PANEL_GROUP_RIGHT_TRANSFORM: return "Transform";
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
    LineDrawing3dThemePalette palette = {0};
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
    int font_h = 14;
    int title_band_h = 16;
    int title_w = 0;
    SDL_Rect title_chip = {0, 0, 0, 0};

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

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        title_color = palette.text_muted;
        panel_fill = palette.panel_fill;
        panel_border = palette.panel_border;
        panel_fill.a = 145;
        panel_border.a = 210;
    }

    font = FontManager_GetUIPanelFont();
    font_h = UIPanel_FontHeightPx(font);
    title_band_h = font_h + 4;
    bounds.x = min_x - 4;
    bounds.y = min_y - title_band_h;
    bounds.w = (max_x - min_x) + 8;
    bounds.h = (max_y - min_y) + title_band_h + 4;

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(renderer, panel_fill.r, panel_fill.g, panel_fill.b, panel_fill.a);
    SDL_RenderFillRect(renderer, &bounds);
    SDL_SetRenderDrawColor(renderer, panel_border.r, panel_border.g, panel_border.b, panel_border.a);
    SDL_RenderDrawRect(renderer, &bounds);

    if (font) {
        if (line_drawing_text_measure_utf8(renderer, font, title, &title_w, NULL)) {
            title_chip = (SDL_Rect){ bounds.x + 6, bounds.y + 3, title_w + 10, font_h + 2 };
            SDL_SetRenderDrawColor(renderer, panel_border.r, panel_border.g, panel_border.b, 100);
            SDL_RenderFillRect(renderer, &title_chip);
        }
        DrawTextBasic(renderer, font, title, bounds.x + 8, bounds.y + 4, title_color);
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
    Render_UIPanelSceneList(ui, renderer);
    Render_UIPanelRootSummary(ui, renderer);
    Render_UIPanelRightTabSummary(ui, renderer);
}
