#include "UI/render_ui_panel.h"
#include "UI/ui_panel.h"
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

static void DrawTextClipped(SDL_Renderer* renderer,
                            TTF_Font* font,
                            const char* text,
                            int x,
                            int y,
    int maxWidth,
    SDL_Color color) {
    char clipped[512];
    BuildEllipsizedText(renderer, font, text, maxWidth, clipped, sizeof(clipped));
    DrawTextBasic(renderer, font, clipped, x, y, color);
}

static int UIPanel_FontHeightPx(TTF_Font* font) {
    int h = 14;
    if (font) {
        h = TTF_FontHeight(font);
    }
    if (h < 12) h = 12;
    return h;
}

static int UIPanel_LineGapPx(TTF_Font* font) {
    int gap = UIPanel_FontHeightPx(font) / 3;
    if (gap < 4) gap = 4;
    return gap;
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

static const char* Object3DKindLabel(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_RECT_PRISM: return "RectPrism";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static void FormatDimensionDisplay(float world_value, char* out, size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.2f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.2f", world_value);
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

    // ─── Button Background ─────────────────────
    SDL_SetRenderDrawColor(r, button_fill.r, button_fill.g, button_fill.b, button_fill.a);
    SDL_RenderFillRect(r, &btn->bounds);

    // ─── Button Border ─────────────────────────
    SDL_SetRenderDrawColor(r, button_border.r, button_border.g, button_border.b, button_border.a);
    SDL_RenderDrawRect(r, &btn->bounds);

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

static const char* UIPanel_GroupTitle(UIPanelGroup group, UIPanelSide side) {
    (void)side;
    switch (group) {
        case UI_PANEL_GROUP_LEFT_FILE_IO: return "File / IO";
        case UI_PANEL_GROUP_LEFT_ROOT_PATHS: return "Root Paths";
        case UI_PANEL_GROUP_RIGHT_VIEW: return "View";
        case UI_PANEL_GROUP_RIGHT_MODES: return "Modes";
        case UI_PANEL_GROUP_RIGHT_PRIMITIVES: return "Primitives";
        case UI_PANEL_GROUP_RIGHT_CONSTRUCTION: return "Construction";
        case UI_PANEL_GROUP_RIGHT_PRISM: return "Prism";
        case UI_PANEL_GROUP_RIGHT_GIZMO: return "Gizmo";
        case UI_PANEL_GROUP_RIGHT_TRANSFORM: return "Transform";
        case UI_PANEL_GROUP_RIGHT_BOUNDS: return "Scene Bounds";
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
        DrawTextBasic(renderer, font, title, bounds.x + 8, bounds.y + 4, title_color);
    }
}

static void RenderRootSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color labelColor = {200, 200, 210, 255};
    SDL_Color valueColor = {230, 230, 235, 255};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect panel = {0, 0, 0, 0};
    int minX = INT_MAX;
    int maxX = 0;
    int maxY = 0;
    int font_h = 14;
    int line_gap = 4;
    int panel_pad = 8;
    int line0_y = 0;
    int line1_y = 0;
    int line2_y = 0;
    int line3_y = 0;
    if (!ui || !renderer || !font) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        labelColor = palette.text_muted;
        valueColor = palette.text_primary;
    }

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_LEFT) continue;
        if (btn->bounds.x < minX) minX = btn->bounds.x;
        if (btn->bounds.x + btn->bounds.w > maxX) maxX = btn->bounds.x + btn->bounds.w;
        if (btn->bounds.y + btn->bounds.h > maxY) maxY = btn->bounds.y + btn->bounds.h;
    }
    if (minX == INT_MAX || maxX <= minX) return;

    panel.x = minX;
    panel.y = maxY + 8;
    panel.w = maxX - minX;
    font_h = UIPanel_FontHeightPx(font);
    line_gap = UIPanel_LineGapPx(font);
    panel_pad = font_h / 2;
    if (panel_pad < 8) panel_pad = 8;
    panel.h = (panel_pad * 2) + (font_h * 4) + (line_gap * 3);
    line0_y = panel.y + panel_pad;
    line1_y = line0_y + font_h + line_gap;
    line2_y = line1_y + font_h + line_gap;
    line3_y = line2_y + font_h + line_gap;

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, 200);
        SDL_RenderDrawRect(renderer, &panel);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 200);
        SDL_RenderDrawRect(renderer, &panel);
    }

    DrawTextBasic(renderer, font, "Input Root:", panel.x + 8, line0_y, labelColor);
    DrawTextClipped(renderer,
                    font,
                    state && Global_GetInputRoot() ? Global_GetInputRoot() : "(unset)",
                    panel.x + 8,
                    line1_y,
                    panel.w - 16,
                    valueColor);
    DrawTextBasic(renderer, font, "Output Root:", panel.x + 8, line2_y, labelColor);
    DrawTextClipped(renderer,
                    font,
                    state && Global_GetOutputRoot() ? Global_GetOutputRoot() : "(unset)",
                    panel.x + 8,
                    line3_y,
                    panel.w - 16,
                    valueColor);
}

static void RenderObjectSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color labelColor = {200, 200, 210, 255};
    SDL_Color valueColor = {230, 230, 235, 255};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect panel = {0, 0, 0, 0};
    int minX = INT_MAX;
    int maxX = 0;
    int maxY = 0;
    const Object3D* object = NULL;
    int font_h = 14;
    int line_gap = 4;
    int panel_pad = 8;
    int line0_y = 0;
    int line1_y = 0;
    int line2_y = 0;
    int line3_y = 0;
    int line4_y = 0;
    int line5_y = 0;
    if (!ui || !renderer || !font || !state) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        labelColor = palette.text_muted;
        valueColor = palette.text_primary;
    }

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_RIGHT) continue;
        if (btn->bounds.x < minX) minX = btn->bounds.x;
        if (btn->bounds.x + btn->bounds.w > maxX) maxX = btn->bounds.x + btn->bounds.w;
        if (btn->bounds.y + btn->bounds.h > maxY) maxY = btn->bounds.y + btn->bounds.h;
    }
    if (minX == INT_MAX || maxX <= minX) return;

    panel.x = minX;
    panel.y = maxY + 8;
    panel.w = maxX - minX;
    font_h = UIPanel_FontHeightPx(font);
    line_gap = UIPanel_LineGapPx(font);
    panel_pad = font_h / 2;
    if (panel_pad < 8) panel_pad = 8;
    panel.h = (panel_pad * 2) + (font_h * 6) + (line_gap * 5);
    line0_y = panel.y + panel_pad;
    line1_y = line0_y + font_h + line_gap;
    line2_y = line1_y + font_h + line_gap;
    line3_y = line2_y + font_h + line_gap;
    line4_y = line3_y + font_h + line_gap;
    line5_y = line4_y + font_h + line_gap;

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, 200);
        SDL_RenderDrawRect(renderer, &panel);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 200);
        SDL_RenderDrawRect(renderer, &panel);
    }

    DrawTextBasic(renderer, font, "Object Context:", panel.x + 8, line0_y, labelColor);
    if (state->editor.selectedObject3DId != 0u) {
        object = Layout_ObjectStore_FindConst(&state->layout.objectStore, state->editor.selectedObject3DId);
    }
    if (!object) {
        DrawTextClipped(renderer,
                        font,
                        "No object selected.",
                        panel.x + 8,
                        line1_y,
                        panel.w - 16,
                        valueColor);
        DrawTextClipped(renderer,
                        font,
                        "Select a plane/prism or use Add Plane/Add Prism.",
                        panel.x + 8,
                        line2_y,
                        panel.w - 16,
                        labelColor);
        return;
    }

    {
        char lineA[160];
        char lineB[160];
        char lineC[160];
        char lineD[160];
        char lineE[160];
        char wText[32] = {0};
        char hText[32] = {0};
        char dText[32] = {0};
        bool lockPlane = false;
        bool lockBounds = false;
        snprintf(lineA,
                 sizeof(lineA),
                 "ID:%u  Kind:%s",
                 object->objectId,
                 Object3DKindLabel(object->kind));
        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            FormatDimensionDisplay(object->rectPrism.width, wText, sizeof(wText));
            FormatDimensionDisplay(object->rectPrism.height, hText, sizeof(hText));
            FormatDimensionDisplay(object->rectPrism.depth, dText, sizeof(dText));
            lockPlane = object->rectPrism.lockToConstructionPlane;
            lockBounds = object->rectPrism.lockToBounds;
        } else {
            FormatDimensionDisplay(object->plane.width, wText, sizeof(wText));
            FormatDimensionDisplay(object->plane.height, hText, sizeof(hText));
            snprintf(dText, sizeof(dText), "n/a");
            lockPlane = object->plane.lockToConstructionPlane;
            lockBounds = object->plane.lockToBounds;
        }
        snprintf(lineB, sizeof(lineB), "W:%s  H:%s  D:%s", wText, hText, dText);
        snprintf(lineC,
                 sizeof(lineC),
                 "P:(%.2f, %.2f, %.2f)",
                 object->transform.position.x,
                 object->transform.position.y,
                 object->transform.position.z);
        snprintf(lineD,
                 sizeof(lineD),
                 "R:(%.1f, %.1f, %.1f)",
                 object->transform.rotationDeg.x,
                 object->transform.rotationDeg.y,
                 object->transform.rotationDeg.z);
        snprintf(lineE,
                 sizeof(lineE),
                 "LockPlane:%s  BoundsLock:%s",
                 lockPlane ? "Yes" : "No",
                 lockBounds ? "Yes" : "No");

        DrawTextClipped(renderer, font, lineA, panel.x + 8, line1_y, panel.w - 16, valueColor);
        DrawTextClipped(renderer, font, lineB, panel.x + 8, line2_y, panel.w - 16, valueColor);
        DrawTextClipped(renderer, font, lineC, panel.x + 8, line3_y, panel.w - 16, valueColor);
        DrawTextClipped(renderer, font, lineD, panel.x + 8, line4_y, panel.w - 16, valueColor);
        DrawTextClipped(renderer, font, lineE, panel.x + 8, line5_y, panel.w - 16, labelColor);
    }
}

void Render_UIPanelSide(const UIPanelState* ui, SDL_Renderer* renderer, UIPanelSide side) {
    int i = 0;
    if (!ui || !renderer) return;
    while (i < ui->count) {
        int first = 0;
        int last = 0;
        UIPanelGroup group = UI_PANEL_GROUP_NONE;
        while (i < ui->count && ui->buttons[i].side != side) {
            ++i;
        }
        if (i >= ui->count) break;

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
    RenderRootSummary(ui, renderer);
}

void Render_UIPanelObjectSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;
    RenderObjectSummary(ui, renderer);
}

void Render_UIPanel(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;
    Render_UIPanelSide(ui, renderer, UI_PANEL_LEFT);
    Render_UIPanelSide(ui, renderer, UI_PANEL_RIGHT);
    Render_UIPanelRootSummary(ui, renderer);
}
