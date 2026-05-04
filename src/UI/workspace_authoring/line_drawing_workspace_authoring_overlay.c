#include "UI/workspace_authoring/line_drawing_workspace_authoring_overlay.h"

#include <stdio.h>
#include <string.h>

#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include "core_font.h"
#include "core_pane_module.h"
#include "core_theme.h"
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

static SDL_Rect line_drawing_authoring_kit_rect(KitRenderRect rect) {
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

static void line_drawing_authoring_draw_sdl_button(SDL_Renderer* renderer,
                                                   SDL_Rect rect,
                                                   const char* label,
                                                   int active,
                                                   int enabled,
                                                   const LineDrawing3dThemePalette* palette) {
    SDL_Color fill = active ? palette->menu_highlight : palette->button_fill;
    SDL_Color border = palette->button_border;
    SDL_Color text = enabled ? palette->text_primary : palette->text_muted;
    int text_w = 0;
    int text_h = 0;
    TTF_Font* font = FontManager_GetUIPanelFont();

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, enabled ? 235u : 120u);
    (void)SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, enabled ? 238u : 130u);
    (void)SDL_RenderDrawRect(renderer, &rect);
    if (font && label && line_drawing_text_measure_utf8(renderer, font, label, &text_w, &text_h)) {
        int tx = rect.x + (rect.w - text_w) / 2;
        int ty = rect.y + (rect.h - text_h) / 2;
        if (tx < rect.x + 6) tx = rect.x + 6;
        if (ty < rect.y + 2) ty = rect.y + 2;
        line_drawing_authoring_draw_text(renderer, label, tx, ty, text);
    }
}

static void line_drawing_authoring_draw_section(SDL_Renderer* renderer,
                                                SDL_Rect rect,
                                                const char* title,
                                                const char* detail,
                                                const LineDrawing3dThemePalette* palette) {
    SDL_SetRenderDrawColor(renderer,
                           palette->panel_fill.r,
                           palette->panel_fill.g,
                           palette->panel_fill.b,
                           238u);
    (void)SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette->panel_border.r,
                           palette->panel_border.g,
                           palette->panel_border.b,
                           245u);
    (void)SDL_RenderDrawRect(renderer, &rect);
    line_drawing_authoring_draw_text(renderer, title, rect.x + 14, rect.y + 12, palette->text_primary);
    if (detail && detail[0]) {
        line_drawing_authoring_draw_text(renderer, detail, rect.x + 14, rect.y + 36, palette->text_muted);
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

static KitWorkspaceAuthoringFontThemeButtonId line_drawing_authoring_font_button_id_at(uint32_t index) {
    switch (index) {
        case 0u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT;
        case 1u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_IDE;
        case 2u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_CUSTOM_STUB;
        default:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
    }
}

static KitWorkspaceAuthoringFontThemeButtonId line_drawing_authoring_theme_button_id_at(uint32_t index) {
    switch (index) {
        case 0u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT;
        case 1u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_STANDARD_GREY;
        case 2u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_MIDNIGHT_CONTRAST;
        case 3u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_SOFT_LIGHT;
        case 4u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_GREYSCALE;
        default:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
    }
}

static KitWorkspaceAuthoringFontThemeButtonId line_drawing_authoring_custom_button_id_at(uint32_t index) {
    switch (index) {
        case 0u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB;
        case 1u:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_EDIT_STUB;
        default:
            return KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
    }
}

static void line_drawing_authoring_draw_font_theme_overlay(
    SDL_Renderer* renderer,
    const GlobalState* state,
    const LineDrawing3dThemePalette* palette) {
    KitWorkspaceAuthoringFontThemeLayout layout = {0};
    SDL_Rect screen_rect;
    SDL_Rect panel_rect;
    SDL_Rect section_rect;
    SDL_Rect value_rect;
    char line[192];
    char theme_name[64] = "unknown";
    const char* font_name = FontManager_GetSharedFontPresetName();
    int zoom_step = FontManager_GetZoomStep();
    uint32_t i;

    if (!kit_workspace_authoring_ui_font_theme_build_layout(NULL,
                                                            state->screenWidth,
                                                            state->screenHeight,
                                                            &layout)) {
        return;
    }
    (void)line_drawing3d_shared_theme_current_preset(theme_name, sizeof(theme_name));
    if (!font_name || !font_name[0]) {
        font_name = "unknown";
    }

    screen_rect = (SDL_Rect){0, 0, state->screenWidth, state->screenHeight};
    panel_rect = line_drawing_authoring_kit_rect(layout.panel);
    SDL_SetRenderDrawColor(renderer,
                           palette->background_fill.r,
                           palette->background_fill.g,
                           palette->background_fill.b,
                           255u);
    (void)SDL_RenderFillRect(renderer, &screen_rect);
    SDL_SetRenderDrawColor(renderer,
                           palette->panel_fill.r,
                           palette->panel_fill.g,
                           palette->panel_fill.b,
                           246u);
    (void)SDL_RenderFillRect(renderer, &panel_rect);
    SDL_SetRenderDrawColor(renderer,
                           palette->panel_border.r,
                           palette->panel_border.g,
                           palette->panel_border.b,
                           255u);
    (void)SDL_RenderDrawRect(renderer, &panel_rect);
    line_drawing_authoring_draw_text(renderer,
                                     "Font/Theme Overlay",
                                     panel_rect.x + 14,
                                     panel_rect.y + 14,
                                     palette->text_primary);

    section_rect = line_drawing_authoring_kit_rect(layout.font_preset_section);
    snprintf(line, sizeof(line), "Current font preset: %s", font_name);
    line_drawing_authoring_draw_section(renderer, section_rect, "Font Preset", line, palette);
    for (i = 0u; i < layout.font_preset_button_count; ++i) {
        CoreFontPresetId preset_id = CORE_FONT_PRESET_IDE;
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            line_drawing_authoring_font_button_id_at(i);
        int active = 0;
        int enabled = (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id);
        if (kit_workspace_authoring_ui_font_theme_button_font_preset_id(button_id, &preset_id)) {
            active = preset_id == FontManager_GetSharedFontPresetId();
        }
        line_drawing_authoring_draw_sdl_button(
            renderer,
            line_drawing_authoring_kit_rect(layout.font_preset_buttons[i]),
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            active,
            enabled,
            palette);
    }

    section_rect = line_drawing_authoring_kit_rect(layout.text_size_section);
    snprintf(line, sizeof(line), "Text Size step:%+d (%d%%)", zoom_step, 100 + (zoom_step * 10));
    line_drawing_authoring_draw_section(renderer, section_rect, "Text Size", line, palette);
    line_drawing_authoring_draw_text(renderer,
                                     "Sample sCulpt UI text 123",
                                     section_rect.x + 14,
                                     section_rect.y + 62,
                                     palette->text_primary);
    line_drawing_authoring_draw_sdl_button(
        renderer,
        line_drawing_authoring_kit_rect(layout.text_size_dec_button),
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC),
        0,
        1,
        palette);
    line_drawing_authoring_draw_sdl_button(
        renderer,
        line_drawing_authoring_kit_rect(layout.text_size_inc_button),
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC),
        0,
        1,
        palette);
    value_rect = line_drawing_authoring_kit_rect(layout.text_size_value_chip);
    snprintf(line, sizeof(line), "%+d", zoom_step);
    line_drawing_authoring_draw_sdl_button(renderer, value_rect, line, 1, 1, palette);
    line_drawing_authoring_draw_sdl_button(
        renderer,
        line_drawing_authoring_kit_rect(layout.text_size_reset_button),
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET),
        zoom_step == 0,
        1,
        palette);

    section_rect = line_drawing_authoring_kit_rect(layout.theme_preset_section);
    snprintf(line, sizeof(line), "Current theme preset: %s", theme_name);
    line_drawing_authoring_draw_section(renderer, section_rect, "Theme Preset", line, palette);
    for (i = 0u; i < layout.theme_preset_button_count; ++i) {
        CoreThemePresetId preset_id = CORE_THEME_PRESET_DARK_DEFAULT;
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            line_drawing_authoring_theme_button_id_at(i);
        const char* preset_name = NULL;
        int active = 0;
        int enabled = (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id);
        if (kit_workspace_authoring_ui_font_theme_button_theme_preset_id(button_id, &preset_id)) {
            preset_name = core_theme_preset_name(preset_id);
            active = preset_name && strcmp(preset_name, theme_name) == 0;
        }
        line_drawing_authoring_draw_sdl_button(
            renderer,
            line_drawing_authoring_kit_rect(layout.theme_preset_buttons[i]),
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            active,
            enabled,
            palette);
    }

    section_rect = line_drawing_authoring_kit_rect(layout.custom_theme_section);
    line_drawing_authoring_draw_section(
        renderer,
        section_rect,
        "Custom Theme Slots",
        state->workspaceAuthoring.font_theme_status[0]
            ? state->workspaceAuthoring.font_theme_status
            : "Custom preset creation/editing is reserved for a later line_drawing theme lane.",
        palette);
    for (i = 0u; i < layout.custom_theme_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            line_drawing_authoring_custom_button_id_at(i);
        line_drawing_authoring_draw_sdl_button(
            renderer,
            line_drawing_authoring_kit_rect(layout.custom_theme_buttons[i]),
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            0,
            (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id),
            palette);
    }

    line_drawing_authoring_draw_text(renderer,
                                     "Tab returns to pane authoring | Enter applies | Esc cancels",
                                     panel_rect.x + 14,
                                     panel_rect.y + panel_rect.h - 26,
                                     palette->text_muted);
}

void LineDrawingWorkspaceAuthoringOverlay_Draw(SDL_Renderer* renderer, const GlobalState* state) {
    LineDrawing3dThemePalette palette = {0};
    if (!renderer || !state || !LineDrawingWorkspaceAuthoringHost_Active(state)) {
        return;
    }
    if (!line_drawing3d_shared_theme_resolve_palette(&palette)) {
        palette.panel_fill = (SDL_Color){ 18, 22, 30, 255 };
        palette.button_fill = (SDL_Color){ 28, 34, 46, 255 };
        palette.button_border = (SDL_Color){ 120, 160, 220, 255 };
        palette.text_primary = (SDL_Color){ 235, 240, 248, 255 };
        palette.text_muted = (SDL_Color){ 154, 164, 180, 255 };
    }

    if (LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(state)) {
        line_drawing_authoring_draw_font_theme_overlay(renderer, state, &palette);
    } else if (LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(state)) {
        line_drawing_authoring_draw_pane_rows(renderer, state, &palette);
    }
    line_drawing_authoring_draw_controls(renderer, state, &palette);
}
