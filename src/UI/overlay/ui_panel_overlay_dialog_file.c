#include "UI/overlay/ui_panel_overlay_render_internal.h"

#include "UI/ui_panel_internal.h"
#include "UI/font_manager.h"
#include "UI/info_overlay.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"

#include <stdio.h>
#include <string.h>

static void RenderSaveDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    if (!ui->saveDialog.active) return;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    int width = Global_GetScreenWidth();
    int height = Global_GetScreenHeight();

    SDL_Rect backdrop = { 0, 0, width, height };
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    SDL_Rect panel = {
        width / 2 - 220,
        InfoOverlay_HeightPx() + 20,
        440,
        130
    };

    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    int text_x = panel.x + 16;
    int text_y = panel.y + 16;
    char line[256];

    snprintf(line, sizeof(line), "Save layout as (*.json):");
    UIPanelOverlay_DrawText(renderer,
                            line,
                            text_x,
                            text_y,
                            has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});

    SDL_Rect input_rect = {
        panel.x + 14,
        panel.y + 48,
        panel.w - 28,
        32
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &input_rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &input_rect);

    char buffer[200];
    snprintf(buffer, sizeof(buffer), "%s", ui->saveDialog.buffer);
    UIPanelOverlay_DrawTextClipped(renderer,
                                   buffer,
                                   input_rect.x + 8,
                                   input_rect.y + 6,
                                   input_rect.w - 16,
                                   has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        char caret_buf[128];
        size_t len = ui->saveDialog.cursor;
        if (len > sizeof(caret_buf) - 1u) len = sizeof(caret_buf) - 1u;
        memcpy(caret_buf, ui->saveDialog.buffer, len);
        caret_buf[len] = '\0';
        int caret_offset = 0;
        (void)line_drawing_text_measure_utf8(renderer, font, caret_buf, &caret_offset, NULL);
        int caret_x = input_rect.x + 8 + caret_offset;
        if (caret_x > input_rect.x + input_rect.w - 8) caret_x = input_rect.x + input_rect.w - 8;
        int caret_top = input_rect.y + 4;
        int caret_bottom = input_rect.y + input_rect.h - 4;
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.text_primary.r, palette.text_primary.g,
                                   palette.text_primary.b, 220);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        }
        SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
    }

    snprintf(line, sizeof(line), "Press Enter to confirm, Esc to cancel.");
    UIPanelOverlay_DrawText(renderer,
                            line,
                            text_x,
                            panel.y + panel.h - 36,
                            has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});
}

static void RenderRootDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const char* title = NULL;
    const char* description = "Enter to apply, Esc to cancel.";
    int width = 0;
    int height = 0;
    int text_x = 0;
    int text_y = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect input_rect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caret_buf[256];
    int caret_offset = 0;
    int caret_x = 0;
    int caret_top = 0;
    int caret_bottom = 0;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    if (!ui->rootDialog.active) return;

    switch (ui->rootDialog.target) {
        case UI_ROOT_TARGET_INPUT:
            title = "Set Session Input Root";
            break;
        case UI_ROOT_TARGET_OUTPUT:
            title = "Set Output Root";
            break;
        case UI_ROOT_TARGET_OBJECT_ASSET:
            title = "Set Object Asset Root";
            break;
        default:
            title = "Set Root";
            break;
    }

    width = Global_GetScreenWidth();
    height = Global_GetScreenHeight();
    backdrop = (SDL_Rect){0, 0, width, height};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    panel = (SDL_Rect){
        width / 2 - 300,
        InfoOverlay_HeightPx() + 20,
        600,
        154
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    text_x = panel.x + 16;
    text_y = panel.y + 14;
    UIPanelOverlay_DrawText(renderer,
                            title,
                            text_x,
                            text_y,
                            has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    UIPanelOverlay_DrawText(renderer,
                            description,
                            text_x,
                            text_y + 22,
                            has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    input_rect = (SDL_Rect){
        panel.x + 14,
        panel.y + 58,
        panel.w - 28,
        34
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &input_rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &input_rect);
    UIPanelOverlay_DrawTextClipped(renderer,
                                   ui->rootDialog.buffer,
                                   input_rect.x + 8,
                                   input_rect.y + 8,
                                   input_rect.w - 16,
                                   has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->rootDialog.cursor;
        if (len >= sizeof(caret_buf)) len = sizeof(caret_buf) - 1u;
        memcpy(caret_buf, ui->rootDialog.buffer, len);
        caret_buf[len] = '\0';
        if (TTF_SizeUTF8(font, caret_buf, &caret_offset, NULL) == 0) {
            caret_x = input_rect.x + 8 + caret_offset;
            if (caret_x > input_rect.x + input_rect.w - 8) caret_x = input_rect.x + input_rect.w - 8;
            caret_top = input_rect.y + 4;
            caret_bottom = input_rect.y + input_rect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
        }
    }
}

void UIPanelOverlay_RenderFileDialogs(SDL_Renderer* renderer, const UIPanelState* ui) {
    RenderSaveDialog(renderer, ui);
    RenderRootDialog(renderer, ui);
}
