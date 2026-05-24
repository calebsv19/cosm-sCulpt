#include "UI/ui_panel_object_inspector.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelObjectInspector_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelObjectInspector_LineGap(void) {
    int gap = UIPanelObjectInspector_FontHeight() / 3;
    if (gap < 4) gap = 4;
    return gap;
}

static int UIPanelObjectInspector_PanelPad(void) {
    int pad = UIPanelObjectInspector_FontHeight() / 2;
    if (pad < 8) pad = 8;
    return pad;
}

static void UIPanelObjectInspector_DrawText(SDL_Renderer* renderer,
                                            TTF_Font* font,
                                            const char* text,
                                            int x,
                                            int y,
                                            SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void UIPanelObjectInspector_DrawTextClipped(SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   const char* text,
                                                   int x,
                                                   int y,
                                                   int maxWidth,
                                                   SDL_Color color) {
    char clipped[256];
    int width = 0;
    if (!renderer || !font || !text || maxWidth <= 0) return;
    if (line_drawing_text_measure_utf8(renderer, font, text, &width, NULL) && width <= maxWidth) {
        UIPanelObjectInspector_DrawText(renderer, font, text, x, y, color);
        return;
    }
    snprintf(clipped, sizeof(clipped), "%s", text);
    while (strlen(clipped) > 3) {
        size_t len = strlen(clipped);
        clipped[len - 1] = '\0';
        clipped[len - 2] = '.';
        clipped[len - 3] = '.';
        clipped[len - 4] = '.';
        if (line_drawing_text_measure_utf8(renderer, font, clipped, &width, NULL) && width <= maxWidth) {
            UIPanelObjectInspector_DrawText(renderer, font, clipped, x, y, color);
            return;
        }
    }
}

static const char* UIPanelObjectInspector_KindLabel(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_RECT_PRISM: return "RectPrism";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static void UIPanelObjectInspector_FormatDimension(float world_value,
                                                   char* out,
                                                   size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.2f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.2f", world_value);
    }
}

static int UIPanelObjectInspector_LineCount(const Object3D* object) {
    return object ? 7 : 4;
}

int UIPanel_ObjectInspectorReservedHeight(const UIPanelState* ui) {
    GlobalState* state = Global_Get();
    const Object3D* object = NULL;
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    int lines = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT || !state) return 0;
    if (state->editor.selectedObject3DId != 0u) {
        object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                              state->editor.selectedObject3DId);
    }
    font_h = UIPanelObjectInspector_FontHeight();
    line_gap = UIPanelObjectInspector_LineGap();
    pad = UIPanelObjectInspector_PanelPad();
    lines = UIPanelObjectInspector_LineCount(object);
    return (pad * 2) + (font_h * lines) + (line_gap * (lines - 1));
}

void Render_UIPanelObjectInspector(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    const Object3D* object = NULL;
    LineDrawing3dThemePalette palette = {0};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect panel = {0, 0, 0, 0};
    int font_h = 0;
    int line_gap = 0;
    int panel_pad = 0;
    int y = 0;
    int text_w = 0;
    if (!ui || !renderer || !state || !font) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return;
    if (ui->rightBodyRect.w <= 0 || ui->rightBodyRect.h <= 0) return;

    panel = ui->rightBodyRect;
    panel.h = UIPanel_ObjectInspectorReservedHeight(ui);
    if (panel.h <= 0) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        label_color = palette.text_muted;
        value_color = palette.text_primary;
        accent_color = palette.button_border;
    }

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
                               palette.panel_border.b, 210);
        SDL_RenderDrawRect(renderer, &panel);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 170);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 200);
        SDL_RenderDrawRect(renderer, &panel);
    }
    {
        SDL_Rect accent_band = { panel.x + 1, panel.y + 1, panel.w - 2, 4 };
        SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, 220);
        SDL_RenderFillRect(renderer, &accent_band);
    }

    font_h = UIPanelObjectInspector_FontHeight();
    line_gap = UIPanelObjectInspector_LineGap();
    panel_pad = UIPanelObjectInspector_PanelPad();
    y = panel.y + panel_pad;

    UIPanelObjectInspector_DrawText(renderer, font, "Selected Object", panel.x + 8, y, label_color);
    y += font_h + line_gap;

    if (state->editor.selectedObject3DId != 0u) {
        object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                              state->editor.selectedObject3DId);
    }
    if (!object) {
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "No object selected.",
                                               panel.x + 8,
                                               y,
                                               panel.w - 16,
                                               value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "Pick from the Scene list or click an object origin in the viewport.",
                                               panel.x + 8,
                                               y,
                                               panel.w - 16,
                                               label_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "Create new geometry from the Create tab when nothing is selected.",
                                               panel.x + 8,
                                               y,
                                               panel.w - 16,
                                               label_color);
        return;
    }

    {
        char line_id[96];
        char line_dims[160];
        char line_pos[160];
        char line_rot[160];
        char line_flags[160];
        char line_hint[160];
        char w_text[32] = {0};
        char h_text[32] = {0};
        char d_text[32] = {0};
        bool lock_plane = false;
        bool lock_bounds = false;

        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            UIPanelObjectInspector_FormatDimension(object->rectPrism.width, w_text, sizeof(w_text));
            UIPanelObjectInspector_FormatDimension(object->rectPrism.height, h_text, sizeof(h_text));
            UIPanelObjectInspector_FormatDimension(object->rectPrism.depth, d_text, sizeof(d_text));
            lock_plane = object->rectPrism.lockToConstructionPlane;
            lock_bounds = object->rectPrism.lockToBounds;
        } else {
            UIPanelObjectInspector_FormatDimension(object->plane.width, w_text, sizeof(w_text));
            UIPanelObjectInspector_FormatDimension(object->plane.height, h_text, sizeof(h_text));
            snprintf(d_text, sizeof(d_text), "n/a");
            lock_plane = object->plane.lockToConstructionPlane;
            lock_bounds = object->plane.lockToBounds;
        }

        snprintf(line_id,
                 sizeof(line_id),
                 "#%u  %s",
                 object->objectId,
                 UIPanelObjectInspector_KindLabel(object->kind));
        snprintf(line_dims,
                 sizeof(line_dims),
                 "Dimensions  W:%s  H:%s  D:%s",
                 w_text,
                 h_text,
                 d_text);
        snprintf(line_pos,
                 sizeof(line_pos),
                 "Position  %.2f, %.2f, %.2f",
                 object->transform.position.x,
                 object->transform.position.y,
                 object->transform.position.z);
        snprintf(line_rot,
                 sizeof(line_rot),
                 "Rotation  %.1f, %.1f, %.1f deg",
                 object->transform.rotationDeg.x,
                 object->transform.rotationDeg.y,
                 object->transform.rotationDeg.z);
        snprintf(line_flags,
                 sizeof(line_flags),
                 "Locks  Plane:%s  Bounds:%s",
                 lock_plane ? "On" : "Off",
                 lock_bounds ? "On" : "Off");
        snprintf(line_hint,
                 sizeof(line_hint),
                 "Use the controls below for typed edits and gizmo mode changes.");

        if (line_drawing_text_measure_utf8(renderer, font, line_id, &text_w, NULL) && text_w < panel.w - 16) {
            UIPanelObjectInspector_DrawText(renderer, font, line_id, panel.x + 8, y, accent_color);
        } else {
            UIPanelObjectInspector_DrawTextClipped(renderer, font, line_id, panel.x + 8, y, panel.w - 16, accent_color);
        }
        y += font_h + line_gap;
        SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, 90);
        SDL_RenderDrawLine(renderer, panel.x + 8, y - (line_gap / 2), panel.x + panel.w - 8, y - (line_gap / 2));
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_dims, panel.x + 8, y, panel.w - 16, value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_pos, panel.x + 8, y, panel.w - 16, value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_rot, panel.x + 8, y, panel.w - 16, value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_flags, panel.x + 8, y, panel.w - 16, label_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_hint, panel.x + 8, y, panel.w - 16, label_color);
    }
}
