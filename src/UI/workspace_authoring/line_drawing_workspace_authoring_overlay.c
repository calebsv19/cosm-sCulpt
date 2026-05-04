#include "UI/workspace_authoring/line_drawing_workspace_authoring_overlay.h"

#include <stdio.h>

#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include "core_pane_module.h"
#include "kit_workspace_authoring_ui.h"

enum {
    LINE_DRAWING_AUTHORING_PANE_ROW_CAP = 8
};

typedef struct LineDrawingAuthoringPaneRow {
    CorePaneRect pane_rect;
    uint32_t pane_id;
    const char* module_key;
    const char* module_label;
} LineDrawingAuthoringPaneRow;

static SDL_Rect line_drawing_authoring_sdl_rect(CorePaneRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Color line_drawing_authoring_color_alpha(SDL_Color color, Uint8 alpha) {
    color.a = alpha;
    return color;
}

static const CorePaneModuleBinding* line_drawing_authoring_binding_for_pane(
    const LineDrawingPaneHost* host,
    uint32_t pane_id) {
    uint32_t i;
    if (!host || pane_id == 0u) return NULL;
    for (i = 0u; i < host->module_binding_count; ++i) {
        if (host->module_bindings[i].pane_node_id == pane_id) {
            return &host->module_bindings[i];
        }
    }
    return NULL;
}

static const CorePaneModuleDescriptor* line_drawing_authoring_descriptor_for_binding(
    const LineDrawingPaneHost* host,
    const CorePaneModuleBinding* binding) {
    const CorePaneModuleDescriptor* descriptor = NULL;
    if (!host || !binding) return NULL;
    if (core_pane_module_find_by_type_id(&host->module_registry,
                                         binding->module_type_id,
                                         &descriptor) != CORE_PANE_MODULE_OK) {
        return NULL;
    }
    return descriptor;
}

static uint32_t line_drawing_authoring_build_rows(const GlobalState* state,
                                                  LineDrawingAuthoringPaneRow* rows,
                                                  uint32_t cap) {
    const LineDrawingPaneHost* host = NULL;
    uint32_t count = 0u;
    uint32_t i;
    if (!state || !rows || cap == 0u) return 0u;
    host = &state->paneHost;
    if (!host->initialized) return 0u;

    for (i = 0u; i < host->leaf_count && count < cap; ++i) {
        const CorePaneLeafRect* leaf = &host->leaves[i];
        const CorePaneModuleBinding* binding =
            line_drawing_authoring_binding_for_pane(host, leaf->id);
        const CorePaneModuleDescriptor* descriptor =
            line_drawing_authoring_descriptor_for_binding(host, binding);
        rows[count].pane_rect = leaf->rect;
        rows[count].pane_id = leaf->id;
        rows[count].module_key = descriptor ? descriptor->module_key : "unbound";
        rows[count].module_label = descriptor ? descriptor->display_name : "Unbound";
        ++count;
    }
    return count;
}

static void line_drawing_authoring_draw_text(SDL_Renderer* renderer,
                                             const char* text,
                                             int x,
                                             int y,
                                             SDL_Color color) {
    TTF_Font* font = FontManager_GetUIPanelFont();
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void line_drawing_authoring_draw_button(SDL_Renderer* renderer,
                                               const KitWorkspaceAuthoringOverlayButton* button,
                                               SDL_Color fill,
                                               SDL_Color border,
                                               SDL_Color text) {
    SDL_Rect rect;
    int text_w = 0;
    int text_h = 0;
    TTF_Font* font = FontManager_GetUIPanelFont();
    if (!renderer || !button || !button->visible) return;

    rect = line_drawing_authoring_sdl_rect(button->rect);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    (void)SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    (void)SDL_RenderDrawRect(renderer, &rect);

    if (font && button->label && line_drawing_text_measure_utf8(renderer,
                                                                font,
                                                                button->label,
                                                                &text_w,
                                                                &text_h)) {
        int tx = rect.x + (rect.w - text_w) / 2;
        int ty = rect.y + (rect.h - text_h) / 2;
        if (tx < rect.x + 4) tx = rect.x + 4;
        if (ty < rect.y + 1) ty = rect.y + 1;
        line_drawing_authoring_draw_text(renderer, button->label, tx, ty, text);
    }
}

static void line_drawing_authoring_draw_controls(SDL_Renderer* renderer,
                                                 const GlobalState* state,
                                                 const LineDrawing3dThemePalette* palette) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = 0u;
    uint32_t i;
    SDL_Color fill = line_drawing_authoring_color_alpha(palette->button_fill, 230);
    SDL_Color border = line_drawing_authoring_color_alpha(palette->button_border, 240);
    SDL_Color text = line_drawing_authoring_color_alpha(palette->text_primary, 255);

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        state->screenWidth,
        LineDrawingWorkspaceAuthoringHost_Active(state),
        LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(state),
        buttons,
        4u);
    for (i = 0u; i < count; ++i) {
        line_drawing_authoring_draw_button(renderer, &buttons[i], fill, border, text);
    }
}

static void line_drawing_authoring_draw_pane_rows(SDL_Renderer* renderer,
                                                  const GlobalState* state,
                                                  const LineDrawing3dThemePalette* palette) {
    LineDrawingAuthoringPaneRow rows[LINE_DRAWING_AUTHORING_PANE_ROW_CAP];
    uint32_t count = 0u;
    uint32_t i;
    SDL_Color pane_border = line_drawing_authoring_color_alpha(palette->button_border, 220);
    SDL_Color label_fill = line_drawing_authoring_color_alpha(palette->panel_fill, 220);
    SDL_Color text = line_drawing_authoring_color_alpha(palette->text_primary, 245);
    SDL_Color muted = line_drawing_authoring_color_alpha(palette->text_muted, 235);

    count = line_drawing_authoring_build_rows(state, rows, LINE_DRAWING_AUTHORING_PANE_ROW_CAP);
    for (i = 0u; i < count; ++i) {
        SDL_Rect pane_rect = line_drawing_authoring_sdl_rect(rows[i].pane_rect);
        SDL_Rect label_rect;
        char label[160];
        (void)snprintf(label,
                       sizeof(label),
                       "P%u %s",
                       rows[i].pane_id,
                       rows[i].module_label ? rows[i].module_label : "Unbound");
        SDL_SetRenderDrawColor(renderer, pane_border.r, pane_border.g, pane_border.b, pane_border.a);
        (void)SDL_RenderDrawRect(renderer, &pane_rect);
        label_rect = (SDL_Rect){ pane_rect.x + 8, pane_rect.y + 8, 220, 22 };
        if (label_rect.x + label_rect.w > pane_rect.x + pane_rect.w - 8) {
            label_rect.w = (pane_rect.x + pane_rect.w - 8) - label_rect.x;
        }
        if (label_rect.w < 72) {
            label_rect.w = pane_rect.w - 16;
        }
        if (label_rect.w > 0) {
            SDL_SetRenderDrawColor(renderer, label_fill.r, label_fill.g, label_fill.b, label_fill.a);
            (void)SDL_RenderFillRect(renderer, &label_rect);
            SDL_SetRenderDrawColor(renderer, pane_border.r, pane_border.g, pane_border.b, pane_border.a);
            (void)SDL_RenderDrawRect(renderer, &label_rect);
            line_drawing_authoring_draw_text(renderer, label, label_rect.x + 6, label_rect.y + 4, text);
        }
        if (pane_rect.w > 260 && pane_rect.h > 90) {
            line_drawing_authoring_draw_text(renderer,
                                             rows[i].module_key ? rows[i].module_key : "unbound",
                                             pane_rect.x + 14,
                                             pane_rect.y + 38,
                                             muted);
        }
    }
}

void LineDrawingWorkspaceAuthoringOverlay_Draw(SDL_Renderer* renderer, const GlobalState* state) {
    LineDrawing3dThemePalette palette = {0};
    if (!renderer || !state || !LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(state)) {
        return;
    }
    if (!line_drawing3d_shared_theme_resolve_palette(&palette)) {
        palette.panel_fill = (SDL_Color){ 18, 22, 30, 255 };
        palette.button_fill = (SDL_Color){ 28, 34, 46, 255 };
        palette.button_border = (SDL_Color){ 120, 160, 220, 255 };
        palette.text_primary = (SDL_Color){ 235, 240, 248, 255 };
        palette.text_muted = (SDL_Color){ 154, 164, 180, 255 };
    }

    line_drawing_authoring_draw_pane_rows(renderer, state, &palette);
    line_drawing_authoring_draw_controls(renderer, state, &palette);
}
